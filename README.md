# CircuitBentBaby
This repository contains the test code for the CircuitBentBaby senior design project (CPE 424-A, Spring 2023).

## Project Overview

CircuitBentBaby focuses on developing inexpensive, accessible electronics for video
synthesis. First dating back to the late 60s, this experimental artform leverages colors, signals,
and mathematics to create moving image on screen, typically in live performance or installation.
According to the late Nam June Paik, the proverbial father of Video Art, future "artists will work
with capacitors, resistors, and semiconductors as they work today with brushes, violins, and
junk." While video synthesis tools are still in production today, its niche market is dominated by
increasingly expensive and obsolete technology. CircuitBentBaby remedies 2 problems: price
and accessibility. Because CBB’s video synth runs on a microcontroller, a low cost computer, it
is offered at a fraction of the cost of its competitors. Also, the synth is professionally documented
and open source, so artists can learn how to modify it for their performances. Unlike most
modern products, CBB’s synth uses low level electronics, and is designed to be modified by its
users. In an age in which the consumer landscape is dominated by complex electronics, unable to
be modified, and locked behind tight warranties, CircuitBentBaby’s engineers subvert the status
quo by creating easily-accessible, low-level tools for video artists to interact with.

**Team Members:** Christopher Kruger, Nixon Puertollano, Trent Reichenbach, Matthew Tricomi, Liamjohn Velazquez, Walter Wargacki

**Project Advisor:** Prof. David Darian Muresan

## Installation Instructions

Below are the instructions to build and flash the test programs onto the CircuitBentBaby device.

*NOTE: These instructions are specifically for Linux operating systems. Windows and Mac have not been tested, so a Linux virtual machine can be used for computers running those operating systems. A Unix shell program such as MSYS2 may also work, but this has not been tested as thouroughly.*

1. Install Git onto your system and clone this repository, as well as the repositories for [the Pico SDK](https://github.com/raspberrypi/pico-sdk) and [Pico Extras](https://github.com/raspberrypi/pico-extras).
2. Set the environment variables for the paths to the Pico SDK and Pico Extras directories. This can be done by typing `nano .bashrc` into the terminal and adding the below lines anywhere in the file (terminal will have to be closed and reopened for the lines to take effect).
```
export PICO_SDK_PATH=(path to Pico SDK directory)
export PICO_EXTRAS_PATH=(path to Pico Extras directory)
```
3. Download the required programs for building.
```
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib
```

Steps 1-3 only need to be done once. The following steps assume the program is already written and the CMake files are properly set up, such as for the programs in this repository.

4. From the terminal, navigate to the directory for the program and run the following commands.
```
mkdir build
cd build
cmake ..
make (name of program)
```
5. Open the build directory and locate the `.uf2` file. Plug the CircuitBentBaby device into the USB port (if the device already has a program flashed onto it, hold down the button on the Pico while plugging the device in, then release the button). Open the folder for the Pico, then drag and drop the `.uf2` file into it.

## Links

[Project Site](https://sites.google.com/stevens.edu/circuitbentbaby/home)
