#!/bin/bash

bar="\n\e[44m  \e[0m"
echo -e "$bar Update source files"

cd rAudio-status
cp -f /home/x/BIG/RPi/Git/rAudio-status/* . 2> /dev/null
./compile.sh

echo -e "$bar Copy status.armv6h to rAudio/srv/http/bash\n"

cp -f status.armv6h /home/x/BIG/RPi/Git/rAudio/srv/http/bash
