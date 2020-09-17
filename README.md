# BootNext-Efi
A tool allowing for rebooting directly to your target OS. This repo contains the EFI part of the code.

## About
This tool allows you to reboot to your target OS without having to click through your bootloader/UEFI.

This is made possible by the EFI application in this repository. It reads both a config and a target file and then decides which OS to boot.
If there is no "next target" set, it will simply boot the default file specified in the config.

The target file (and this application itself) can easily be installed by the corresponding client for your OS - currently available for Windows and macOS.
The Linux version is still in development.

## Compilation
This tool is compiled using `gnu-efi`. The compilation can be started by running `build.sh`.
This will also create a simple HDD image and run a `qemu` VM to test the bootloader.
You may need to install some dependencies on your OS for this to work (building on Linux is highly recommended).
