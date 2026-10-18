#pragma once

#include <alsa/asoundlib.h>

// Replicates the cubic-root volume scale algorithm used by `amixer -M`.
inline int mapPercent(long current_db, long min_db, long max_db) {
    if (max_db <= min_db) return 0;

    // Linear mapping for narrow ranges (<= 24 dB span)
    if (max_db - min_db <= 2400) {
        double ratio = static_cast<double>(current_db - min_db) / (max_db - min_db);
        return static_cast<int>(std::round(ratio * 100.0));
    }

    // Logarithmic (perceptual) mapping for wider ranges
    double normalized = std::pow(10.0, static_cast<double>(current_db - max_db) / 6000.0);
    double min_normalized = std::pow(10.0, static_cast<double>(min_db - max_db) / 6000.0);
    if (1.0 - min_normalized != 0.0) {
        normalized = (normalized - min_normalized) / (1.0 - min_normalized);
    }

    int pct = static_cast<int>(std::round(normalized * 100.0));
    return (pct < 0) ? 0 : (pct > 100) ? 100 : pct;
}

// Optimized: direct lookup using the known mixer control name.
// device defaults to ALSA's "default" ctl (see /etc/asound.conf) when omitted.
inline int getVolume(const std::string& CONTROL, const std::string& device = "default") {
    int percent = 0;
    snd_mixer_t *handle = nullptr;
    snd_mixer_selem_id_t *sid = nullptr;

    if (snd_mixer_open(&handle, 0) < 0) return 0;
    if (snd_mixer_attach(handle, device.c_str()) < 0 ||
        snd_mixer_selem_register(handle, nullptr, nullptr) < 0 ||
        snd_mixer_load(handle) < 0) {
        snd_mixer_close(handle);
        return 0;
    }

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, CONTROL.c_str());
    snd_mixer_elem_t *elem = snd_mixer_find_selem(handle, sid);

    if (elem) {
        long min_db = 0, max_db = 0, cur_db = 0;
        bool ok = false;

        if (snd_mixer_selem_has_playback_volume(elem)) {
            ok = snd_mixer_selem_get_playback_dB_range(elem, &min_db, &max_db) >= 0 &&
                 snd_mixer_selem_get_playback_dB(elem, SND_MIXER_SCHN_FRONT_LEFT, &cur_db) >= 0;
        } else if (snd_mixer_selem_has_capture_volume(elem)) {
            ok = snd_mixer_selem_get_capture_dB_range(elem, &min_db, &max_db) >= 0 &&
                 snd_mixer_selem_get_capture_dB(elem, SND_MIXER_SCHN_FRONT_LEFT, &cur_db) >= 0;
        }
        if (ok) percent = mapPercent(cur_db, min_db, max_db);
    }

    snd_mixer_close(handle);
    return percent;
}
