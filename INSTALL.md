# Oikomaticz

Usage:
```
Oikomaticz [-www <port>]  [-verbose <0|1>]
          -www <port>   Default is: -www 8080
          -verbose <0|1> (0 is none, 1 is debug)   Default is: -verbose 0
```

Examples:
```
Oikomaticz            (this is the same as Oikomaticz -www 8080 -verbose 0)
Oikomaticz -www 81 -verbose 1
```

If Oikomaticz and the browser are running on the same system you can connect with http://localhost:8080/
To stop the application: press Ctrl-C in the application screen (not in the browser)

Compatible browsers:
* Chrome/Firefox/Safari...
* Internet Explorer version 10+

Be aware that a Raspberry pi receives its time from an online ntp server.
If the pi is not connected to a network, the device time will not be updated, resulting in scheduling issues. 

All ports below 1024 on linux systems can only be started by root.
If you run oikomaticz on port 80, make sure to run it as root, e.g. : sudo ./oikomaticz

Compiling from source code:
---------------------------

First get get all prerequisites for your operation system. See the section below.

Then get the source code:
```
git clone https://github.com/gordonb3/oikomaticz.git
cd oikomaticz

cmake -DCMAKE_BUILD_TYPE=Release CMakeLists.txt
make
```

Now you should have the binary application, you can start it with
`./oikomaticz`

For additional parameters type:
`./oikomaticz -h`

Note: Compiling on the Raspberry Pi will take about 15 minutes

To Update to a newer version:
- stop the application (control-c), or stop the startup script (see below) with `/etc/init.d/oikomaticz.sh stop`

(from the oikomaticz folder)
```
git pull
cmake CMakeLists.txt
make
```

Unix startup script:
To start Oikomaticz automatically when the system starts perform the following steps:
```
sudo cp oikomaticz.sh /etc/init.d
sudo chmod +x /etc/init.d/oikomaticz.sh
sudo update-rc.d oikomaticz.sh defaults
```

Edit the startup script and point the DEAMON location to point to the installation folder:
```
sudo vi /etc/init.d/oikomaticz.sh

DAEMON=/home/pi/oikomaticz/oikomaticz
```

If you want to use another web interface port change:
`OPTIONS="-www 8080"`

You can now start oikomaticz with:
`sudo /etc/init.d/oikomaticz.sh start`

To stop:
`sudo /etc/init.d/oikomaticz.sh stop`

To check if oikomaticz is running:
`sudo /etc/init.d/oikomaticz.sh status`

If your system supports it you can also do
```
sudo service oikomaticz.sh start
sudo service oikomaticz.sh stop
sudo service oikomaticz.sh status
```

To update when you have installed it as startup service:
```
cd /home/pi/oikomaticz # (or where you installed oikomaticz)
sudo service oikomaticz.sh stop
git pull
make
sudo service oikomaticz.sh start
```

Option: Create an update and backup script
`chmod +x updatedomo`

To update oikomaticz

* login to your Raspberry Pi
```
cd oikomaticz
./updatedomo
```


## Prerequisites

All: (Assuming oikomaticz development is in a subfolder of the user)

`cd ~`

### OpenZWave support
If you need support for Open-ZWave (for example if you want to use an Aeon USB V2 zwave adapter),
you need to compile open zwave

- Compile OpenZWave (https://github.com/OpenZWave/open-zwave)
(On non-darwin install libudev-dev (`sudo apt-get install libudev-dev`)
```
git clone https://github.com/OpenZWave/open-zwave.git
cd open-zwave
make
```

### Tellstick support
If you need support for Tellstick or Tellstic Duo (http://www.telldus.se/products/range) you need to install telldus-core before compileing. 
On systems with apt package manger (like ubuntu and Raspbian) telldus-core can be installed by doing the following:
 * Add `deb http://download.telldus.com/debian/ stable main` to the file `/etc/apt/sources.list`
 * Run
```
wget -q http://download.telldus.se/debian/telldus-public.key -O- | sudo apt-key add -
sudo apt-get update
sudo apt-get install libtelldus-core-dev
```

|* Darwin (Mac)
|There is a problem in the MakeFile, change the DEBUG/RELEASE defines with:
|DEBUG_CFLAGS    := -Wall -Wno-unknown-pragmas -Wno-inline -Werror -Wno-self-assign -Wno-unused-private-field -Wno-format -g -DDEBUG -DLOG_STDERR -fPIC
|RELEASE_CFLAGS  := -Wall -Wno-unknown-pragmas -Werror -Wno-self-assign -Wno-unused-private-field -Wno-format -O3 -DNDEBUG -fPIC
|
|Also you need to add the following at the top of the files ../../src/platform/unix/LogImpl.cpp and ../../src/platform/unix/SerialControllerImpl.cpp
|
|#include <pthread.h>
|

make


### Ubuntu / Raspberry Pi (wheezy)
```
sudo apt-get install build-essential -y
sudo apt-get install cmake libboost-dev libboost-thread-dev libboost-system-dev libsqlite3-dev subversion curl libcurl4-openssl-dev libusb-dev libudev-dev zlib1g-dev libssl-dev
```

Raspberry Pi (wheezy, 22 November 2012): (First time compile time: 25 minutes)
At this moment (First generation RPi (128MB) (And possible the last generation too), there is a issue with the FTDI drivers (USB).
A quick fix it to add the following in front of /boot/cmdline.txt
`dwc_otg.speed=1`
This will cause the USB to switch back to version 1.1, but also the network speed is reduced a lot! (not that i want to use 80mbps)
When using the Arch Linux image, it looks like you do not have to apply the above fix.

### Raspberry Pi (Arch Linux)
`sudo pacman -S gcc cmake subversion boost sqlite make curl libusb libudev-dev`

### Debian (Mac OS)
OS X installation:

You need
- XCode with Command Line Tools
- macports or homebrew

```
sudo port install cmake
sudo port install boost
sudo port install libusb
sudo port install libusb-compat
sudo port install libudev-dev
sudo port install zlib1g-dev
```

