# Zephyr development environment to train myself

This repo contains a VSCode [DevContainer](.devcontainer/devcontainer.json) that uses the Docker Container defined in [zephyr_container](https://github.com/silvio-vallorani/zephyr_container).

When the DevContainer starts, it create a Zephyr Project with only the modules listed in the [west](west.yml) file.

This repository can be used to learn Zephyr: the environment can be identically reproduced everywhere.

Some functions involve the use of Segger Jlink. All of them can be used with JLink EDU / JLink OB. In my case I use the [SEGGER ST Reflash](https://www.segger.com/products/debug-probes/j-link/models/other-j-links/st-link-on-board/) tool to convert the onboard ST-Link/v2 in JLink OB on the nucleo board I use.

## How to use

- Install [Docker Desktop](https://www.docker.com/products/docker-desktop/) and start it.
- Install [VSCode](https://code.visualstudio.com/Download) and the [Dev Containers Extension Pack](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers).
- Clone the repository and open it with VSCode.
- Using Command Palette (Ctrl+Shift+P) select "Remote-Containers: Reopen in Container" to start the container.

## List of Zephyr functionality i wanna try

- [x] Create a Docker Container with all the tools to dev a Zephyr Project
- [x] Create a VSCode DevContainer that use the Docker Container and starts a Zephyr Project for an ST Nucleo board (used nucleo-f401re)
- [x] Create VSCode tasks to build MCUBoot bootloader and Application for slot-0 and slot-1
- [x] Create VSCode tasks to build Application with CodeChecker, Start a CodeChecker server and store the report in the server to view the potentially issues
- [x] Create a SEGGER Ozone project which view the build and src folders/files to debug the firmware using the Zephyr Plugin for Thread Awareness and STM32F401.svd file to debug peripherals registers
- [x] Use board overlay/binding to define some GPIO which will be used for debugging with a Logic Level Analyzer
- [x] Use board overlay to describe the attached DHT11 temperature/humidity sensor
- [x] Use board overlay to describe the attached WS2812B RGB strip led
- [x] Define a static thread which periodically fetch the DHT11 sensor (using sensor api)
- [x] Define a static thread which periodically update the strip-led (using led-strip api)
- [x] Use a Message Queue to do threaded actions from IRQ
- [x] Add Segger RTT to print debug messages on the Ozone RTT terminal ad/or use SEGGER SystemView
- [x] Add MCUBoot to manage the DFU using signed images and boot/slot-0/slot-1 partitioning method
- [x] Prepare CommandFiles to flash boot/slot-0/slot-1 images separately; used to test MCUBoot
- [x] Use the Settings subsystem to store in non volatile memory some key-value pairs
- [x] Use the Zephyr bus (zbus) subsystem to exchange data between threads: sensor thred produce data (publisher) which will be used by the strip-led thread (subscriber) and immediately toggle on-board led (listener)
- [x] Use the Core Dump subsystem to dump the application memory on fatal error (refer [here](https://blog.espressif.com/core-dump-a-powerful-tool-for-debugging-programs-in-zephyr-with-esp32-boards-969830fd6cdb) for backtracing the dump )
- [x] Use the Retention System to guarantee some ram-stored info to be valid after reset
- [x] Use the Hardware Info System to get some info about the board e.g. reset cause
- [x] Use Task Watchdog subsystem to supervise the application
- [ ] Use the Twister subsystem to write some unit tests and run them (try to develop in TDD way)
- [ ] Use the State Machine Framework to define a simple state machine
- [ ] Use user space / privileged mode to keep memory areas more safe
- [ ] Use Trusted Firmware-M to manage the project life in a secure context

- [ ] Change default partition slots to be used as custom static MCUBoot partition layout
- [ ] Code a custom driver for a device (e.g. an i2c device)
- [ ] Define a totally custom board and use it to test all the listed feature: should be done without refactoring the application code at all
- [ ] Use the Zephyr shell to interact with the application
