# PreAmpArduino

This is the Arduino code needed to support a design for a pre amp which will do the following:
- select between 4 input ports, configurable XlR or RCA
- support for 1 or 2 ladder boards, with 1 XLR and balance are not supported
- 64 steps volume control
- mute
- standby mode
- support output led to steer relay powering (pre)AMP
- support output led to steer relay to switch direct out or via Amp
- support output led steer relay to switch between amp and headphones
- output led to light a LED if device is in standby
- oled screen to show releveant info
- support of remote control, apple only
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

Code is as is. No support we use the current version ourselves. Code runs on an Arduino Every only due to amount of program storage and dynamic memory

You need to install the code on the every using standard IDE. Code requires some additional libraries 


authors:
Walter Widmer
Peter den Heijer


open issues
1 screen type. 


