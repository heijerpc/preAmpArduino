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

Due to "strange" default values used within the arduino we struggled with the stability of the NVram. Low voltage issues are monitored using a fuse. However both Arduino IDE and PlatformIO do not write this fuse by default. The default value of the Arduino is disabled for the Brown Out Detetion (BOD). This means the cpu is not turned of if power drops. This could result in corrupted NVram. 2 options:

option 1
If you like to use the Arduino IDE you need to write the BOD fuse once. As the Arduino IDE using the standard board manager does not support the BOD fuse we need to write it once using another board manager. As the standard board doesnt write it anymore we can go back to the standard board manager. Even with a power down the value of the BOD doesnt change. What to do:
We first add another board manager:
- go to file -> preferences and at the following line to Additonal  board managers URL:
 https://mcudude.github.io/MegaCoreX/package_MCUdude_MegaCoreX_index.json 
- go to board managers (second item from the top left hand side of the screen)
- install the board manager MegaCoreX
- now we have the option to select a different board manager from MegacoreX being the ATmega4809
- when selected we can set some values in the tools menu:
  bod at 4.3V
  pinlayout nano every
  Keep the rest as is
- Now compile and download any prog to the arduino every
- the bod is now set so we return back to the standard board manager
- select the nano every again as board manager
- reload the IDE
- now you can compile and download the preAmp.ino 

option 2
Use PlatformIO as IDE. PlatformIO is capable of writing all fuses. However this requires some editing as also PlatformIO doesnt write the BOD by default. You need to change the nano_every.json file adding the fuse value
of the bodcfg. This means you have to add the line "bodcfg":"0xE4". This will result in the following:

 "fuses": {
    "syscfg0": "0xC9",
    "bootend": "0x00",
    "osccfg": "0x01",
    "bodcfg": "0xE4"
  },

 As a result PlatformIO will write the fuse bodcfg with a value of E4 which will enable brown out detection at 4.2V

Again, we do not provide you support. We provide you the code, the design and all required files to build it yourself. You are also free to change anything 


authors:
Walter Widmer
Peter den Heijer





