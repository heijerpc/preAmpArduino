# PreAmpArduino

This is the Arduino code needed to support a design for a pre amp which will do the following:
- select between 4 input ports, configurable XLR or RCA
- support for 1 or 2 ladder boards, with 1 ladder board XLR and balance are not supported
- 64 steps volume control
- mute
- standby mode
- support output led to steer relay powering (pre)AMP
- support output led to steer relay to switch direct out or via Amp
- support output led steer relay to switch between amp and headphones
- output led to light a LED if device is in standby
- oled screen to show releveant info
- support of remote control, Apple remote only
- all changeable variables are stored in EEPROM
- 5 buttons
  - standby
  - headphones
  - direct out
  - channel select
  - mute
- volume rotary is used to control volume and menu.
- changeable delay between relay volume switches. Volume will be switch from old value to intermediate to next value. Delay between changes is configurabel to reduce plop. 


- Via a menu:
  - support for balance
  - startup delay
  - brigthness of the screen
  - preamp amplify rate to adjust displayed vol level
  - changeable names of input channels
  - balance
  - initial startup volume, either per channel or general

Code is based on:
- implementation of AMP's build by Jos van Eindhoven
- PassivePreamp available on DIY audio

Code is as is. No support, however we use the current version ourselves. Code runs on an Arduino Every only due to amount of program storage and dynamic memory

You need to install the code on the Every using standard IDE. Code requires some additional libraries. If you are not familiar with the arduino please ask for any help from somebody who knows. Otherwise do a small course which are provided on the internet to understand how to load code on an arduino. 


The supporting files to build the board are included.

Again, we do not provide you support. We provide you the code, the design and all required files to build it yourself. You are also free to change anything 


authors:
Walter Widmer
Peter den Heijer





