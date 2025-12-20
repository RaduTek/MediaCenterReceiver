# Arduino Windows Media Center Remote Receiver

This is an Arduino project targetting ATMEGA32u4 based Arduino boards that receives IR commands and translates them to USB HID events, in order to control the Windows Media Center application.

It can be considered a functional equivalent to the original eHome/WMC IR receivers, but it does not simulate it's behavior.

## Requirements

-   [HID-Project](https://github.com/NicoHood/HID) - advanced USB HID functions (Keyboard, Consumer and Mouse)
-   [IRremote](https://github.com/Arduino-IRremote/Arduino-IRremote) - Receive IR remote signals

## Hardware

-   ATMEGA32u4 based board
-   IR receiver module (e.g. VS1838B) connected to pin 2

## Features

-   Implements most buttons with Windows Media Center shortcuts
-   Supports proper key press and release events (the operating system or application handles key repeat)
-   Implements T9 style typing on the numerical pad

## Supported remotes

-   Windows Media Center remotes using the RC6 protocol
-   Logitech Z Cinema remote

Other remotes can be easily added by modifying the `handle_keypad` function in the `MediaCenterReceiver.ino` file, and also ensuring the right protocol and address is added to the loop function, where the key code is generated.

To easily make the key code definition header files, see the included Excel spreadsheet.

### WMC Remote notes

The colored buttons (Red, Green, Yellow, Blue), available on Europe region remotes for use with Teletext features, are not implemented, because there are no equivalent keyboard shortcuts for these keys.

The Eject key found on some WMC remotes is bound to the Eject key, part of HID Consumer Control, but is not implemented by WMC software.

### Logitech Z Cinema remote notes

![Logitech Z Cinema remote picture](https://i.ebayimg.com/images/g/lGcAAOSwux5YSnh9/s-l400.jpg)

This is a remote that came bundled with the Logitech Z Cinema speaker system, which can take a digital audio input from a computer over USB.

This remote is designed to work with WMC, and such it has most of the buttons you'd expect.

It also has additional buttons that were supposed to control the speaker system, and the highlight: a spinning wheel.

With this project, the wheel can work in 4 modes:

-   Volume control (default) - press the Headphone button (bottom-left) to activate
-   Mouse scroll - press the SRS button (top-left) to activate
-   Horizontal arrows - press the Menu button (top-right) to activate
-   Vertical arrows - press the Reset button (bottom-right) to activate

These modes let you navigate software like Windows Media Center in an iPod-like way.

The wheel's middle button is always bound to the Enter key.

The 4 preset keys above the wheel, as well as the 3 input keys (usb, aux, display) are not yet used for anything.

## Using the numeric input pad

The numeric pad simulates a T9 style text input by default.

Keys 2-9 respect the legend printed on the remote buttons. Key 1 enters a variety of punctuation symbols, and key 0 is the space key.

Press the \* (star) key to enable shift mode - capitalize letters.

Press the # (pound) key to switch between T9 alphanumeric mode and numeric only mode.

Use the Clear key as backspace, and Enter key to confirm inputs in the used software.

## Comparison with original receivers

Original eHome/WMC receivers are in fact IR transceivers, as they can both send and receive raw IR data. IR commands are decoded and bound to input events at the driver level.

This project does not implement a USB device compatible with the eHome drivers. Instead, it decodes and handles IR input internally, and converts it into USB HID Keyboard, Mouse or Consumer events.
