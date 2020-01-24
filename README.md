### User instructions

1. Follow the instructions here to download the executable for your operating system: https://github.com/Massdrop/mdloader/releases

2. Plug in your keyboard.

3. In your terminal, change to the directory where you downloaded the executable and applet-*.bin file(s).

4. **Windows** - Run `mdloader_windows.exe --first --download FILE_NAME --restart`. Replace "FILE_NAME" with the filename of your compiled firmware.  
**Linux** - Run `mdloader_linux --first --download FILE_NAME --restart`. Replace "FILE_NAME" with the filename of your compiled firmware. Depending on your user's permissions, you might have to add your user to the `dialout` group or use `sudo` on the command.  
**Mac** - Run `mdloader_mac --first --download FILE_NAME --restart`.  If you downloaded with Mac Safari, run `mdloader_mac.dms --first --download FILE_NAME --restart`. Replace "FILE_NAME" with the filename of your compiled firmware. 
  
5. You should see the message:  
```
Scanning for device for 60 seconds  
.....
```

6. Within 60 seconds, press the reset button on your keyboard. For most keyboards running the default firmware, you can hold `Fn` + `b` for half a second and release to reset your keyboard (you will see the LEDs turn off). For CTRL keyboards in the first production run running original firmware or of the first method does not work for you, you will need to use a pin to press the reset button through the hole in the bottom of the keyboard.

7. You should see a series of messages similar to:
```
Device port: /dev/cu.usbmodem234431 (SAMD51J18A)

Opening port '/dev/cu.usbmodem234431'... Success!
Found MCU: SAMD51J18A
Bootloader version: v2.18Sep  4 2018 16:48:28
Applet file: applet-flash-samd51j18a.bin
Applet Version: 1
Writing firmware... Complete!
Booting device... Success!
Closing port... Success!
```

8. Afterwards, you should see the keyboard's LEDs light up again (if your configuration has LEDs enabled) and the keyboard should respond to typing. Your keyboard is now running the new firmware you specified.

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

**Linux**: User may need to be added to group dialout to access programming port  
**Mac**: User may need to prefix mdloader_mac: `./mdloader_mac` per [#8](https://github.com/Massdrop/mdloader/issues/8)  
