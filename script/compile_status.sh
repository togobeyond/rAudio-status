#!/bin/bash

[[ ! -e /usr/bin/pkg-config ]] && pacman -Sy --noconfirm pkgconfig

bar="\n\e[44m  \e[0m"
dir=/mnt/MPD/NAS/RPi

mkdir -p $dir
mount -t cifs //192.168.1.9/RPi $dir -o username=guest,password=
cp -f $dir/Git/rAudio-status/* /root/rAudio-status/ 2>/dev/null
cd rAudio-status

echo -e "$bar Source files copied."

SECONDS=0

opt='
-Wno-psabi
'
opt="
$( [[ ! $1 ]] && echo -O2 )
$opt
_status.cpp
-o $file_bin
$( pkg-config --cflags --libs alsa dbus-1 libcurl libmpdclient libsystemd libupnpp taglib )
"
echo 
echo g++ $opt
echo ...

g++ $opt

[[ $? != 0 ]] && exit

echo Compile duration: $SECONDS seconds

cp -f status /srv/http/bash
mv status status.armv7h
cp -f status $dir/Git/rAudio-status/status.armv7h
umount -l $dir
rmdir $dir

echo -e "$bar status.armv7h copied to ../RPi/rAudio/"
