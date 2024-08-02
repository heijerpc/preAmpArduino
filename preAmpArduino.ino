//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Global definitions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// v0.2, integration of oled screen
// v0.3 optimize and change, adapt volume part for screen and rotator
// v0.4 included functions to support relay, include 2 buttons (mute and channel) and restructured setup due to lack of readabilitie
// v0.5 included IR support, fixed issue with LSB B bank controling volume.
// v0.6 included eeprom support, 3 to 4 channels, headphone support, passiveAmp support,
// v0.7 added IR codes, changed channelinput to + and -, added long delay to led preamp stabilize, added menu, changed array and struct in eeprom
//      changed oledscreen type and procedures to write data to screen
// v0.81 added portselection to second board, if volume >63 <0 dont do anything.
// v0.93  changed oled screen to full graph
//       changed setup of volume as we had large blups during volume change
//       changed init string for 017chip
//       changed setup menu, quit after 90 sec, return to orginal volume
//       change volume changes if volume below 0 or above 63
//       changed again volume setup using and function and previous volume level
//       included eeprom programming
//       changed proc how to handle keeping button pressed on IR controler
// v0.94 changed setup menu
//       full graph support
//       optimized the debug options
//       added option to define input port config xlr/rca
//       added option to change startup delay
// v0.95 added option to change channel name
//       optimized menu
//       changed balance
// v0.96 bug fixes
//       made alive boolean persistant, so preamp starts up in previous mode (active/standby)
//       made startup screen more customabel 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// below definitions could be change by user depending on setup, no code changes needed
//#define DEBUG                                     // Comment this line when DEBUG mode is not needed
const bool DaughterBoard = true;                    // boolean, defines if a daughterboard is used to support XLR and balance, either true or false
const uint8_t InputPortType = 0b00000011;           // define port config, 1 is XLR, 0 is RCA. Only used when daughterboard is true, LSB is input 1
#define DelayPlop 20                                // delay timer between volume changes preventing plop, 20 mS for drv777
const char* Toptekst = "PeWalt, V 0.96";            // current version of the code, shown in startscreen top, content could be changed
const char* MiddleTekst = "          please wait";  //as an example const char* MiddleTekst = "Cristian, please wait";
const char* BottemTekst = " " ;                     //as an example const char* BottemTekst = "design by: Walter Widmer" ;
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// definitions for EPROM writing
#include <EEPROM.h>
struct SavedData {         // definition of the data stored in eeprom
  char UniqueString[9];    // unique indentifier to see if eeprom is programmed and correct version
  bool VolPerChannel;      // boolean, if True volume level per channel is defined, otherwise one generic volume is used
  uint8_t SelectedInput;   // last selected input channel
  int BalanceOffset;       // balance offset of volume, difference between left and right
  int ContrastLevel;       // contraslevel of the screen
  int PreAmpGain;          // vol on screen is between -63 and 0, PreAmpGain is added to value displayed on the screen
  int StartDelayTime;      // delay after power on amp to stabilize amp output
  bool HeadphoneActive;    // boolean, headphones active, no output to amp
  bool DirectOut;          // boolean, preamp is used passive or active mode
  bool PrevStatusDirectOut;// boolean, status of previous status of direct out, used when using headphones
  bool Alive;              // boolean, defines if amp is active of standby modes
  char InputTekst[5][15];  // description of input channels
};
SavedData InitData;  // initdata is a structure defined by SavedData containing the values
int VolLevels[5];    // Vollevels is an array storing initial volume level when switching to channel, used if volperchannel is true
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// pin definitions
#define PowerOnOff A0         // pin connected to the relay handeling power on/off of the amp
#define HeadphoneOnOff A1     // pin connected to the relay handeling headphones active / not active
#define DirectOutState A2     // pin connected to the relay handeling direct out state of amp
#define StartDelay A3         // pin connected to the relay which connects amp to output
volatile int rotaryPinA = 3;  // encoder pin A, volatile as its addressed within the interupt of the rotary
volatile int rotaryPinB = 4;  // encoder pin B,
#define RotaryButton 5        // pin is connected to the switch part of the rotary
#define ButtonChannel 6       // pin is conncected to the button to change input channel round robin, pullup
#define ButtonStandby 7       // pin is connected to the button to change standby on/off, pullup
#define buttonHeadphone 8     // pin  is connected to the button to change headphone on/off, pullup
#define ButtonDirectOut 9     // pin  is connected to the button to switch between direct out or amp, pullup
#define ButtonMute 10         // pin  is connected to the button to change mute on/off, pullup
#define LedStandby 11         // connected to a led that is on if amp is in standby mode
#define OledReset 12          // connected to the reset port of Oled screen, used to reset Oled screen
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// definitons for the IR receiver
#define DECODE_NEC        // only add NEC prot to support apple remote, saves memory, fixed keyword
#include <IRremote.hpp>   // include drivers for IR
#define IR_RECEIVE_PIN 2  // datapin of IR is connected to pin 2
#define AppleLeft 9       // below the IR codes received for apple
#define AppleRight 6
#define AppleUp 10
#define AppleDown 12
#define AppleMiddle 92
#define AppleMenu 3
#define AppleForward 95
int DelayTimer;                       // timer to delay between volume changes using IR, actual value set in main loop
unsigned long sMillisOfFirstReceive;  // used within IR procedures to determine if command is a repeat
bool sLongPressJustDetected;          // used within IR procedures to determine if command is a repeat
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// definitions for the oled screen
#define OLED_Address 0x3C                            // 3C is address used by oled controler
#define fontH08 u8g2_font_timB08_tr                  // 11w x 11h, char 7h
#define fontH08fixed u8g2_font_spleen5x8_mr          // 15w x 14h, char 10h
#define fontH10 u8g2_font_timB10_tr                  // 15w x 14h, char 10h
#define fontH10figure u8g2_font_spleen8x16_mn        //  8w x 13h, char 12h
#define fontH14 u8g2_font_timB14_tr                  // 21w x 18h, char 13h
#define fontgrahp u8g2_font_open_iconic_play_2x_t    // 16w x 16h pixels
#define fontH21cijfer u8g2_font_timB24_tn            // 17w x 31h, char 23h
char VolInChar[4];                                   // used on many places to convert int to char
#include <U8g2lib.h>                                 // include graphical based character mode library
U8G2_SSD1309_128X64_NONAME0_F_HW_I2C Screen(U8G2_R2);  // define the screen type used.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// definitions for the attenuator board
#define MCP23017_I2C_ADDRESS_bottom 0x25  // I2C address of the relay board bottom
#define MCP23017_I2C_ADDRESS_top 0x26     // I2C address of the relay board daughterboard
int AttenuatorMain;                       // actual internal volume level,
int AttenuatorRight;                      // actual volume value sent to relay
int AttenuatorLeft;                       // actual volume value sent to relay
int AttenuatorRightTmp;                   // intermediate volume sent to relay
int AttenuatorLeftTmp;                    // intermeditate volume sent to relay
volatile int AttenuatorChange = 0;        // change of volume out of interupt function
bool muteEnabled = false;                 // boolean, status of Mute
bool VolumeChanged = false;               // defines if volume is changed
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// general definitions
#include <Wire.h>                          // include functions for i2c
#include <ezButton.h>                      // include functions for debounce

ezButton button(RotaryButton);             // create ezButton object  attached to the rotary button;
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// definitions for the compiler
void DefineVolume(int increment);                         // define volume levels
void RotaryTurn();                                        // interupt handler if rotary is turned
void WriteVolumeScreen(int volume);                       // write volume level to screen
void WriteFixedValuesScreen();                            // write info on left side screen (mute, passive, input)
void OledSchermInit();                                    // init procedure for Oledscherm
void MCP23017init(uint8_t MCP23017_I2C_ADDRESS);          // init procedure for relay extender
void SetRelayChannel(uint8_t Word);                       // set relays to selected input channel
void SetRelayVolume(int RelayLeft, int RelayRight);       // set relays to selected volume level
void ChangeStandby();                                     // changes status of preamp between active and standby
void ChangeHeadphone();                                   // change status of headphones between active and standby
void ChangeDirectOut();                                   // change status of preamp between direct out and amplified
void ChangeMute();                                        // enable and disable mute
void ChangeInput(int change);                             // change the input channel
void WaitForXseconds();                                   // wait for number of seconds, used to warm up amp
bool DetectLongPress(uint16_t aLongPressDurationMillis);  // detect if multiple times key on remote controle is pushed
void CheckIfEepromHasInit();                              // check if EEPROM is configured correctly
void WriteEEprom();                                       // write initial values to the eeprom
void MainSetupMenu();                                     // the main setup menu
void SetupMenuBalance();                                  // submenu, setting the balance
void SetupMenuInitVol();                                  // submenu, setting initial values of volume
void SetupMenuGeneral();                                  // submenu, setting general parameters
void SetupMenuChangeNameInputChan();                      // submenu, setting names of input channels
char* ChVolInChar3(int volume);                           // return 2 digit integer in char including +/-
char* ChVolInChar2(int volume);                           // return 1 digit integer in char including +/-
void ListContentEEPROM();                                 // used in debug mode to print content of eeprom
void ScanI2CBus();                                        // used in debug mode to scan the i2c bus
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // pin modes
  pinMode(PowerOnOff, OUTPUT);             // control the relay that provides power to the rest of the amp
  pinMode(HeadphoneOnOff, OUTPUT);         // control the relay that switches headphones on and off
  pinMode(DirectOutState, OUTPUT);         // control the relay that switches the preamp between direct out or active
  pinMode(StartDelay, OUTPUT);             // control the relay that connects amp to output ports
  pinMode(rotaryPinA, INPUT_PULLUP);       // pin A rotary is high and its an input port
  pinMode(rotaryPinB, INPUT_PULLUP);       // pin B rotary is high and its an input port
  pinMode(ButtonChannel, INPUT_PULLUP);    // button to change channel
  pinMode(ButtonStandby, INPUT_PULLUP);    // button to go into standby/active
  pinMode(buttonHeadphone, INPUT_PULLUP);  // button to switch between AMP and headphone
  pinMode(ButtonDirectOut, INPUT_PULLUP);  // button to switch between passive and active mode
  pinMode(ButtonMute, INPUT_PULLUP);       // button to mute
  pinMode(LedStandby, OUTPUT);             // led will be on when device is in standby mode
  pinMode(OledReset, OUTPUT);              // set reset of oled screen
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // write init state to output pins
  digitalWrite(PowerOnOff, LOW);      // keep amp turned off
  digitalWrite(HeadphoneOnOff, LOW);  // turn headphones off
  digitalWrite(DirectOutState, LOW);  // turn the preamp in active state
  digitalWrite(StartDelay, LOW);      // disconnect amp from output
  digitalWrite(LedStandby, LOW);      // turn off standby led to indicate device is becoming active
  digitalWrite(OledReset, LOW);       // keep the Oled screen in reset mode
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// enable debug to write to console if debug is enabled
 #ifdef DEBUG
  Serial.begin(9600);  // if debuglevel on start monitor screen
 #endif
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // // read setup values stored within the eeprom
  CheckIfEepromHasInit();      // check if eeprom has config file, otherwise write config
  EEPROM.get(0, InitData);     // get variables within init out of EEPROM
  EEPROM.get(110, VolLevels);  // get the array setting volume levels
 #ifdef DEBUG                   // if debug enabled write message
  Serial.println(F("initprog: the following values read from EEPROM"));
  ListContentEEPROM();
 #endif
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Set up pins for rotary:
  attachInterrupt(digitalPinToInterrupt(rotaryPinA), RotaryTurn, RISING);  // if pin encoderA changes run RotaryTurn
  button.setDebounceTime(50);                                              // set debounce time to 50 milliseconds
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // startup IR
  IrReceiver.begin(IR_RECEIVE_PIN);  // Start the IR receiver
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // initialize the oled screen and relay extender
  Wire.begin();  // start i2c communication
  delay(100);
 #ifdef DEBUG
  ScanI2CBus();  // in debug mode show i2c addresses used
 #endif
  OledSchermInit();  // intialize the Oled screen
  delay(100);
  MCP23017init(MCP23017_I2C_ADDRESS_bottom);  // initialize the relays
  if (DaughterBoard) {                        // if we have a daughterboard
    MCP23017init(MCP23017_I2C_ADDRESS_top);   // initialize daugterboard
  }
  SetRelayVolume(0, 0);  // set volume relays to 0, just to be sure
  delay(DelayPlop);     // wait to stabilize
  SetRelayChannel(0);   // disconnect all input channels
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // set input / volume / etc depending standby active state
  InitData.Alive=!InitData.Alive; //invert bool so we can use standard standby proc
  ChangeStandby();
 #ifdef DEBUG
  Serial.println(F("Setup: end of setup proc"));
 #endif
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Main loop
//////////////////////////////////////////////////////////////////////////////////////////////
void loop() {
  if (InitData.Alive) {                                         // we only react if we are in alive state, not in standby
    if (AttenuatorChange != 0) {                                // if AttenuatorChange is changed by the interupt we change volume/etc
      DefineVolume(AttenuatorChange);                           // Calculate correct temp and end volumelevels
      if (VolumeChanged) {                                      // if volume is changed
        SetRelayVolume(AttenuatorLeftTmp, AttenuatorRightTmp);  // set relays to the temp level
        delay(DelayPlop);                                       // wait to prevent plop
        SetRelayVolume(AttenuatorLeft, AttenuatorRight);        // set relay to the correct level
        WriteVolumeScreen(AttenuatorMain);                      // display volume level on oled screen
      }
      AttenuatorChange = 0;  // reset the value to 0
    }
    if (digitalRead(ButtonMute) == LOW) {  // if button mute is pushed
      delay(500);                          // wait to prevent multiple switches
      ChangeMute();                        // change status of mute
    }
    if (digitalRead(ButtonChannel) == LOW) {  // if button channel switch is pushed
      delay(500);                             // wait to prevent multiple switches
      ChangeInput(1);                         // change input channel
    }
    if (digitalRead(buttonHeadphone) == LOW) {  // if button headphones switch is pushed
      delay(500);                               // wait to prevent multiple switches
      ChangeHeadphone();                        // change to headphone or back
    }
    if (digitalRead(ButtonDirectOut) == LOW) {  // if button passive switch is pushed
      delay(500);                               // wait to prevent multiple switches
      ChangeDirectOut();                        // change active/passive state
    }
    button.loop();
    if (button.isPressed()) {  // if rotary button is pushed go to setup menu
      MainSetupMenu();         // start setup menu
      delay(500);              // wait to prevent multiple switches
    }
  }
  if (digitalRead(ButtonStandby) == LOW) {  // if button standby is is pushed
    delay(500);                             // wait to prevent multiple switches
    ChangeStandby();                        // changes status
  }
  if (IrReceiver.decode()) {      // if we receive data on the IR interface
    if (DetectLongPress(1500)) {  // simple function to increase speed of volume change by reducing wait time
      DelayTimer = 0;
    } else {
      DelayTimer = 300;
    }
    switch (IrReceiver.decodedIRData.command) {  // read the command field containing the code sent by the remote
      case AppleUp:
        if (InitData.Alive) {        // we only react if we are in alive state, not in standby
          DefineVolume(1);           // calculate correct volume levels, plus 1
          if (VolumeChanged) {
            SetRelayVolume(AttenuatorLeftTmp, AttenuatorRightTmp);  //  set relays to the temp  level
            delay(DelayPlop);                                       // wait to prevent plop
            SetRelayVolume(AttenuatorLeft, AttenuatorRight);        // set relay to the correct level
            WriteVolumeScreen(AttenuatorMain);                      // display volume level on screen
          }
          delay(DelayTimer);
        }
        break;
      case AppleDown:
        if (InitData.Alive) {             // we only react if we are in alive state, not in standby
          DefineVolume(-1);               // calculate correct volume levels, minus 1
          if (VolumeChanged) {
            SetRelayVolume(AttenuatorLeftTmp, AttenuatorRightTmp);  //  set relays to the temp  level
            delay(DelayPlop);                                       // wait to prevent plop
            SetRelayVolume(AttenuatorLeft, AttenuatorRight);        // set relay to the correct level
            WriteVolumeScreen(AttenuatorMain);                      // display volume level on oled screen
          }
          delay(DelayTimer);
        }
        break;
      case AppleLeft:
        if (InitData.Alive) {        // we only react if we are in alive state, not in standby
          ChangeInput(-1);           // change input channel
          delay(300);
        }
        break;
      case AppleRight:
        if (InitData.Alive) {       // we only react if we are in alive state, not in standby
          ChangeInput(1);           // change input channel
          delay(300);
        }
        break;
      case AppleForward:
        ChangeStandby();            // switch status of standby
        break;
      case AppleMiddle:
        if (InitData.Alive) {       // we only react if we are in alive state, not in standby
          ChangeMute();             // change mute
          delay(300);
        }
        break;
      case AppleMenu:
        if (InitData.Alive) {        // we only react if we are in alive state, not in standby
          ChangeDirectOut();         // switch between direct out and via preamp
          delay(300);
        }
        break;
    }
    IrReceiver.resume();  // open IR receiver for next command
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// delay the startup to wait for pre amp to stabilize, clears screen at start and end of proc
void WaitForXseconds() {
 #ifdef DEBUG               // if debug enabled write message
  Serial.println(F("WaitForXseconds: waiting for preamp to stabilize "));
 #endif
  Screen.clearBuffer();                       // clear the internal memory and screen
  Screen.setFont(fontH08);                    // choose a suitable font
  Screen.setCursor(0, 8);                     // set cursur in correct position
  Screen.print(Toptekst);                     // write tekst to buffer
  Screen.setCursor(13, 63);                   // set cursur in correct position
  Screen.print(BottemTekst);                  // write tekst to buffer
  Screen.setFont(fontH10);                    // choose a suitable font
  Screen.setCursor(5, 28);                    // set cursur in correct position
  Screen.print(MiddleTekst);                  // write please wait
  for (int i = InitData.StartDelayTime; i > 0; i--) {    // run for startdelaytime times
    Screen.setDrawColor(0);                              // clean channel name  part in buffer
    Screen.drawBox(65, 31, 30, 14);
    Screen.setDrawColor(1);
    Screen.setCursor(65, 45);               // set cursur in correct position
    Screen.print(i);                          // print char array
    Screen.sendBuffer();                      // transfer internal memory to the display
    delay(1000);                              // delay for a second
  }
  Screen.clearDisplay();                      // clear screen
 #ifdef DEBUG                                 // if debug enabled write message
  Serial.println(F("WaitForXseconds: preamp wait time expired "));
 #endif
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// procedure to handle change of headphones status
void ChangeHeadphone() {
  if (InitData.HeadphoneActive) {   // headphone is active so we have to change to inactive
 #ifdef DEBUG                       // if debug enabled write message
    Serial.print(F(" ChangeHeadphone : moving from active to inactive,  "));
 #endif
    InitData.HeadphoneActive = false;                 // status off headphone active changed to false
    SetRelayVolume(0, 0);                             // switch volume off
    delay(DelayPlop);                                 // wait to stabilize
    digitalWrite(HeadphoneOnOff, LOW);                // switch headphones off
    if (InitData.PrevStatusDirectOut==true) {         // if direct out was active before we turned on headphones
      InitData.DirectOut = true;                      // make directout true
      digitalWrite(DirectOutState, HIGH);             // set relay high, amp is not active
    }
    delay(100);                                       // wait to stabilize
    DefineVolume(0);                                  // define new volume levels
    SetRelayVolume(AttenuatorLeft, AttenuatorRight);  // set volume correct level
    WriteFixedValuesScreen();                         //display info on oled screen
    WriteVolumeScreen(AttenuatorMain);                //display volume on oled screen
    EEPROM.put(0, InitData);                          // write new status to EEPROM
 #ifdef DEBUG
    Serial.println(" status is now inactive ");
 #endif
  } else {                                            // headphone is not active so we have to make it active
 #ifdef DEBUG                                         // if debug enabled write message
    Serial.print(F(" ChangeHeadphone : moving from inactive to active,  "));
 #endif
    InitData.HeadphoneActive = true;      // status headphone is true
    SetRelayVolume(0, 0);                 // switch volume off
    delay(DelayPlop);                     // wait to stabilize
    if (InitData.DirectOut) {             // amp must be active if we use headphone
      InitData.DirectOut = false;         // make directout false
      digitalWrite(DirectOutState, LOW);  // set relay low, amp is active
      delay(100);                         // wait to stabilize
      InitData.PrevStatusDirectOut=true;
    } else {
      InitData.PrevStatusDirectOut=false;
    }
    digitalWrite(HeadphoneOnOff, HIGH);  // switch headphone relay on
    delay(100);                          // wait to stabilize
    DefineVolume(0);                     // define new volume level
    if (!muteEnabled) {                  // if mute not enabled write volume to relay
      SetRelayVolume(AttenuatorLeft, AttenuatorRight);
    }
    WriteFixedValuesScreen();           //display info on oled screen
    WriteVolumeScreen(AttenuatorMain);  //display volume level on screen
    EEPROM.put(0, InitData);            // write new status to EEPROM
 #ifdef DEBUG                            // if debug enabled write message
    Serial.println(F(" status is now active "));
 #endif
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// procedure to switch between direct out and pre amp amplifier
void ChangeDirectOut() {
  if (!InitData.HeadphoneActive) {  // only allow switch if headphones is not active
    if (InitData.DirectOut) {       // Amp is in direct out state so we have to change to amp active
 #ifdef DEBUG                        // if debug enabled write message
      Serial.print(F(" ChangePassive : moving from direct out to preamp,  "));
 #endif
      InitData.DirectOut = false;         // make the boolean the correct value
      EEPROM.put(0, InitData);            // write new status to EEPROM
      SetRelayVolume(0, 0);               // switch volume off
      delay(DelayPlop);                   // wait to stabilize
      digitalWrite(DirectOutState, LOW);  // switch directout relay off
      delay(100);                         // wait to stabilize
      DefineVolume(0);                    // define new volume level
      if (!muteEnabled) {                 // if mute not enabled write volume to relay
        SetRelayVolume(AttenuatorLeft, AttenuatorRight);
      }
      WriteFixedValuesScreen();           //display info on oled screen
      WriteVolumeScreen(AttenuatorMain);  //display volume level on screen
 #ifdef DEBUG                              // if debug enabled write message
      Serial.println(F(" status is now active "));
 #endif
    } else {  // Amp is in active mode so we have to change it to passive
 #ifdef DEBUG  // if debug enabled write message
      Serial.print(F(" ChangePassive : switch from preamp amp active to direct out,  "));
 #endif
      InitData.DirectOut = true;           // make the boolean the correct value
      EEPROM.put(0, InitData);             // write new status to EEPROM
      SetRelayVolume(0, 0);                // switch volume off
      delay(DelayPlop);                    // wait to stabilize
      digitalWrite(DirectOutState, HIGH);  // switch direct out relay on
      delay(100);                          // wait to stabilize
      DefineVolume(0);                     // define new volume level
      SetRelayVolume(AttenuatorLeft, AttenuatorRight);
      WriteFixedValuesScreen();           //display info on oled screen
      WriteVolumeScreen(AttenuatorMain);  //display volume level on screen
 #ifdef DEBUG                              // if debug enabled write message
      Serial.println(F(" status is now directout "));
 #endif
    }
  }
}
///////////////////////////////////////////////////////////////////////////////////////////////////////
//// display the main setup menu on the screen
void MainSetupMenu() {
  int orgAttenuatorLevel = AttenuatorMain;  // save orginal attenuator level
  bool write = true;                        // used determine if we need to write volume level to screen
  bool quit = false;                        // determine if we should quit the loop
  bool RestoreOldVolAndChan = false;
  int choice = 5;                                                  // value of submenu choosen
  int offset = -63;                                                // ofset used to display correct vol level on screen
  unsigned long int idlePeriod = 30000;                            // idlePeriod you need to change something within menu otherwise quit menu
  unsigned long time_saved;                                        // used to help determine idle time
  if (!InitData.DirectOut) offset = offset + InitData.PreAmpGain;  // if amp not direct out add gain amp
  if (muteEnabled) ChangeMute();                                   // if mute enabled disable mute
 #ifdef DEBUG                                                       // if debug enabled write message
  Serial.println(F("setupMenu: Settings in EEPROM starting setup menu"));
  ListContentEEPROM();
 #endif
  button.loop();                               // make sure button is clean
  time_saved = millis();                       // save current time
  while (!quit) {                              // run as long as option for is not choosen
    if (millis() > time_saved + idlePeriod) {  // timeout to verify if still somebody doing something
      quit = true;
      break;
    }
    if (write) {  // write the menu
      Screen.clearBuffer();
      Screen.setFont(fontH10);
      Screen.setCursor(22, 10);
      Screen.print(F("SETUP MENU"));
      Screen.setFont(fontH08fixed);
      Screen.setCursor(0, 20);
      Screen.print(F("1 : Balance"));
      Screen.setCursor(0, 30);
      Screen.print(F("2 : Initial volume"));
      Screen.setCursor(0, 40);
      Screen.print(F("3 : Input channel name"));
      Screen.setCursor(0, 50);
      Screen.print(F("4 : General"));
      Screen.setCursor(0, 60);
      Screen.print(F("5 : Exit"));
      sprintf(VolInChar, "%i", choice);
      Screen.drawButtonUTF8(110, 60, U8G2_BTN_BW1, 0, 1, 1, VolInChar);
      Screen.sendBuffer();
    }
    write = false;                         // we assume nothing changed, so dont write menu
    if (AttenuatorChange != 0) {           // if AttenuatorChange is changed using rotary
      choice = choice + AttenuatorChange;  // change choice
      AttenuatorChange = 0;                // reset attenuatorchange
      if (choice > 5) {                    // code to keep attenuator between 1 and 4
        choice = 1;
      }
      if (choice < 1) {
        choice = 5;
      }
      write = true;           // output is changed so we have to rewrite screen
      time_saved = millis();  // save time of last change
    }
    button.loop();             // detect if button is pressed
    if (button.isPressed()) {  // choose the correct function
      if (choice == 1) SetupMenuBalance();
      if (choice == 2) {
        SetupMenuInitVol();
        RestoreOldVolAndChan = true;
      }
      if (choice == 3) SetupMenuChangeNameInputChan();
      if (choice == 4) SetupMenuGeneral();
      if (choice == 5) quit = true;
      button.loop();          // be sure button is clean
      write = true;           // output is changed so we have to rewrite screen
      time_saved = millis();  // save time of last change
    }
  }

  // we quit menu, save all data and restore sound and volume
  EEPROM.put(0, InitData);     // save new values
  EEPROM.put(110, VolLevels);  // save volume levels
  if (RestoreOldVolAndChan) {
    AttenuatorMain = orgAttenuatorLevel;  // restore orginal attenuator level
    DefineVolume(0);                      // define volume levels
    SetRelayVolume(0, 0);                 // set volume level to 0
    delay(DelayPlop);
    SetRelayChannel(InitData.SelectedInput);  // set amp on prefered channel
    delay(DelayPlop);
    SetRelayVolume(AttenuatorLeftTmp, AttenuatorRightTmp);  // set relays to the temp  level
    delay(DelayPlop);
    SetRelayVolume(AttenuatorLeft, AttenuatorRight);  // set relays to the old  level
  }
  Screen.clearBuffer();                 // clean memory screen
  WriteFixedValuesScreen();           // display info on screen
  WriteVolumeScreen(AttenuatorMain);  // display volume level on oled screen
  Screen.sendBuffer();                  // write memory to screen
 #ifdef DEBUG                          // if debug enabled write message
  Serial.println(F("setupMenu: Settings in EEPROM ending setup menu"));
  ListContentEEPROM();
 #endif
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// procedure to change the names display for the input channels
void SetupMenuChangeNameInputChan() {
  const int ShortPressTime = 1000;                                          // short time press
  const int LongPressTime = 1000;                                           // long time press
  bool write = true;                                                        // used determine if we need to write volume level to screen
  bool quit = false;                                                        // determine if we should quit the loop
  bool isPressing = false;                                                  // defines if button is pressed
  bool isLongDetected = false;                                              // defines if button is pressed long
  bool isShortDetected = false;                                             // defines if button is pressed short
  int InputChan = 1;                                                        // inputchannel
  int SelectedChar = 0;                                                     // char to be changed
  int CurCharPos = -1;                                                      // char position within CharsAvailable
  unsigned long int idlePeriod = 30000;                                     // idlePeriod you need to change something within menu otherwise quit menu
  unsigned long time_saved;                                                 // used to help determine idle time
  unsigned long pressedTime = 0;                                            // used for detecting pressed time
  unsigned long releasedTime = 0;                                           // used for detecting pressen time
  char CharsAvailable[40] = { "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890 -:" };  // list of chars available for name input channel
  Screen.clearBuffer();
  Screen.setFont(fontH10);
  Screen.setCursor(5, 10);
  Screen.print(F("Input channel name"));                                      // print header
  Screen.setFont(fontH08fixed);
  for (int i = 1; i < 5; i++) {                                             // print current names of channels
    Screen.setCursor(0, (20 + (i * 10)));
    Screen.print(InitData.InputTekst[i]);
  }
  Screen.sendBuffer();                                                        // send content to screen
  button.loop();                                                            
  time_saved = millis();
  while (!quit) {                              // loop through the different channel names
    if (millis() > time_saved + idlePeriod) {  // verify if still somebody doing something
      quit = true;
      break;
    }
    SelectedChar = 0;                            // select the first char of the channel name
    while ((!quit) && (!isLongDetected)) {       // loop through name of channel
      if (millis() > time_saved + idlePeriod) {  // verify if still somebody doing something
        quit = true;
        break;
      }
      write = true;                                                 // force to write first char
      while ((!isShortDetected) && (!quit) && (!isLongDetected)) {  // loop to change a char
        if (millis() > time_saved + idlePeriod) {                   // verify if still somebody doing something
          quit = true;
          break;
        }
        if (write) {  // if char is changed write char
          char buf[2];
          sprintf(buf, "%c", InitData.InputTekst[InputChan][SelectedChar]);  // change first char
          strcpy(VolInChar, buf);
          Screen.setDrawColor(0);  // clean letter part in buffer
          Screen.drawBox(99, 44, 16, 16);
          Screen.setDrawColor(1);
          Screen.setFont(fontH10);
          Screen.setCursor(100, 54);
          Screen.print(VolInChar);  // write letter
          Screen.setDrawColor(0);   // clean channel name part in buffer
          Screen.drawBox((5 * SelectedChar), (10 + (InputChan * 10)), 6, 11);
          Screen.setDrawColor(1);
          Screen.setFont(fontH08fixed);
          Screen.drawButtonUTF8((5 * SelectedChar), (20 + (InputChan * 10)), U8G2_BTN_INV, 0, 0, 0, VolInChar);  // write char inverse
          Screen.sendBuffer();
          for (int CharPos = 0; CharPos < 38; CharPos++) {  // detect which pos is of current char in charoptions
            if (InitData.InputTekst[InputChan][SelectedChar] == CharsAvailable[CharPos]) {
              CurCharPos = CharPos;
              break;
            }
          }
          write = false;  // all write actions done, write is false again
        }
        if (AttenuatorChange != 0) {                   // if AttenuatorChange is changed using rotary
          write = true;                                // something will change so we need to write
          CurCharPos = CurCharPos + AttenuatorChange;  // change AttenuatorLeft init
          AttenuatorChange = 0;                        // reset attenuatorchange
          if (CurCharPos > 38) {                       // code to keep curcharpos between 0 and 38
            CurCharPos = 0;
          }
          if (CurCharPos < 0) {
            CurCharPos = 38;
          }
          InitData.InputTekst[InputChan][SelectedChar] = CharsAvailable[CurCharPos];  // change the char to the new char
          time_saved = millis();
        }
        button.loop();
        if (button.isPressed()) {
          pressedTime = millis();
          isPressing = true;
          isLongDetected = false;
        }
        if (button.isReleased()) {
          isPressing = false;
          releasedTime = millis();
          long pressDuration = releasedTime - pressedTime;
          if (pressDuration < ShortPressTime) isShortDetected = true;
        }
        if (isPressing == true && isLongDetected == false) {
          long pressDuration = millis() - pressedTime;
          if (pressDuration > LongPressTime) isLongDetected = true;
        }
      }
      Screen.setDrawColor(0);  // clean channel name  part in buffer
      Screen.drawBox(0, (10 + (InputChan * 10)), 128, 12);
      Screen.setDrawColor(1);
      Screen.setCursor(0, (20 + (InputChan * 10)));
      Screen.print(InitData.InputTekst[InputChan]);
      Screen.sendBuffer();
      isShortDetected = false;
      SelectedChar++;
      if (SelectedChar > 10) SelectedChar = 0;  // only allow chars within specific range to be changed.
      button.loop();
      if (button.isPressed()) {
        pressedTime = millis();
        isPressing = true;
        isLongDetected = false;
      }
      if (button.isReleased()) {
        isPressing = false;
        releasedTime = millis();
        long pressDuration = releasedTime - pressedTime;
        if (pressDuration < ShortPressTime) isShortDetected = true;
      }
      if (isPressing == true && isLongDetected == false) {
        long pressDuration = millis() - pressedTime;
        if (pressDuration > LongPressTime) isLongDetected = true;
      }
    }
    button.loop();
    isShortDetected = false;
    isLongDetected = false;
    isPressing = false;
    SelectedChar = 0;
    InputChan++;
    if (InputChan > 4) quit = true;
    time_saved = millis();
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// change brightness, amp attenuation and start delay
void SetupMenuGeneral() {
  const int ShortPressTime = 1000;       // short time press
  const int LongPressTime = 1000;        // long time press
  bool write = true;                     // used determine if we need to write volume level to screen
  bool quit = false;                     // determine if we should quit the loop
  bool isPressing = false;               // defines if button is pressed
  bool isLongDetected = false;           // defines if button is pressed long
  bool isShortDetected = false;          // defines if button is pressed short
  unsigned long int idlePeriod = 30000;  // idlePeriod you need to change something within menu otherwise quit menu
  unsigned long time_saved;              // used to help determine idle time
  unsigned long pressedTime = 0;         // time button was pressed
  unsigned long releasedTime = 0;        // time buttons was released
  Screen.clearBuffer();                    // write menu including all variables
  Screen.setFont(fontH10);
  Screen.setCursor(42, 10);
  Screen.print(F("General"));
  Screen.setFont(fontH08fixed);
  Screen.setCursor(0, 20);
  Screen.print(F("Brightness screen"));
  Screen.setCursor(0, 31);
  Screen.print(F("Preamp gain"));
  Screen.setCursor(0, 42);
  Screen.print(F("Startup delay"));
  Screen.setCursor(110, 20);
  Screen.print(F(" "));
  Screen.print(InitData.ContrastLevel);
  Screen.setCursor(110, 31);
  if (InitData.PreAmpGain < 10) Screen.print(F(" "));
  Screen.print(InitData.PreAmpGain);
  Screen.setCursor(110, 42);
  if (InitData.StartDelayTime < 10) Screen.print(F(" "));
  Screen.print(InitData.StartDelayTime);
  Screen.sendBuffer();
  button.loop();                          // verify button is clean
  while ((!isLongDetected) && (!quit)) {  // loop this page as long as no long press and no timeout run menu
    write = true;
    time_saved = millis();
    /////////////////////////change brightness
    while ((!isShortDetected) && (!quit) && (!isLongDetected)) {  // loop to change brightness
      if (millis() > time_saved + idlePeriod) {                   // timeout to verify if still somebody doing something
        quit = true;
        break;
      }
      if (write) {             // if changed rewrite value of brightness
        Screen.setDrawColor(0);  // clean brightness part in buffer
        Screen.drawBox(108, 12, 20, 12);
        Screen.setDrawColor(1);
        strcpy(VolInChar, " ");  // convert int to char using right spacing
        char buf[2];
        sprintf(buf, "%i", InitData.ContrastLevel);
        strcat(VolInChar, buf);
        Screen.drawButtonUTF8(110, 20, U8G2_BTN_BW1, 0, 1, 1, VolInChar);  // print value using a box
        Screen.sendBuffer();                                               // copy memory to screen
      }
      write = false;                                                         // assume no changes
      if (AttenuatorChange != 0) {                                           // if AttenuatorChange is changed using rotary
        write = true;                                                        // brightness  is changed
        InitData.ContrastLevel = InitData.ContrastLevel + AttenuatorChange;  // change brightness
        AttenuatorChange = 0;                                                // reset attenuatorchange
        if (InitData.ContrastLevel > 7) {                                    // code to keep attenuator between 1 and 7
          InitData.ContrastLevel = 7;
          write = false;
        }
        if (InitData.ContrastLevel < 0) {
          InitData.ContrastLevel = 0;
          write = false;
        }
        Screen.setContrast((((InitData.ContrastLevel * 2) + 1) << 4) | 0x0f);  // set new value of brightness (1-254)
        time_saved = millis();                                               // save time of last change
      }
      button.loop();  // check if and how button is pressed
      if (button.isPressed()) {
        pressedTime = millis();
        isPressing = true;
        isLongDetected = false;
      }
      if (button.isReleased()) {
        isPressing = false;
        releasedTime = millis();
        long pressDuration = releasedTime - pressedTime;
        if (pressDuration < ShortPressTime) isShortDetected = true;
      }
      if (isPressing == true && isLongDetected == false) {
        long pressDuration = millis() - pressedTime;
        if (pressDuration > LongPressTime) isLongDetected = true;
      }
    }                      // finished changing brightness
    Screen.setDrawColor(0);  // write value to screen without box
    Screen.drawBox(108, 12, 20, 12);
    Screen.setDrawColor(1);
    Screen.setCursor(110, 20);
    Screen.print(F(" "));
    Screen.print(InitData.ContrastLevel);
    Screen.sendBuffer();
    isShortDetected = false;                                      // reset short push detected
    write = true;                                                 // set write to true so we start correctly preamp gain
    time_saved = millis();                                        // save time of last change
    /////////////////////////change amp gain
    while ((!isShortDetected) && (!quit) && (!isLongDetected)) {  // loop to change amp offset
      if (millis() > time_saved + idlePeriod) {                   // timeout to verify if still somebody doing something
        quit = true;
        break;
      }
      if (write) {             // if amp gain is changed
        Screen.setDrawColor(0);  // clean part of amp gaing in memory
        Screen.drawBox(108, 23, 20, 12);
        Screen.setDrawColor(1);
        if (InitData.PreAmpGain < 10) {  // change int into char with correct spacing
          strcpy(VolInChar, " ");
        } else {
          strcpy(VolInChar, "");
        }
        char buf[3];
        sprintf(buf, "%i", InitData.PreAmpGain);
        strcat(VolInChar, buf);
        Screen.drawButtonUTF8(110, 31, U8G2_BTN_BW1, 0, 1, 1, VolInChar);  //write amp gain with a box
        Screen.sendBuffer();
      }
      write = false;
      if (AttenuatorChange != 0) {                                     // if AttenuatorChange is changed using rotary
        write = true;                                                  // amp gain is changed
        InitData.PreAmpGain = InitData.PreAmpGain + AttenuatorChange;  // change amp gain
        AttenuatorChange = 0;                                          // reset attenuatorchange
        if (InitData.PreAmpGain > 63) {                                // code to keep attenuator between 0 and 63
          InitData.PreAmpGain = 63;
          write = false;
        }
        if (InitData.PreAmpGain < 0) {
          InitData.PreAmpGain = 0;
          write = false;
        }
        time_saved = millis();  // save time of last change
      }
      button.loop();  // check if button is pressed
      if (button.isPressed()) {
        pressedTime = millis();
        isPressing = true;
        isLongDetected = false;
      }
      if (button.isReleased()) {
        isPressing = false;
        releasedTime = millis();
        long pressDuration = releasedTime - pressedTime;
        if (pressDuration < ShortPressTime) isShortDetected = true;
      }
      if (isPressing == true && isLongDetected == false) {
        long pressDuration = millis() - pressedTime;
        if (pressDuration > LongPressTime) isLongDetected = true;
      }
    }                      // amp gain is set
    Screen.setDrawColor(0);  // clean amp gain  part in buffer
    Screen.drawBox(108, 23, 20, 12);
    Screen.setDrawColor(1);
    Screen.setCursor(110, 31);  // write value of amp gain without box in correct setup
    if (InitData.PreAmpGain < 10) Screen.print(F(" "));
    Screen.print(InitData.PreAmpGain);
    Screen.sendBuffer();
    isShortDetected = false;                                      // reset short push detected
    write = true;                                                 // set write to true so we start correctly preamp gain
    time_saved = millis();                                        // save time of last change
    /////////////////////////change delay after power on
    while ((!isShortDetected) && (!quit) && (!isLongDetected)) {  // loop to change startup delay
      if (millis() > time_saved + idlePeriod) {                   // verify if still somebody doing something
        quit = true;
        break;
      }
      if (write) {             // if amp gain is changed
        Screen.setDrawColor(0);  // clean delay part in buffer
        Screen.drawBox(108, 34, 20, 12);
        Screen.setDrawColor(1);
        if (InitData.StartDelayTime < 10) {  // change value from int into char using correct spacing
          strcpy(VolInChar, " ");
        } else {
          strcpy(VolInChar, "");
        }
        char buf[3];
        sprintf(buf, "%i", InitData.StartDelayTime);
        strcat(VolInChar, buf);
        Screen.drawButtonUTF8(110, 42, U8G2_BTN_BW1, 0, 1, 1, VolInChar);  // write value in char in a box
        Screen.sendBuffer();
      }
      write = false;
      if (AttenuatorChange != 0) {                                             // if AttenuatorChange is changed using rotary
        write = true;                                                          // delay is changed
        InitData.StartDelayTime = InitData.StartDelayTime + AttenuatorChange;  // change delay
        AttenuatorChange = 0;                                                  // reset attenuatorchange
        if (InitData.StartDelayTime > 99) {                                    // code to keep delay between 0 and 99
          InitData.PreAmpGain = 99;
          write = false;
        }
        if (InitData.StartDelayTime < 0) {
          InitData.StartDelayTime = 0;
          write = false;
        }
        time_saved = millis();  // save time of last change
      }
      button.loop();  // check if button is pressed
      if (button.isPressed()) {
        pressedTime = millis();
        isPressing = true;
        isLongDetected = false;
      }
      if (button.isReleased()) {
        isPressing = false;
        releasedTime = millis();
        long pressDuration = releasedTime - pressedTime;
        if (pressDuration < ShortPressTime) isShortDetected = true;
      }
      if (isPressing == true && isLongDetected == false) {
        long pressDuration = millis() - pressedTime;
        if (pressDuration > LongPressTime) isLongDetected = true;
      }
    }                      // start delay is set
    Screen.setDrawColor(0);  // clean start delay part in buffer
    Screen.drawBox(108, 34, 20, 12);
    Screen.setDrawColor(1);
    Screen.setCursor(110, 42);  // write value delay to buffer without box
    if (InitData.StartDelayTime < 10) Screen.print(F(" "));
    Screen.print(InitData.StartDelayTime);
    Screen.sendBuffer();
    isShortDetected = false;  // reset short push detected
  }
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////
//
void SetupMenuInitVol() {
  const int ShortPressTime = 1000;                                 // short time press
  const int LongPressTime = 1000;                                  // long time press
  bool write = true;                                               // used determine if we need to write volume level to screen
  bool quit = false;                                               // determine if we should quit the loop
  bool isPressing = false;                                         // defines if button is pressed
  bool isLongDetected = false;                                     // defines if button is pressed long
  bool isShortDetected = false;                                    // defines if button is pressed short
  int offset = -63;                                                // offset to define vol level shown on screen
  unsigned long int idlePeriod = 30000;                            // idlePeriod you need to change something within menu otherwise quit menu
  unsigned long time_saved;                                        // used to help determine idle time
  unsigned long pressedTime = 0;                                   // time button was pressed
  unsigned long releasedTime = 0;                                  // time button was released
  if (!InitData.DirectOut) offset = offset + InitData.PreAmpGain;  // determine offset between internal volume and vol displayed on screen
  Screen.clearBuffer();
  Screen.setFont(fontH10);
  Screen.setCursor(38, 10);
  Screen.print(F("Init volume"));  // write header
  button.loop();
  time_saved = millis();
  while ((!isLongDetected) && (!quit)) {       // loop this page as long as rotary button not long pressed and action is detected
    if (millis() > time_saved + idlePeriod) {  // verify if still somebody doing something
      quit = true;
      break;
    }
    write = true;  // something changed
    Screen.setFont(fontH08fixed);
    while ((!isShortDetected) && (!quit) && (!isLongDetected)) {  // loop to determine  volume/channel or general volume
      if (millis() > time_saved + idlePeriod) {                   // verify if still somebody doing something
        quit = true;
        break;
      }
      Screen.setCursor(0, 20);
      Screen.print(F("Volume per channel"));                             // write line defining what to change
      if (write) {                                                     // if something changed
        if (InitData.VolPerChannel) {                                  // if we have vol/channel
          Screen.drawButtonUTF8(110, 20, U8G2_BTN_BW1, 0, 1, 1, "YES");  // write yes in a box
          for (int i = 1; i < 5; i++) {                                // write the port description and the current levels
            Screen.setCursor(0, (20 + (i * 10)));
            Screen.print(InitData.InputTekst[i]);
            Screen.setCursor(110, (20 + (i * 10)));
            Screen.print(ChVolInChar3(VolLevels[i] + offset));
          }
        } else {  // if we have a general volume level
          Screen.setCursor(110, 24);
          Screen.drawButtonUTF8(110, 20, U8G2_BTN_BW1, 0, 1, 1, " NO");  // write no in a box
          Screen.setCursor(0, 31);                                       // write general description and the current level
          Screen.print(InitData.InputTekst[0]);
          Screen.setCursor(110, 31);
          Screen.print(ChVolInChar3(VolLevels[0] + offset));
        }
        Screen.sendBuffer();  // copy memory to screen
      }
      write = false;
      if (AttenuatorChange != 0) {                           // if rotary is turned
        InitData.VolPerChannel = (!InitData.VolPerChannel);  // invert the setting if we use vol/channel yes no
        AttenuatorChange = 0;                                // reset AttenuatorChange
        Screen.setDrawColor(0);                                // clean memory part of yes/no of volume per channel and rest of screen
        Screen.drawBox(0, 14, 127, 50);
        Screen.drawBox(108, 12, 20, 12);
        Screen.setDrawColor(1);
        write = true;
        time_saved = millis();  // save time of last change
      }
      button.loop();  // check if button is pressed
      if (button.isPressed()) {
        pressedTime = millis();
        isPressing = true;
        isLongDetected = false;
      }
      if (button.isReleased()) {
        isPressing = false;
        releasedTime = millis();
        long pressDuration = releasedTime - pressedTime;
        if (pressDuration < ShortPressTime) isShortDetected = true;
      }
      if (isPressing == true && isLongDetected == false) {
        long pressDuration = millis() - pressedTime;
        if (pressDuration > LongPressTime) isLongDetected = true;
      }
    }                      // volume/channel is set
    Screen.setDrawColor(0);  // clean volume part in buffer
    Screen.drawBox(108, 12, 20, 12);
    Screen.setDrawColor(1);
    Screen.setFont(fontH08fixed);
    if (InitData.VolPerChannel) {  // write correct value without box
      Screen.setCursor(110, 20);
      Screen.print("YES");
    } else {
      Screen.setCursor(110, 20);
      Screen.print(" NO");
    }
    Screen.sendBuffer();
    isShortDetected = false;
    ///////////////  set values per channel or generic volume
    if (InitData.VolPerChannel) {                  // we have volume / channel
      for (int i = 1; i < 5; i++) {                // run loop for  4 volumes per channel
        if (millis() > time_saved + idlePeriod) {  // verify if still somebody doing something
          quit = true;
          break;
        }
        if ((!quit) && (!isLongDetected)) {  // only when we not quiting the for loop, if we quit commands are not needed
          write = true;
          SetRelayVolume(0, 0);
          delay(DelayPlop);
          SetRelayChannel(i);  // select the correct input channel
        }
        while ((!isShortDetected) && (!quit) && (!isLongDetected)) {  // loop to set value for a specific channel
          if (millis() > time_saved + idlePeriod) {                   // timeout to verify if still somebody doing something
            quit = true;
            break;
          }
          if (write) {                                              // if volume level changed
            AttenuatorMain = VolLevels[i];                          // set the main volume to the level of the channel
            DefineVolume(0);                                        // calculate vol levels
            SetRelayVolume(AttenuatorLeftTmp, AttenuatorRightTmp);  //  set relays to the temp  level
            delay(DelayPlop);
            SetRelayVolume(AttenuatorLeft, AttenuatorRight);  // set relays to final level
            Screen.setDrawColor(0);                             // clean volume part in buffer
            Screen.drawBox(108, (12 + (i * 10)), 20, 12);
            Screen.setDrawColor(1);
            Screen.drawButtonUTF8(110, (20 + (i * 10)), U8G2_BTN_BW1, 0, 1, 1, (ChVolInChar3(VolLevels[i] + offset)));  // write vol level in box
            Screen.sendBuffer();
          }
          write = false;
          if (AttenuatorChange != 0) {  // if AttenuatorChange is changed using rotary
            write = true;
            VolLevels[i] = VolLevels[i] + AttenuatorChange;  // change volume level of specific channel
            AttenuatorChange = 0;                            // reset attenuatorchange
            if (VolLevels[i] > 63) {                         // code to keep attenuator between 0 and 63
              VolLevels[i] = 63;
              write = false;
            }
            if (VolLevels[i] < 0) {
              VolLevels[i] = 0;
              write = false;
            }
            time_saved = millis();  // save time of last change
          }
          button.loop();  // check if button is pressed
          if (button.isPressed()) {
            pressedTime = millis();
            isPressing = true;
            isLongDetected = false;
          }
          if (button.isReleased()) {
            isPressing = false;
            releasedTime = millis();
            long pressDuration = releasedTime - pressedTime;
            if (pressDuration < ShortPressTime) isShortDetected = true;
          }
          if (isPressing == true && isLongDetected == false) {
            long pressDuration = millis() - pressedTime;
            if (pressDuration > LongPressTime) isLongDetected = true;
          }
        }                      // volume level for specific channel is set
        Screen.setDrawColor(0);  // clean volume part in buffer
        Screen.drawBox(108, (12 + (i * 10)), 20, 12);
        Screen.setDrawColor(1);
        Screen.setCursor(110, (20 + (i * 10)));
        Screen.print(ChVolInChar3(VolLevels[i] + offset));  // write correct volume level without box around it
        Screen.sendBuffer();
        isShortDetected = false;
      }
    } else {  // no volume/channel, so we set generic volume
      write = true;
      while ((!isShortDetected) && (!quit) && (!isLongDetected)) {
        if (millis() > time_saved + idlePeriod) {  // timeout to verify if still somebody doing something
          quit = true;
          break;
        }
        if (write) {                                              // value is changed
          AttenuatorMain = VolLevels[0];                          // set main volume level to generic volume level
          DefineVolume(0);                                        // calculate volume levels
          SetRelayVolume(AttenuatorLeftTmp, AttenuatorRightTmp);  // set relays to the temp  level
          delay(DelayPlop);
          SetRelayVolume(AttenuatorLeft, AttenuatorRight);  // set relays to final version
          Screen.setDrawColor(0);                             // clean volume part in buffer
          Screen.drawBox(108, 22, 20, 12);
          Screen.setDrawColor(1);
          Screen.drawButtonUTF8(110, 31, U8G2_BTN_BW1, 0, 1, 1, (ChVolInChar3(VolLevels[0] + offset)));  // write new volume level in a box
          Screen.sendBuffer();
        }
        write = false;
        if (AttenuatorChange != 0) {                       // if AttenuatorChange is changed using rotary
          write = true;                                    // volume level is changed
          VolLevels[0] = VolLevels[0] + AttenuatorChange;  // change volume level
          AttenuatorChange = 0;                            // reset attenuatorchange
          if (VolLevels[0] > 63) {                         // code to keep attenuator between 0 and 63
            VolLevels[0] = 63;
            write = false;
          }
          if (VolLevels[0] < 0) {
            VolLevels[0] = 0;
            write = false;
          }
          time_saved = millis();
        }
        button.loop();  // check if button is pressed
        if (button.isPressed()) {
          pressedTime = millis();
          isPressing = true;
          isLongDetected = false;
        }
        if (button.isReleased()) {
          isPressing = false;
          releasedTime = millis();
          long pressDuration = releasedTime - pressedTime;
          if (pressDuration < ShortPressTime) isShortDetected = true;
        }
        if (isPressing == true && isLongDetected == false) {
          long pressDuration = millis() - pressedTime;
          if (pressDuration > LongPressTime) isLongDetected = true;
        }
      }                      // volume level for generic is set
      Screen.setDrawColor(0);  // clean volume part in buffer
      Screen.drawBox(108, 22, 20, 12);
      Screen.setDrawColor(1);
      Screen.setCursor(110, 31);
      Screen.print(ChVolInChar3(VolLevels[0] + offset));  // write correct volume level without box around it
      Screen.sendBuffer();
      isShortDetected = false;
    }
  }
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  set balance value using menu
void SetupMenuBalance() {
  int BalanceRight;                                                // balance value right channel
  int BalanceLeft;                                                 // balance value left channel
  const int LongPressTime = 500;                                   // long time press
  bool write = true;                                               // used determine if we need to write volume level to screen
  bool quit = false;                                               // determine if we should quit the loop
  bool isPressing = false;                                         // defines if button is pressed
  bool isLongDetected = false;                                     // defines if button is pressed long
  int offset = -63;                                                // offset to define vol level shown on screen
  unsigned long int idlePeriod = 30000;                            // idlePeriod you need to change something within menu otherwise quit menu
  unsigned long time_saved;                                        // used to help determine idle time
  unsigned long pressedTime = 0;                                   // time button is pressed
  button.loop();                                                   // reset status of button
  if (!InitData.DirectOut) offset = offset + InitData.PreAmpGain;  // calculate correct value of offset of internal vol level
  Screen.setFont(fontH10);                                           // set font
  Screen.clearBuffer();
  if (!DaughterBoard) {                                            // we need 2 boards to set balance.
    Screen.setCursor(0, 30);
    Screen.print(F("daughterboard missing"));
    Screen.sendBuffer();
    delay(2000);    
  } else {  // we have 2 boards
    Screen.setCursor(44, 10);
    Screen.print(F("balance"));                                      // print heading
    Screen.setCursor(11, 38);
    Screen.print(F("-------------+-------------"));
    Screen.setCursor(0, 63);
    Screen.print(F("L:               dB       R:"));
    write = true;                                                  // we assume something changed to write first time value
    time_saved = millis();                                         // save time last change
    while ((!isLongDetected) && (!quit)) {                         // loop to set balance as long not quit and button not long pressed
      if (millis() > time_saved + idlePeriod) {                    // verify if still somebody doing something
        quit = true;
        break;
      }
      if (write) {                                                        // balance is changed
        Screen.drawGlyph((63 + (InitData.BalanceOffset * 4)), 50, 0x005E);  // write current balance using position
        BalanceRight = (InitData.BalanceOffset >> 1);                     // divide balanceOffset using a bitshift to the right
        BalanceLeft = BalanceRight - InitData.BalanceOffset;              // difference is balance left
        Screen.setFont(fontH10figure);
        Screen.setDrawColor(0);  // clean balance part in buffer
        Screen.drawBox(16, 52, 24, 11);
        Screen.drawBox(110, 52, 24, 11);
        Screen.setDrawColor(1);
        Screen.setCursor(15, 63);  // write balance values into screen memory
        Screen.print(ChVolInChar2(BalanceLeft));
        Screen.setCursor(110, 63);
        Screen.print(ChVolInChar2(BalanceRight));
        Screen.sendBuffer();  // copy memory to screen
      }
      write = false;
      if (AttenuatorChange != 0) {  // if AttenuatorChange is changed using rotary
        write = true;               // balance is changed
        Screen.setFont(fontH10);
        Screen.setDrawColor(0);  // clean balance pointer in part in buffer
        Screen.drawBox((63 + (InitData.BalanceOffset * 4)), 39, 9, 8);
        Screen.setDrawColor(1);
        InitData.BalanceOffset = InitData.BalanceOffset + AttenuatorChange;  // calculate new balance offset
        AttenuatorChange = 0;                                                // reset value
        if (InitData.BalanceOffset > 14) {                                   // keep value between 10 and -10
          InitData.BalanceOffset = 14;
          write = false;  // value was already 14, no use to write
        }
        if (InitData.BalanceOffset < -14) {
          InitData.BalanceOffset = -14;
          write = false;  // value was alread -14, no use to write
        }
        Screen.drawGlyph((63 + (InitData.BalanceOffset * 4)), 50, 0x005E);  // write current balance pointer using position
        DefineVolume(0);
        SetRelayVolume(AttenuatorLeftTmp, AttenuatorRightTmp);  //  set relays to the temp  level
        delay(DelayPlop);
        SetRelayVolume(AttenuatorLeft, AttenuatorRight);  // set new volume level with balance level
        time_saved = millis();                            // set time to last change
      }
      button.loop();  // check if button is pressed
      if (button.isPressed()) {
        pressedTime = millis();
        isPressing = true;
        isLongDetected = false;
      }
      if (button.isReleased()) {
        isPressing = false;
      }
      if (isPressing == true && isLongDetected == false) {
        long pressDuration = millis() - pressedTime;
        if (pressDuration > LongPressTime) isLongDetected = true;
      }
    }
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// procedure to handle moving to and coming out of standby mode
void ChangeStandby() {
  if (InitData.Alive) {  // move from alive to standby, turn off screen, relays and amp
 #ifdef DEBUG    // if debug enabled write message
    Serial.print(F(" ChangeStandby : moving to standby,  "));
 #endif
    digitalWrite(LedStandby, HIGH);    // turn on standby led to indicate device is in standby
    InitData.Alive = false;            // we are in standby, alive is false
    EEPROM.put(0, InitData);           // write new status to EEPROM
    SetRelayVolume(0, 0);              // set volume relays off
    delay(DelayPlop);                  // wait to stabilize
    SetRelayChannel(0);                // disconnect all input channels
    delay(100);                        // wait to stabilze
    digitalWrite(StartDelay, LOW);     // disconnect amp from output
    delay(100);                        // wait to stabilze
    digitalWrite(DirectOutState, LOW); // switch relay off
    digitalWrite(HeadphoneOnOff, LOW); // switch relay off
    Screen.clearDisplay();               // clear display
    Screen.setPowerSave(1);              // turn screen off
    digitalWrite(PowerOnOff, LOW);     // make pin of power on low, power of amp is turned off
 #ifdef DEBUG                         // if debug enabled write message
    Serial.println(F(" status is now standby "));
 #endif
  } else {
 #ifdef DEBUG  // if debug enabled write message
    Serial.print(F(" ChangeStandby : moving to active,  "));
 #endif
    InitData.Alive = true;                    // preamp is alive
    EEPROM.put(0, InitData);                  // write new status to EEPROM
    digitalWrite(LedStandby, LOW);            // turn off standby led to indicate device is powered on
    digitalWrite(PowerOnOff, HIGH);           // make pin of standby high, power of amp is turned on
    delay(1000);                              // wait to stabilize
    Screen.setPowerSave(0);                     // turn screen on
    WaitForXseconds();                        // wait to let amp warm up
    digitalWrite(StartDelay, HIGH);           // connect amp to output
    delay(100);                               // wait to stabilize
    if (InitData.DirectOut) {                 // if DirectOut is active set port high to active relay
      digitalWrite(DirectOutState, HIGH);     // turn the preamp in directout mode
    }
    SetRelayChannel(InitData.SelectedInput);  // select correct input channel
    delay(DelayPlop);                         // wait to stabilize
    if (InitData.HeadphoneActive) {           // headphone is active
     digitalWrite(HeadphoneOnOff, HIGH);      // switch headphone relay on
    }
    if (InitData.VolPerChannel) {                          // if we have a dedicated volume level per channel give attenuatorlevel the correct value
     AttenuatorMain = VolLevels[InitData.SelectedInput];   // if vol per channel select correct level
    } else {                                               // if we dont have the a dedicated volume level per channel
     AttenuatorMain = VolLevels[0];                        // give attenuator level the generic value
    }
    DefineVolume(0);                          // define new volume level
    if (!muteEnabled) {                       // if mute not enabled write volume to relay
      SetRelayVolume(AttenuatorLeft, AttenuatorRight);
    }
    WriteFixedValuesScreen();                 //display info on oled screen
    WriteVolumeScreen(AttenuatorMain);        // display volume level on oled screen
 #ifdef DEBUG                                 // if debug enabled write message
    Serial.println(F(" status is now active "));
 #endif
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Interrupt Service Routine for a change to Rotary Encoder pin A
void RotaryTurn() {
  if (digitalRead(rotaryPinB)) {
    AttenuatorChange = AttenuatorChange - 1;  // rotation left, turn down
  } else {
    AttenuatorChange = AttenuatorChange + 1;  // rotatate right, turn Up!
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function to change to next inputchannel
void ChangeInput(int change) {
 #ifdef DEBUG  // if debug enabled write message                                                
  Serial.print(F(" ChangeInput: old selected Input: "));
  Serial.print(InitData.SelectedInput);
 #endif
  if (muteEnabled) {  // if mute enabled and change volume change mute status to off
    ChangeMute();     // disable mute
  }
  InitData.SelectedInput = InitData.SelectedInput + change;        // increase or decrease current channel by 1
  if (InitData.SelectedInput > 4) { InitData.SelectedInput = 1; }  // implement round robbin for channel number
  if (InitData.SelectedInput < 1) { InitData.SelectedInput = 4; }
  EEPROM.put(0, InitData);                               // save new channel number in eeprom
  if (InitData.VolPerChannel) {                          // if we have a dedicated volume level per channel give attenuatorlevel
    AttenuatorMain = VolLevels[InitData.SelectedInput];  // if vol per channel select correct level
  }
  SetRelayVolume(0, 0);                     // set volume to zero
  delay(DelayPlop);                         // wait  to stabilize
  SetRelayChannel(InitData.SelectedInput);  // set relays to new input channel
  delay(100);                               // wait to stabilize
  DefineVolume(0);                          // define new volume level
  if (!muteEnabled) {                       // if mute not enabled write volume to relay
    SetRelayVolume(AttenuatorLeft, AttenuatorRight);
  }
  WriteFixedValuesScreen();                   //display info on oled screen
  WriteVolumeScreen(AttenuatorMain);          // update screen
 #ifdef DEBUG                                 // if debug enabled write message
  Serial.print(F(", new Selected Input: "));  // write new channel number to debug
  Serial.println(InitData.SelectedInput);
 #endif
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Functions to change volume
void DefineVolume(int increment) {
  int AttenuatorLeftOld = AttenuatorLeft;    // save old value left channel
  int AttenuatorRightOld = AttenuatorRight;  // save old value right channel
  VolumeChanged = true;                      // assume volume is changed
  if ((muteEnabled) && (increment != 0)) {   // if mute enabled and change volume change mute status to off
    ChangeMute();                            // disable mute
  }
 #ifdef DEBUG  // if debug enabled write message
  Serial.print(F("DefineVolume: volume was : "));
  Serial.println(AttenuatorMain);
 #endif
  AttenuatorMain = AttenuatorMain + increment;  // define new attenuator level
  if (AttenuatorMain > 63) {                    // volume cant be higher as 63
    AttenuatorMain = 63;
    VolumeChanged = false;  //as attenuator level was already 63 no use to write
  }
  if (AttenuatorMain < 0) {  // volume cant be lower as 0
    AttenuatorMain = 0;
    VolumeChanged = false;  //as attenuator level was already 0 no use to write
  }
  if (VolumeChanged) {          // als volume veranderd is
    if (AttenuatorMain == 0) {  // If volume=0  both left and right set to 0
      AttenuatorRight = 0;
      AttenuatorLeft = 0;
    } else if (!DaughterBoard) {  // if  no daughterboard we ignore balance
      AttenuatorRight = AttenuatorMain;
      AttenuatorLeft = AttenuatorMain;
    } else {                                                      //amp in normal state, we use balance
      int BalanceRight = ((InitData.BalanceOffset) >> 1);         // divide balanceOffset using a bitshift to the right
      int BalanceLeft = BalanceRight - (InitData.BalanceOffset);  // balance left is the remaining part of
      AttenuatorRight = AttenuatorMain + BalanceRight;            // correct volume right with balance
      AttenuatorLeft = AttenuatorMain + BalanceLeft;              // correct volume left with balance
      if (AttenuatorRight > 63) { AttenuatorRight = 63; }         // volume higher as 63 not allowed
      if (AttenuatorLeft > 63) { AttenuatorLeft = 63; }           // volume higher as 63 not allowed
      if (AttenuatorRight < 0) { AttenuatorRight = 0; }           // volume lower as 0 not allowed
      if (AttenuatorLeft < 0) { AttenuatorLeft = 0; }             // volume lower as 0 not allowed
    }
    AttenuatorLeftTmp = AttenuatorLeft & AttenuatorLeftOld;       //define intermediate volume levels to prevent plop
    AttenuatorRightTmp = AttenuatorRight & AttenuatorRightOld;    //define intermediate volume levels to prevent plop
 #ifdef DEBUG                                                      // if debug enabled write message
    Serial.print(F("DefineVolume: volume is now : "));
    Serial.print(AttenuatorMain);
    Serial.print(F(" volume left = "));
    Serial.print(AttenuatorLeft);
    Serial.print(F(". Bytes, old, temp, new "));
    Serial.print(AttenuatorLeftOld, BIN);
    Serial.print(" , ");
    Serial.print(AttenuatorLeftTmp, BIN);
    Serial.print(" , ");
    Serial.print(AttenuatorLeft, BIN);
    Serial.print(F(" ,volume right = "));
    Serial.print(AttenuatorRight);
    Serial.print(F(".  Bytes, old, temp, new "));
    Serial.print(AttenuatorRightOld, BIN);
    Serial.print(" , ");
    Serial.print(AttenuatorRightTmp, BIN);
    Serial.print(" , ");
    Serial.print(AttenuatorRight, BIN);
    Serial.print(F(",  headphones : "));
    Serial.println(InitData.HeadphoneActive);
 #endif
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Function to change mute
void ChangeMute() {
  if (muteEnabled) {                          // if mute enable we turn mute off
    muteEnabled = false;                      // change value of boolean
    SetRelayChannel(InitData.SelectedInput);  // set relays to support original inputchnannel
    delay(100);
    DefineVolume(0);                          // define new volume level
    SetRelayVolume(AttenuatorLeft, AttenuatorRight);
    WriteFixedValuesScreen();                 //display info on  screen
    WriteVolumeScreen(AttenuatorMain);
 #ifdef DEBUG  // if debug enabled write message
    Serial.print(F("ChangeMute : Mute now disabled, setting volume to: "));
    Serial.println(AttenuatorMain);
 #endif
  } else {
    muteEnabled = true;        // mute is not enabled so we turn mute on
    SetRelayVolume(0, 0);      // switch volume off
    delay(DelayPlop);          // set relays to zero volume
    SetRelayChannel(0);        // set relays to no input channel
    WriteFixedValuesScreen();  //display info on oled screen
    WriteVolumeScreen(0);
 #ifdef DEBUG                  // if debug enabled write message
    Serial.println(F("ChangeMute: Mute enabled"));
 #endif
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// write stable values on the screen (mute, headphones, passive)
void WriteFixedValuesScreen() {
  Screen.clearDisplay();
  Screen.setFont(fontgrahp);
  if (InitData.HeadphoneActive) {             // headphone is active
    Screen.drawGlyph(0, 36, 0x0043);            // write headphone symbol
  } else if (InitData.DirectOut) {            // amp is direct out
    Screen.drawTriangle(8, 16, 18, 26, 8, 36);  // write amp sign
    Screen.drawLine(2, 26, 22, 26);
    Screen.setDrawColor(0);
    Screen.drawTriangle(10, 21, 15, 26, 10, 31);
    Screen.setDrawColor(1);
    Screen.drawLine(2, 35, 19, 18);
    Screen.drawLine(2, 34, 19, 17);
  } else {  // amp is active write amp sign
    Screen.drawTriangle(8, 16, 18, 26, 8, 36);
    Screen.drawLine(2, 26, 22, 26);
    Screen.setDrawColor(0);
    Screen.drawTriangle(10, 21, 15, 26, 10, 31);
    Screen.setDrawColor(1);
  }
  Screen.setFont(fontH14);  // write inputchannel
  Screen.setCursor(0, 63);
  Screen.print(InitData.InputTekst[InitData.SelectedInput]);
  if (InitData.BalanceOffset != 0) {                   // write balance values
    int BalanceRight = (InitData.BalanceOffset >> 1);  // divide balanceOffset using a bitshift to the right
    int BalanceLeft = BalanceRight - InitData.BalanceOffset;
    Screen.setDrawColor(0);                              // clean balance part in buffer
    Screen.drawBox(0, 0, 128, 10);
    Screen.setDrawColor(1);
    Screen.setFont(fontH10);
    Screen.setCursor(0, 10);
    Screen.print("L:");
    Screen.setFont(fontH10figure);
    Screen.print(ChVolInChar2(BalanceLeft));
    Screen.setFont(fontH10);
    Screen.setCursor(98, 10);
    Screen.print("R:");
    Screen.setFont(fontH10figure);
    Screen.print(ChVolInChar2(BalanceRight));
    Screen.sendBuffer();
  } else {
    Screen.setDrawColor(0);                               // clean balance part in buffer
    Screen.drawBox(0, 0, 128, 10);
    Screen.setDrawColor(1);
  }
  Screen.sendBuffer();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// intialisation of the screen after powerup of screen.
void OledSchermInit() {
  digitalWrite(OledReset, LOW);                                        // set screen in reset mode
  delay(15);                                                           // wait to stabilize
  digitalWrite(OledReset, HIGH);                                       // set screen active
  delay(15);                                                           // wait to stabilize
  Screen.setI2CAddress(OLED_Address * 2);                                // set oled I2C address
  Screen.begin();                                                        // init the screen
  Screen.setContrast((((InitData.ContrastLevel * 2) + 1) << 4) | 0x0f);  // set contrast level, reduce number of options
  Screen.sendBuffer();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// write volume level to screen
void WriteVolumeScreen(int volume) {
  int offset = -63;
  if (!InitData.DirectOut) {  // if the amp is not in direct out mode adjust volume displayed on screen
    offset = offset + InitData.PreAmpGain;
  }
  volume = volume + offset;  // change volumelevel shown on screen, adding offset
  Screen.setDrawColor(0);      // clean volume part in buffer
  Screen.drawBox(35, 13, 80, 31);
  Screen.setDrawColor(1);
  if (muteEnabled) {  // if mute is enabled write MUTE
    Screen.setFont(fontH14);
    Screen.drawStr(35, 34, "MUTE");
  } else {                        // mute is not enabled, write volume on screen.
    Screen.setFont(fontH21cijfer);  // write volume
    Screen.setCursor(35, 37);
    Screen.print(ChVolInChar3(volume));
    Screen.setFont(fontH10);
    Screen.drawStr(87, 37, "dB");
  }
  Screen.sendBuffer();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// set relays in status to support requested volume
void SetRelayVolume(int RelayLeft, int RelayRight) {
  Wire.beginTransmission(MCP23017_I2C_ADDRESS_bottom);  // write volume left to left ladder
  Wire.write(0x13);
  Wire.write(RelayLeft);
  Wire.endTransmission();
  if (DaughterBoard) {                                 // if we have second board
    Wire.beginTransmission(MCP23017_I2C_ADDRESS_top);  // write volume right to right ladder
    Wire.write(0x13);
    Wire.write(RelayRight);
    Wire.endTransmission();
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// set relays in status to support requested channnel
void SetRelayChannel(uint8_t relay) {
  uint8_t inverseWord;
  if (relay == 0) {      // if no channel is selected, drop all ports, this is done during  mute
    inverseWord = 0xef;  // we write inverse, bit 4 = 0 as this one is not inverted
  } else {
    inverseWord = 0xFF ^ (0x01 << (relay - 1));                           // bitshift the number of ports to get a 1 at the correct port and inverse word
    if ((1 == bitRead(InputPortType, (relay - 1))) && (DaughterBoard)) {  // determine if this is an xlr port
      bitClear(inverseWord, 4);                                           // if XLR set bit 4 to 0
    }
  }
  Wire.beginTransmission(MCP23017_I2C_ADDRESS_bottom);  // write port settings to board
  Wire.write(0x12);
  Wire.write(inverseWord);
  Wire.endTransmission();
  if (DaughterBoard) {                                 // if we have an additional board
    Wire.beginTransmission(MCP23017_I2C_ADDRESS_top);  // write port settings to top board (right channel)
    Wire.write(0x12);
    Wire.write(inverseWord);
    Wire.endTransmission();
  }
 #ifdef DEBUG  // if debug enabled write message
  Serial.print(F("SetRelayChannel: bytes written to relays : "));
  Serial.println(inverseWord, BIN);
 #endif
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//initialize the MCP23017 controling the relays;
void MCP23017init(uint8_t MCP23017_I2C_ADDRESS) {
  Wire.beginTransmission(MCP23017_I2C_ADDRESS);
  Wire.write(0x00);  // IODIRA register, input port
  Wire.write(0x00);  // set all of port B to output
  Wire.endTransmission();
  Wire.beginTransmission(MCP23017_I2C_ADDRESS);
  Wire.write(0x12);  // gpioA
  Wire.write(0xEF);  // set all ports high except bit 4
  Wire.endTransmission();
  Wire.beginTransmission(MCP23017_I2C_ADDRESS);
  Wire.write(0x01);  // IODIRB register, volume level
  Wire.write(0x00);  // set all of port A to outputs
  Wire.endTransmission();
  Wire.beginTransmission(MCP23017_I2C_ADDRESS);
  Wire.write(0x13);  // gpioB
  Wire.write(0x00);  // set all ports low,
  Wire.endTransmission();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// detect log time press on remote controle
bool DetectLongPress(uint16_t aLongPressDurationMillis) {
  if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) {        // if repeat and not detected yet
    if (millis() - aLongPressDurationMillis > sMillisOfFirstReceive) {  // if this status takes longer as..
      sLongPressJustDetected = true;                                    // longpress detected
    }

  } else {                             // no longpress detected
    sMillisOfFirstReceive = millis();  // reset value of first press
    sLongPressJustDetected = false;    // set boolean
  }
  return sLongPressJustDetected;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// detect if the EEPROM contains an init config
void CheckIfEepromHasInit() {
  char UniqueString[9] = "PreAmpV3";                            // unique string to check if eeprom already written
  EEPROM.get(0, InitData);                                      // get variables within init out of EEPROM
  for (byte index = 1; index < 9; index++) {                    // loop to compare strings
    if (InitData.UniqueString[index] != UniqueString[index]) {  // compare if strings differ, if so
      WriteEEprom();                                            // write eeprom
      break;                                                    // jump out of for loop
    }
 #ifdef DEBUG
    if (index == 8) {  // index 8 is only possible if both strings are equal
      Serial.println(F("CheckIfEepromHasInit: init configuration found"));
    }
 #endif
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// write the EEProm with the correct values
void WriteEEprom() {
 #ifdef DEBUG
  Serial.println(F("WriteEEprom: no init config detected, writing new init config"));
 #endif
  SavedData start = {
    "PreAmpV3",  // unique string
    false,       // boolean, volume level per channel
    1,           // channel used for start
    0,           // balance offset
    5,           // contrastlevel
    10,          // volume offset
    10,          // delay after power on to stabilize amp
    false,       // headphones active
    false,       // direct out active
    false,       // prev state of direct out
    true,        // amp is alive, not in standby
    "Initial volume",
    "XLR  1     ",
    "XLR  2     ",
    "RCA  1     ",
    "RCA  2     "
  };
  int VolLevelsInit[5] = {
    // array of volumelevels if volumelevel per channel is active
    30,  //Attenuator generic
    30,  //init volume Ch1
    30,  //init volume Ch2
    30,  //init volume Ch2
    30   //init volume Ch4
  };
  EEPROM.put(0, start);            // write init data to eeprom
  EEPROM.put(110, VolLevelsInit);  // write volslevel to eeprom
}
////////////////////////////////////////////////////////////////////////////////////////////////
// change format of volume for displaying on screen, 2 chars
char* ChVolInChar2(int volume) {
  if (volume == 0) {
    strcpy(VolInChar, " 0");
    return (VolInChar);
  } else {
    if (volume < 0) strcpy(VolInChar, "-");
    if (volume > 0) strcpy(VolInChar, "+");
    char buf[2];
    sprintf(buf, "%i", abs(volume));
    strcat(VolInChar, buf);
    return (VolInChar);
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////
// change format of volume for displaying on screen, 3 chars
char* ChVolInChar3(int volume) {
  if (volume == 0) {
    strcpy(VolInChar, "  0");
    return (VolInChar);
  } else {
    if (volume < 0) strcpy(VolInChar, "-");
    if (volume > 0) strcpy(VolInChar, "+");
    if ((volume > -10) && (volume < 10)) {
      strcat(VolInChar, " ");
      char buf[2];
      sprintf(buf, "%i", abs(volume));
      strcat(VolInChar, buf);
      return (VolInChar);
    } else {
      char buf[3];
      sprintf(buf, "%i", abs(volume));
      strcat(VolInChar, buf);
      return (VolInChar);
    }
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////
//  debug proc to show content of eeprom
#ifdef DEBUG
 void ListContentEEPROM() {
  Serial.print(F("unique string         : "));
  Serial.println(InitData.Uniquestring);
  Serial.print(F("volume per channel    : "));
  Serial.println(InitData.VolPerChannel);
  Serial.print(F("selected input chan   : "));
  Serial.println(InitData.SelectedInput);
  Serial.print(F("Balance offset        : "));
  Serial.println(InitData.BalanceOffset);
  Serial.print(F("Contrastlevel screen  : "));
  Serial.println(InitData.ContrastLevel);
  Serial.print(F("Volume offset         : "));
  Serial.println(InitData.PreAmpGain);
  Serial.print(F("startup delay         : "));
  Serial.println(InitData.StartDelayTime);
  Serial.print(F("Headphones active     : "));
  Serial.println(InitData.HeadphoneActive);
  Serial.print(F("DirectOut active      : "));
  Serial.println(InitData.DirectOut);
  Serial.print(F("Prev stat DirectOut   : "));
  Serial.println(InitData.PrevStatusDirectOut);
  Serial.print(F("Pre amp active/standby: "));
  Serial.println(InitData.Alive);
  Serial.print(F("Generic volume level  : "));
  Serial.println(VolLevels[0]);
  Serial.print(F("volumelevel input 1   : "));
  Serial.println(VolLevels[1]);
  Serial.print(F("volumelevel input 2   : "));
  Serial.println(VolLevels[2]);
  Serial.print(F("volumelevel input 3   : "));
  Serial.println(VolLevels[3]);
  Serial.print(F("volumelevel input 4   : "));
  Serial.println(VolLevels[4]);
  Serial.print(F("Text generic volume   : "));
  Serial.println(InitData.InputTekst[0]);
  Serial.print(F("Name channel 1        : "));
  Serial.println(InitData.InputTekst[1]);
  Serial.print(F("Name channel 2        : "));
  Serial.println(InitData.InputTekst[2]);
  Serial.print(F("Name channel 3        : "));
  Serial.println(InitData.InputTekst[3]);
  Serial.print(F("Name channel 4        : "));
  Serial.println(InitData.InputTekst[4]);
 }
#endif
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// function to check i2c bus
#ifdef DEBUG
 void ScanI2CBus() {
  uint8_t error;                                                      // error code
  uint8_t address;                                                    // address to be tested
  int NumberOfDevices;                                                // number of devices found
  Serial.println(F("ScanI2CBus: I2C addresses defined within the code are : ")); // print content of code
  Serial.print(F("Screen             : 0x"));
  Serial.println(OLED_Address, HEX);
  Serial.print(F("Ladderboard bottom : 0x"));
  Serial.println(MCP23017_I2C_ADDRESS_bottom, HEX);
  if (DaughterBoard) {
    Serial.print(F("Ladderboard top    : 0x"));
    Serial.println(MCP23017_I2C_ADDRESS_top, HEX);
  } else {
    Serial.println(F("No top board defined "));
  }
  Serial.println(F("ScanI2C: Scanning..."));
  NumberOfDevices = 0;
  for (address = 1; address < 127; address++) {                                   // loop through addresses
    Wire.beginTransmission(address);                                              // test address
    error = Wire.endTransmission();                                               // resolve errorcode
    if (error == 0) {                                                             // if address exist code is 0
      Serial.print("I2C device found at address 0x");                             // print address info
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
      NumberOfDevices++;
    }
  }
  if (NumberOfDevices == 0) Serial.println(F("ScanI2C:No I2C devices found"));
  else {
    Serial.print(F("ScanI2CBus: done, number of device found : "));
    Serial.println(NumberOfDevices);
  }
 }
#endif