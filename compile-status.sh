#!/bin/bash
# copy to and run from /root

[[ ! -e /usr/bin/pkg-config ]] && pacman -Sy --noconfirm pkgconfig

bar="\n\e[44m  \e[0m"

echo -e "$bar Update source files"

cd rAudio-status
if [[ -e /boot/kernel.img ]]; then
	arch=armv6h
	cp -f /home/x/BIG/RPi/Git/rAudio-status/* . 2> /dev/null
elif [[ -e /boot/kernel7.img ]]; then
	arch=armv7h
	dir=/mnt/MPD/NAS/RPi
	mkdir -p $dir
	mount -t cifs //192.168.1.9/RPi $dir -o username=guest,password=
	cp -f $dir/Git/rAudio-status/* . 2> /dev/null
fi

./compile.sh

echo -e "$bar Copy status.armv7h to rAudio/srv/http/bash\n"

cp -f status.$arch $dir/Git/rAudio/srv/http/bash
if [[ $arch == armv7h ]]; then
	umount -l $dir
	rmdir $dir
fi
