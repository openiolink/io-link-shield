# Introduction
This product is an adapter board for connecting up to four M12 IO-Link devices to a Raspberry PI for operation with open source software. 
The repository contains the IO-Link master stack for using the adapter board.

## Autors

- Daniel Kessler, Balluff
- Dominik Nille, Balluff
- Pascal Frei
- Markus Gafner, BFH

## Requirements



## Execution only
For an automatic and fast setup of your Pi, the ZIP folder can be downloaded under Releases. After downloading, the ZIP folder must be unpacked.
Then run the shell script to setup all requirements with the following command:
```bash
sh quick_setup.sh
```

Execute the already pre-compiled binary with 
```bash
./openiolink
```

## Build and compile the project
To use the application on RaspberryPi 4B, the operating system [Raspbian](http://raspbian.org) is used. Also, the library [WiringPi](http://wiringpi.com) and the build tool [CMake](https://cmake.org) is required. The Library WiringPi is offical deprecated for buster and bullseye debian-based os.

This can get installed using the command:
```bash
sudo apt-get install mosquitto wiringpi cmake
```
If wiringpi is not available install the .deb file provided in this repo using the command:
```bash
sudo dpkg -i wiringpi-2.61-1.deb
```

## Build and compile the project
The easiest way to compile the project is, to copy all sources to the RaspberryPi, and compile the project on the RaspberryPi itself or use Visual Studio Code with a SSH Connection.

- Copy all source files to a folder you like, for example `/home/pi/projects/`
- With the SSH-connection, move to the folder you copied the files: `cd /home/pi/projects/`
- Initialize CMake: `cmake .`
- Build the binary: `make`

If every step was successful, in the project folder should be an executable file, for example `openiolink`. This one can be executed using `./openiolink`.
