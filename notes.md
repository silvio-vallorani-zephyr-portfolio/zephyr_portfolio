## Attaccare il JLink al conteiner per usarlo da dentro

- https://www.xda-developers.com/wsl-connect-usb-devices-windows-11/
- https://learn.microsoft.com/en-us/windows/wsl/connect-usb#attach-a-usb-device

## Install JLink tools in docker
apt update
apt upgrade -y
apt install -y wget

wget https://www.segger.com/downloads/jlink/SystemView_Linux_V356a_x86_64.deb
dpkg -i SystemView_Linux_V356a_x86_64.deb

wget https://www.segger.com/downloads/jlink/Ozone_Linux_V336_x86_64.deb
dpkg -i Ozone_Linux_V336_x86_64.deb

apt -y install --no-install-recommends fontconfig-config fonts-dejavu-core libbsd0 libfontconfig1 libfreetype6 libglib2.0-0 libglib2.0-data libice6 libpng16-16 libsm6 libx11-6 libx11-data libx11-xcb1 libxau6 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-randr0 libxcb-render-util0 libxcb-render0 libxcb-shape0 libxcb-shm0 libxcb-sync1 libxcb-util1 libxcb-xfixes0 libxcb-xkb1 libxcb1 libxdmcp6 libxext6 libxkbcommon-x11-0 libxkbcommon0 libxrender1 shared-mime-info x11-common xdg-user-dirs xkb-data libxrandr2 libxfixes3 libxcursor1
dpkg --unpack JLink_Linux_x86_64.deb
rm -f /var/lib/dpkg/info/jlink.postinst
dpkg --configure jlink
apt install -yf
