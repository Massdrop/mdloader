### tl;dr

1. Follow the instructions here to download the executable for your operating system: https://github.com/Massdrop/mdloader/releases

2. Plug in your keyboard.

3. In your terminal, change to the directory where you downloaded the executable and applet-*.bin file(s).

4. **Windows** - Run `timeout 15 ; mdloader_windows.exe --first --download FILE_NAME --restart`. Replace "FILE_NAME" with the filename of your compiled firmware.  
**Linux** - Run `sleep 15 && mdloader_linux --first --download FILE_NAME --restart`. Replace "FILE_NAME" with the filename of your compiled firmware.  
**Mac** - Run `sleep 15 && mdloader_mac --first --download FILE_NAME --restart`.  If you downloaded with Mac Safari, run `mdloader_mac.dms --first --download FILE_NAME --restart`. Replace "FILE_NAME" with the filename of your compiled firmware.  

5. Press the reset button on your keyboard within 15 seconds.

6. Enjoy (important)

-----

# Massdrop Loader

Massdrop Loader is used to read firmware from and write firmware to Massdrop keyboards which utilize Microchip's SAM-BA bootloader, over the USB connection.

## Supported operating systems

Windows XP or greater (32-bit and 64-bit versions, USB Serial driver in drv_win folder)  
Linux x86 (32-bit and 64-bit versions)  
Mac OS X

## Supported devices

Massdrop keyboard's featuring Microchip's SAM-BA bootloader.

## Building

Enter mdloader directory where Makefile is located and excute:

`make`

This will create a `build` directory with the compiled executable and required applet-*.bin files.  
Run `./build/mdloader` to test.
Note that the target MCU applet file must exist in the directory the executable is called from.

## Usage
```
Usage: mdloader [options] ...
  -h --help                      Print this help message
  -v --verbose                   Print verbose messages
  -V --version                   Print version information
  -f --first                     Use first found device port as programming port
  -l --list                      Print valid attached devices for programming
  -p --port port                 Specify programming port
  -U --upload file               Read firmware from device into <file>
  -a --addr address              Read firmware starting from <address>
  -s --size size                 Read firmware size of <size>
  -D --download file             Write firmware from <file> into device
  -t --test                      Test mode (download/upload writes disabled, upload outputs data to stdout, restart disabled)
     --cols count                Hex listing column count <count> [8]
     --colw width                Hex listing column width <width> [4]
     --restart                   Restart device after successful programming
```

To write firmware to the device and restart it:

`mdloader --first --download new_firmware.hex --restart`

The program will now be searching for your device. Press the reset switch found through the small hole on the back case or by appropriate key sequence to enter programming mode and allow programming to commence.  
Firmware may be provided as a binary ending in .bin or an Intel HEX format ending in .hex, but .hex is preferred for data integrity.  
Note that safeguards are in place to prevent overwriting the bootloader section of the device.

To read firmware from the device:

`mdloader --first --upload read_firmware.bin --addr 0x4000 --size 0x10000`

Where --addr and --size are set as desired.  
Note the output of reading firmware will be in binary format.

Test mode may be invoked with the --test switch to test operations while preventing firmware modification.  
Test mode also allows viewing of binary data from a read instead of writing to a file.

## Troubleshooting

Linux: User may need to be added to group dialout to access programming port  
