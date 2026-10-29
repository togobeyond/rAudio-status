#pragma once

#include <systemd/sd-bus.h>

/*
org.gnome.ShairportSync.RemoteControl:
    - PlayerState    : Playing / Paused / Stopped / Not Available
    - ProgressString : start/current/end (@sample rate)
    - Metadata       : a{sv} (xesam:album, xesam:artist, xesam:title,
                       mpris:artUrl, mpris:length)

org.gnome.ShairportSync:
    - SourceFormat   : AAC/48000/F24/2

org.mpris.MediaPlayer2.Player (name org.mpris.MediaPlayer2.ShairportSync):
    - Metadata       : same dict as RemoteControl.Metadata (fallback)
*/
static bool shairportPropValid(const std::string& v) {
    return !v.empty() && v != "Unknown" && v != "Not Available";
}

std::string get_prop(sd_bus* bus
                    , const char* prop
                    , const char* inf = "org.gnome.ShairportSync.RemoteControl") {
    sd_bus_message* m = nullptr;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    char* val = nullptr;

    int r = sd_bus_get_property_string(bus,
                                       "org.gnome.ShairportSync",
                                       "/org/gnome/ShairportSync",
                                       inf,
                                       prop,
                                       &error,
                                       &val);

    std::string result = (r >= 0 && val) ? val : "Unknown";
    free(val);
    sd_bus_error_free(&error);
    return result;
}

static bool shairportParseProgress(const std::string& s
                                  , long long& start
                                  , long long& current
                                  , long long& end) {
    if (!shairportPropValid(s)) return false;
    size_t p0 = s.find('/');
    if (p0 == std::string::npos) return false;
    size_t p1 = s.find('/', p0 + 1);
    if (p1 == std::string::npos) return false;
    try {
        start   = std::stoll(s.substr(0, p0));
        current = std::stoll(s.substr(p0 + 1, p1 - p0 - 1));
        end     = std::stoll(s.substr(p1 + 1));
    } catch (...) {
        return false;
    }
    return true;
}

static void shairportParseFormat(const std::string& format
                                , std::string& ext
                                , int& bitdepth
                                , int& samplerate) {
    ext        = "AirPlay";
    bitdepth   = 16;
    samplerate = 44100;
    if (!shairportPropValid(format)) return;

    size_t p0 = format.find('/');            // AAC/48000/F24/2
    size_t p1 = format.find('/', p0 + 1);
    if (p0 == std::string::npos || p1 == std::string::npos) return;
    try {
        ext        = format.substr(0, p0);
        samplerate = std::stoi(format.substr(p0 + 1, p1 - p0 - 1));
    } catch (...) {
        ext        = "AirPlay";
        samplerate = 44100;
        return;
    }
    size_t pf = format.find('F', p1);        // F24
    if (pf != std::string::npos) {
        try {
            bitdepth = std::stoi(format.substr(pf + 1, 2));
        } catch (...) {
            bitdepth = 16;
        }
    }
    if (samplerate <= 0) samplerate = 44100;
}

void getMetadata(sd_bus* bus) {
    sd_bus_message* m = nullptr;
    sd_bus_error error = SD_BUS_ERROR_NULL;

    // native interface first - Metadata lives on org.gnome.ShairportSync.RemoteControl
    int r = sd_bus_get_property(bus,
                                "org.gnome.ShairportSync",
                                "/org/gnome/ShairportSync",
                                "org.gnome.ShairportSync.RemoteControl",
                                "Metadata",
                                &error,
                                &m,
                                "a{sv}");
    if (r < 0) {
        sd_bus_error_free(&error);
        error = SD_BUS_ERROR_NULL;
        r = sd_bus_get_property(bus,
                                "org.mpris.MediaPlayer2.ShairportSync",
                                "/org/mpris/MediaPlayer2",
                                "org.mpris.MediaPlayer2.Player",
                                "Metadata",
                                &error,
                                &m,
                                "a{sv}");
    }
    if (r < 0) {
        sd_bus_error_free(&error);
        return;
    }

    if (sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "{sv}") < 0) {
        sd_bus_message_unref(m);
        return;
    }
    const char* key;
    while (sd_bus_message_enter_container(m, SD_BUS_TYPE_DICT_ENTRY, "sv") > 0) {
        if (sd_bus_message_read(m, "s", &key) < 0) {
            sd_bus_message_exit_container(m);
            break;
        }
        std::string s_key(key);

        char type;
        const char* contents = nullptr;
        std::string sig;
        if (sd_bus_message_peek_type(m, &type, &contents) >= 0 && contents)
            sig = contents;

        if (s_key == "xesam:album" && sig == "s") {
            const char* album = nullptr;
            if (sd_bus_message_read(m, "v", "s", &album) >= 0 && album)
                S["Album"] = album;

        } else if (s_key == "xesam:artist" && sig == "as") {
            if (sd_bus_message_enter_container(m, SD_BUS_TYPE_VARIANT, "as") >= 0 &&
                sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "s") >= 0) {
                const char* artist = nullptr;
                std::string artists;
                while (sd_bus_message_read(m, "s", &artist) > 0) {
                    if (!artist) continue;
                    if (!artists.empty()) artists += ", ";
                    artists += artist;
                }
                S["Artist"] = artists;
                sd_bus_message_exit_container(m); // array
                sd_bus_message_exit_container(m); // variant
            }

        } else if (s_key == "xesam:title" && sig == "s") {
            const char* title = nullptr;
            if (sd_bus_message_read(m, "v", "s", &title) >= 0 && title)
                S["Title"] = title;

        } else if (s_key == "mpris:artUrl" && sig == "s") {
            const char* arturl = nullptr;
            if (sd_bus_message_read(m, "v", "s", &arturl) >= 0 && arturl) {
                std::string file = arturl;
                if (file.starts_with("file://")) file.erase(0, 7);
                if (!file.empty() && fs::exists(file)) {
                    fs::path source = file;
                    fs::path target = DIR.SHM +"coverart";
                    std::error_code ec;
                    fs::path source_current = fs::read_symlink(target, ec);
                    if (ec || source_current != source) { // ec - not exists
                        fs::remove(target, ec);
                        fs::create_symlink(source, target);
                    }
                    V.COVERART = "/data/shm/coverart";
                }
            }

        } else if (s_key == "mpris:length" && sig == "x") {
            int64_t time = 0;
            if (sd_bus_message_read(m, "v", "x", &time) >= 0 && time > 0)
                V.TIME = (time + 500000) / 1000000; // us > s

        } else if (s_key == "mpris:length" && sig == "t") {
            uint64_t time = 0;
            if (sd_bus_message_read(m, "v", "t", &time) >= 0 && time > 0)
                V.TIME = (time + 500000) / 1000000; // us > s

        } else {
            sd_bus_message_skip(m, "v");
        }
        sd_bus_message_exit_container(m);
    }
    sd_bus_message_exit_container(m);
    sd_bus_message_unref(m);
    sd_bus_error_free(&error);
}

void shairportMeta(sd_bus* bus) {
    std::string state = get_prop(bus, "PlayerState");
    if (!shairportPropValid(state)) { // no session - stop, not pause
        V.STATE = "stop";
        if (S["Title"].empty()) S["Title"] = "AirPlay";
        return;
    }

    int elapsed = 0;
    int samplerate = 44100;
    if (state == "Paused") {
        V.STATE = "pause";
        try {
            elapsed = std::stoi(fileContent(DIR.SHM +"elapsed"));
        } catch (...) {
            elapsed = 0;
        }
    } else if (state == "Playing") {
        V.STATE = "play";
        try {
            V.TIMESTAMP = std::stoll(fileContent(DIR.SHM +"timestamp"));
        } catch (...) {
            V.TIMESTAMP = 0;
        }
    } else { // Stopped
        V.STATE = "stop";
    }

    getMetadata(bus); // Album, Artist, Title, V.COVERART, V.TIME
    if (S["Title"].empty()) S["Title"] = "AirPlay";

    shairportParseFormat(get_prop(bus, "SourceFormat", "org.gnome.ShairportSync")
                        , V.EXT, V.BITDEPTH, samplerate);
    V.SAMPLERATE = samplerate;

    long long start = 0, current = 0, end = 0;
    if (shairportParseProgress(get_prop(bus, "ProgressString"), start, current, end)) {
        if (V.TIME <= 0 && end > start) // fallback when mpris:length is missing
            V.TIME = (end - start) / (V.SAMPLERATE > 0 ? V.SAMPLERATE : 44100);
    }

    if (current > start && V.SAMPLERATE > 0)
        V.ELAPSED = (current - start) / V.SAMPLERATE + elapsed;
    else
        V.ELAPSED = elapsed;
}
