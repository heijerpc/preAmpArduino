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
//       optimized the debugPreAmp options
//       added option to define input port config xlr/rca
//       added option to change startup delay
// v0.95 added option to change channel name
//       optimized menu
//       changed balance
// v0.96 bug fixes
//       made alive boolean persistant, so preamp starts up in previous mode (active/standby)
//       made startup screen more customabel 
// v.98  changed naming convention
//       to do, see if interupt can be changed.
// v.99  updated rotary procedure and bug fix in standby in combination with mute
// v1.0  turned debug off. removed disable/enable of interupts
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// below definitions could be change by user depending on setup, no code changes needed
//#define debugPreAmp                               // Comment this line when debugPreAmp mode is not needed
const bool daughterBoard = true;                    // boolean, defines if a daughterboard is used to support XLR and balance, either true or false
const uint8_t inputPortType = 0b00000011;           // define port config, 1 is XLR, 0 is RCA. Only used when daughterboard is true, LSB is input 1
#define delayPlop 20                                // delay timer between volume changes preventing plop, 20 mS for drv777
const char* topTekst = "PeWalt, V 1.00";            // current version of the code, shown in startscreen top, content could be changed
const char* middleTekst = "          please wait";  //as an example const char* MiddleTekst = "Cristian, please wait";
const char* bottemTekst = " " ;                     //as an example const char* BottemTekst = "design by: Walter Widmer" ;
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
  int startDelayTime;      // delay after power on amp to stabilize amp output
  bool HeadPhoneActive;    // boolean, headphones active, no output to amp
  bool DirectOut;          // boolean, preamp is used passive or active mode
  bool PrevStatusDirectOut;// boolean, status of previous status of direct out, used when using headphones
  bool Alive;              // boolean, defines if amp is active of standby modes
  char InputTekst[5][15];  // description of input channels
};
SavedData Amp;             // Amp is a structure defined by SavedData containing the values
int VolLevels[5];          // Vollevels is an array storing initial volume level when switching to channel, used if VolPerChannel is true
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// pin definitions
#define powerOnOff A0         // pin connected to the relay handeling power on/off of the amp
#define headphoneOnOff A1     // pin connected to the relay handeling headphones active / not active
#define directOutState A2     // pin connected to the relay handeling direct out state of amp
#define startDelay A3         // pin connected to the relay which connects amp to output
#define irReceivePin 2        // datapin of IR is connected to pin 2
volatile int rotaryPinA = 3;  // encoder pin A, volatile as its addressed within the interupt of the rotary
volatile int rotaryPinB = 4;  // encoder pin B,
#define rotaryButton 5        // pin is connected to the switch part of the rotary
#define buttonChannel 6       // pin is conncected to the button to change input channel round robin, pullup
#define buttonStandby 7       // pin is connected to the button to change standby on/off, pullup
#define buttonHeadphone 8     // pin  is connected to the button to change headphone on/off, pullup
#define buttonDirectOut 9     // pin  is connected to the button to switch between direct out or amp, pullup
#define buttonMute 10         // pin  is connected to the button to change mute on/off, pullup
#define ledStandby 11         // connected to a led that is on if amp is in standby mode
#define oledReset 12          // connected to the reset port of Oled screen, used to reset Oled screen
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// definitons for the IR receiver
#define DECODE_NEC        // only add NEC protocol to support apple remote, saves memory, fixed keyword
#include <IRremote.hpp>   // include drivers for IR
#define appleLeft 9       // below the IR codes received for apple
#define appleRight 6
#define appleUp 10
#define appleDown 12
#define appleMiddle 92
#define appleMenu 3
#define appleForward 95
int delayTimer;                       // timer to delay between volume changes using IR, actual value set in main loop
unsigned long milliSOfFirstReceive;   // used within IR procedures to determine if command is a repeat
bool longPressJustDetected;           // used within IR procedures to determine if command is a repeat
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// definitions for the oled screen
#define oledI2CAddress 0x3C                          // 3C is address used by oled controler
#define fontH08 u8g2_font_timB08_tr                  // 11w x 11h, char 7h
#define fontH08fixed u8g2_font_spleen5x8_mr          // 15w x 14h, char 10h
#define fontH10 u8g2_font_timB10_tr                  // 15w x 14h, char 10h
#define fontH10figure u8g2_font_spleen8x16_mn        //  8w x 13h, char 12h
#define fontH14 u8g2_font_timB14_tr                  // 21w x 18h, char 13h
#define fontgrahp u8g2_font_open_iconic_play_2x_t    // 16w x 16h pixels
#define fontH21cijfer u8g2_font_timB24_tn            // 17w x 31h, char 23h
char volInChar[4];                                   // used on many places to convert int to char
#include <U8g2lib.h>                                 // include graphical based character mode library
U8G2_SSD1309_128X64_NONAME0_F_HW_I2C Screen(U8G2_R2);  // define the screen type used.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// definitions for the attenuator board
#define mcp23017I2CAddressBottom 0x25     // I2C address of the relay board bottom
#define mcp23017I2CAddressTop 0x26        // I2C address of the relay board daughterboard
int attenuatorMain;                       // desired internal volume level,
int attenuatorRight;                      // desired volume value sent to relay
int attenuatorLeft;                       // desired volume value sent to relay
int attenuatorRightTmp;                   // intermediate volume sent to relay
int attenuatorLeftTmp;                    // intermeditate volume sent to relay
volatile int attenuatorChange = 0;        // change of volume out of interupt function
volatile int pinAstateCurrent = LOW;  
volatile int pinBstateCurrent = LOW;                
volatile int pinAStateLast = LOW;
bool muteEnabled = false;                 // boolean, status of Mute
bool volumeChanged = false;               // defines if volume is changed
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// general definitions
#include <Wire.h>                          // include functions for i2c
#include <ezButton.h>                      // include functions for debounce
ezButton button(rotaryButton);             // create ezButton object  attached to the rotary button;
#include <digitalWriteFast.h>              // include fast read used within interrupt routine
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// definitions for the compiler
void defineVolume(int increment);                         // define volume levels
void rotaryTurn();                                        // interupt handler if rotary is turned
void writeVolumeScreen(int volume);                       // write volume level to screen
void writeFixedValuesScreen();                            // write info on left side screen (mute, passive, input)
void oledSchermInit();                                    // init procedure for Oledscherm
void mCP23017init(uint8_t MCP23017_I2C_ADDRESS);          // init procedure for relay extender
void setRelayChannel(uint8_t Word);                       // set relays to selected input channel
void setRelayVolume(int RelayLeft, int RelayRight);       // set relays to selected volume level
void changeStandby();                                     // changes status of preamp between active and standby
void changeHeadphone();                                   // change status of headphones between active and standby
void changeDirectOut();                                   // change status of preamp between direct out and amplified
void changeMute();                                        // enable and disable mute
void changeInput(int change);                             // change the input channel
void waitForXseconds();                                   // wait for number of seconds, used to warm up amp
bool detectLongPress(uint16_t aLongPressDurationMillis);  // detect if multiple times key on remote controle is pushed
void checkIfEepromHasInit();                              // check if EEPROM is configured correctly
void writeEEprom();                                       // write initial values to the eeprom
void mainSetupMenu();                                     // the main setup menu
void setupMenuBalance();                                  // submenu, setting the balance
void setupMenuInitVol();                                  // submenu, setting initial values of volume
void setupMenuGeneral();                                  // submenu, setting general parameters
void setupMenuChangeNameInputChan();                      // submenu, setting names of input channels
char* chvolInChar3(int volume);                           // return 2 digit integer in char including +/-
char* chvolInChar2(int volume);                           // return 1 digit integer in char including +/-
void listContentEEPROM();                                 // used in debugPreAmp mode to print content of eeprom
void scanI2CBus();                                        // used in debugPreAmp mode to scan the i2c bus
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
  // pin modes
  pinMode(powerOnOff, OUTPUT);             // control the relay that provides power to the rest of the amp
  pinMode(headphoneOnOff, OUTPUT);         // control the relay that switches headphones on and off
  pinMode(directOutState, OUTPUT);         // control the relay that switches the preamp between direct out or active
  pinMode(startDelay, OUTPUT);             // control the relay that connects amp to output ports
  pinMode(rotaryPinA, INPUT_PULLUP);       // pin A rotary is high and its an input port
  pinMode(rotaryPinB, INPUT_PULLUP);       // pin B rotary is high and its an input port
  pinMode(buttonChannel, INPUT_PULLUP);    // button to change channel
  pinMode(buttonStandby, INPUT_PULLUP);    // button to go into standby/active
  pinMode(buttonHeadphone, INPUT_PULLUP);  // button to switch between AMP and headphone
  pinMode(buttonDirectOut, INPUT_PULLUP);  // button to switch between passive and active mode
  pinMode(buttonMute, INPUT_PULLUP);       // button to mute
  pinMode(ledStandby, OUTPUT);             // led will be on when device is in standby mode
  pinMode(oledReset, OUTPUT);              // set reset of oled screen
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // write init state to output pins
  digitalWrite(powerOnOff, LOW);      // keep amp turned off
  digitalWrite(headphoneOnOff, LOW);  // turn headphones off
  digitalWrite(directOutState, LOW);  // turn the preamp in active state
  digitalWrite(startDelay, LOW);      // disconnect amp from output
  digitalWrite(ledStandby, LOW);      // turn off standby led to indicate device is becoming active
  digitalWrite(oledReset, LOW);       // keep the Oled screen in reset mode
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// enable debugPreAmp to write to console if debugPreAmp is enabled
 #ifdef debugPreAmp
  Serial.begin(9600);  // if debugPreAmplevel on start monitor screen
 #endif
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // // read setup values stored within the eeprom
  checkIfEepromHasInit();      // check if eeprom has config file, otherwise write config
  EEPROM.get(0, Amp);     // get variables within init out of EEPROM
  EEPROM.get(110, VolLevels);  // get the array setting volume levels
 #ifdef debugPreAmp                   // if debugPreAmp enabled write message
  Serial.println(F("initprog: the following values read from EEPROM"));
  listContentEEPROM();
 #endif
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Set up pins for rotary:
  attachInterrupt(digitalPinToInterrupt(rotaryPinA), rotaryTurn, CHANGE);  // if pin encoderA changes run RotaryTurn
  button.setDebounceTime(50);                                              // set debounce time to 50 milliseconds
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // startup IR
  IrReceiver.begin(irReceivePin);  // Start the IR receiver
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // initialize the oled screen and relay extenderdelayPlop
  Wire.begin();  // start i2c communication
  delay(100);
 #ifdef debugPreAmp
  scanI2CBus();  // in debugPreAmp mode show i2c addresses used
 #endif
  oledSchermInit();  // intialize the Oled screen
  delay(100);
  mCP23017init(mcp23017I2CAddressBottom);  // initialize the relays
  if (daughterBoard) {                     // if we have a daughterboard
    mCP23017init(mcp23017I2CAddressTop);   // initialize daugterboard
  }
  setRelayVolume(0, 0);  // set volume relays to 0, just to be sure
  delay(delayPlop);      // wait to stabilize
  setRelayChannel(0);    // disconnect all input channels
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // set input / volume / etc depending standby active state
  Amp.Alive=!Amp.Alive; //invert bool so we can use standard standby proc
  changeStandby();
 #ifdef debugPreAmp
  Serial.println(F("setup: end of setup proc"));
 #endif
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Main loop
//////////////////////////////////////////////////////////////////////////////////////////////
void loop() {
  if (Amp.Alive) {                                              // we only react if we are in alive state, not in standby
    if (attenuatorChange != 0) {                                // if attenuatorChange is changed by the interupt we change volume/etc
      defineVolume(attenuatorChange);                           // Calculate correct temp and end volumelevels
      if (volumeChanged) {                                      // if volume is changed
        setRelayVolume(attenuatorLeftTmp, attenuatorRightTmp);  // set relays to the temp level
        delay(delayPlop);                                       // wait to prevent plop
        setRelayVolume(attenuatorLeft, attenuatorRight);        // set relay to the correct level
        writeVolumeScreen(attenuatorMain);                      // display volume level on oled screen
      }
      attenuatorChange = 0;  // reset the value to 0
    }
    if (digitalRead(buttonMute) == LOW) {  // if button mute is pushed
      delay(500);                          // wait to prevent multiple switches
      changeMute();                        // change status of mute
    }
    if (digitalRead(buttonChannel) == LOW) {  // if button channel switch is pushed
      delay(500);                             // wait to prevent multiple switches
      changeInput(1);                         // change input channel
    }
    if (digitalRead(buttonHeadphone) == LOW) {  // if button headphones switch is pushed
      delay(500);                               // wait to prevent multiple switches
      changeHeadphone();                        // change to headphone or back
    }
    if (digitalRead(buttonDirectOut) == LOW) {  // if button passive switch is pushed
      delay(500);                               // wait to prevent multiple switches
      changeDirectOut();                        // change active/passive state
    }
    button.loop();
    if (button.isPressed()) {  // if rotary button is pushed go to setup menu
      mainSetupMenu();         // start setup menu
      delay(500);              // wait to prevent multiple switches
    }
  }
  if (digitalRead(buttonStandby) == LOW) {  // if button standby is is pushed
    delay(500);                             // wait to prevent multiple switches
    changeStandby();                        // changes status
  }
  if (IrReceiver.decode()) {      // if we receive data on the IR interface
    if (detectLongPress(1500)) {  // simple function to increase speed of volume change by reducing wait time
      delayTimer = 0;
    } 
    else {
      delayTimer = 300;
    }
    switch (IrReceiver.decodedIRData.command) {  // read the command field containing the code sent by the remote
      case appleUp:
        if (Amp.Alive) {        // we only react if we are in alive state, not in standby
          defineVolume(1);           // calculate correct volume levels, plus 1
          if (volumeChanged) {
            setRelayVolume(attenuatorLeftTmp, attenuatorRightTmp);  //  set relays to the temp  level
            delay(delayPlop);                                       // wait to prevent plop
            setRelayVolume(attenuatorLeft, attenuatorRight);        // set relay to the correct level
            writeVolumeScreen(attenuatorMain);                      // display volume level on screen
          }
          delay(delayTimer);
        }
        break;
      case appleDown:
        if (Amp.Alive) {             // we only react if we are in alive state, not in standby
          defineVolume(-1);               // calculate correct volume levels, minus 1
          if (volumeChanged) {
            setRelayVolume(attenuatorLeftTmp, attenuatorRightTmp);  //  set relays to the temp  level
            delay(delayPlop);                                       // wait to prevent plop
            setRelayVolume(attenuatorLeft, attenuatorRight);        // set relay to the correct level
            writeVolumeScreen(attenuatorMain);                      // display volume level on oled screen
          }
          delay(delayTimer);
        }
        break;
      case appleLeft:
        if (Amp.Alive) {             // we only react if we are in alive state, not in standby
          changeInput(-1);           // change input channel
          delay(300);
        }
        break;
      case appleRight:
        if (Amp.Alive) {            // we only react if we are in alive state, not in standby
          changeInput(1);           // change input channel
          delay(300);
        }
        break;
      case appleForward:
        changeStandby();            // switch status of standby
        break;
      case appleMiddle:
        if (Amp.Alive) {            // we only react if we are in alive state, not in standby
          changeMute();             // change mute
          delay(300);
        }
        break;
      case appleMenu:
        if (Amp.Alive) {             // we only react if we are in alive state, not in standby
          changeDirectOut();         // switch between direct out and via preamp
          delay(300);
        }
        break;
    }
    IrReceiver.resume();  // open IR receiver for next command
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// delay the startup to wait for pre amp to stabilize, clears screen at start and end of proc
void waitForXseconds() {
 #ifdef debugPreAmp                           // if debugPreAmp enabled write message
  Serial.println(F("waitForXseconds: waiting for preamp to stabilize "));
 #endif
  Screen.clearBuffer();                       // clear the internal memory and screen
  Screen.setFont(fontH08);                    // choose a suitable font
  Screen.setCursor(0, 8);                     // set cursur in correct position
  Screen.print(topTekst);                     // write tekst to buffer
  Screen.setCursor(13, 63);                   // set cursur in correct position
  Screen.print(bottemTekst);                  // write tekst to buffer
  Screen.setFont(fontH10);                    // choose a suitable font
  Screen.setCursor(5, 28);                    // set cursur in correct position
  Screen.print(middleTekst);                  // write please wait
  for (int i = Amp.startDelayTime; i > 0; i--) {    // run for startDelayTime times
    Screen.setDrawColor(0);                         // clean channel name  part in buffer
    Screen.drawBox(65, 31, 30, 14);
    Screen.setDrawColor(1);
    Screen.setCursor(65, 45);                 // set cursur in correct position
    Screen.print(i);                          // print char array
    Screen.sendBuffer();                      // transfer internal memory to the display
    delay(1000);                              // delay for a second
  }
  Screen.clearDisplay();                      // clear screen
 #ifdef debugPreAmp                                 // if debugPreAmp enabled write message
  Serial.println(F("waitForXseconds: preamp wait time expired "));
 #endif
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// procedure to handle change of headphones status
void changeHeadphone() {
  if (Amp.HeadPhoneActive) {   // headphone is active so we have to change to inactive
 #ifdef debugPreAmp                       // if debugPreAmp enabled write message
    Serial.print(F(" changeHeadphone : moving from active to inactive,  "));
 #endif
    Amp.HeadPhoneActive = false;                 // status off headphone active changed to false
    setRelayVolume(0, 0);                             // switch volume off
    delay(delayPlop);                                 // wait to stabilize
    digitalWrite(headphoneOnOff, LOW);                // switch headphones off
    if (Amp.PrevStatusDirectOut==true) {         // if direct out was active before we turned on headphones
      Amp.DirectOut = true;                      // make DirectOut true
      digitalWrite(directOutState, HIGH);             // set relay high, amp is not active
    }
    delay(100);                                       // wait to stabilize
    defineVolume(0);                                  // define new volume levels
    setRelayVolume(attenuatorLeft, attenuatorRight);  // set volume correct level
    writeFixedValuesScreen();                         //display info on oled screen
    writeVolumeScreen(attenuatorMain);                //display volume on oled screen
    EEPROM.put(0, Amp);                          // write new status to EEPROM
 #ifdef debugPreAmp
    Serial.println(" status is now inactive ");
 #endif
  } 
  else {                                            // headphone is not active so we have to make it active
 #ifdef debugPreAmp                                         // if debugPreAmp enabled write message
    Serial.print(F(" changeHeadphone : moving from inactive to active,  "));
 #endif
    Amp.HeadPhoneActive = true;           // status headphone is true
    setRelayVolume(0, 0);                 // switch volume off
    delay(delayPlop);                     // wait to stabilize
    if (Amp.DirectOut) {                  // amp must be active if we use headphone
      Amp.DirectOut = false;              // make DirectOut false
      digitalWrite(directOutState, LOW);  // set relay low, amp is active
      delay(100);                         // wait to stabilize
      Amp.PrevStatusDirectOut=true;
    }
    else {
      Amp.PrevStatusDirectOut=false;
    }
    digitalWrite(headphoneOnOff, HIGH);  // switch headphone relay on
    delay(100);                          // wait to stabilize
    defineVolume(0);                     // define new volume level
    if (!muteEnabled) {                  // if mute not enabled write volume to relay
      setRelayVolume(attenuatorLeft, attenuatorRight);
    }
    writeFixedValuesScreen();           //display info on oled screen
    writeVolumeScreen(attenuatorMain);  //display volume level on screen
    EEPROM.put(0, Amp);                 // write new status to EEPROM
 #ifdef debugPreAmp                            // if debugPreAmp enabled write message
    Serial.println(F(" status is now active "));
 #endif
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// procedure to switch between direct out and pre amp amplifier
void changeDirectOut() {
  if (!Amp.HeadPhoneActive) {             // only allow switch if headphones is not active
    if (Amp.DirectOut) {                  // Amp is in direct out state so we have to change to amp active
 #ifdef debugPreAmp                       // if debugPreAmp enabled write message
      Serial.print(F(" changeDirectOut : moving from direct out to preamp,  "));
 #endif
      Amp.DirectOut = false;              // make the boolean the correct value
      EEPROM.put(0, Amp);                 // write new status to EEPROM
      setRelayVolume(0, 0);               // switch volume off
      delay(delayPlop);                   // wait to stabilize
      digitalWrite(directOutState, LOW);  // switch DirectOut relay off
      delay(100);                         // wait to stabilize
      defineVolume(0);                    // define new volume level
      if (!muteEnabled) {                 // if mute not enabled write volume to relay
        setRelayVolume(attenuatorLeft, attenuatorRight);
      }
      writeFixedValuesScreen();            //display info on oled screen
      writeVolumeScreen(attenuatorMain);   //display volume level on screen
 #ifdef debugPreAmp                              // if debugPreAmp enabled write message
      Serial.println(F(" status is now active "));
 #endif
    } 
    else {                                 // Amp is in active mode so we have to change it to passive
 #ifdef debugPreAmp                        // if debugPreAmp enabled write message
      Serial.print(F(" changeDirectOut : switch from preamp amp active to direct out,  "));
 #endif
      Amp.DirectOut = true;                // make the boolean the correct value
      EEPROM.put(0, Amp);                  // write new status to EEPROM
      setRelayVolume(0, 0);                // switch volume off
      delay(delayPlop);                    // wait to stabilize
      digitalWrite(directOutState, HIGH);  // switch direct out relay on
      delay(100);                          // wait to stabilize
      defineVolume(0);                     // define new volume level
      setRelayVolume(attenuatorLeft, attenuatorRight);
      writeFixedValuesScreen();            //display info on oled screen
      writeVolumeScreen(attenuatorMain);   //display volume level on screen
 #ifdef debugPreAmp                        // if debugPreAmp enabled write message
      Serial.println(F(" status is now DirectOut "));
 #endif
    }
  }
}
///////////////////////////////////////////////////////////////////////////////////////////////////////
//// display the main setup menu on the screen
void mainSetupMenu() {
  int orgAttenuatorLevel = attenuatorMain;                  // save orginal attenuator level
  bool write = true;                                        // used determine if we need to write volume level to screen
  bool quit = false;                                        // determine if we should quit the loop
  bool restoreOldVolAndChan = false;
  int choice = 5;                                           // value of submenu choosen
  int offset = -63;                                         // ofset used to display correct vol level on screen
  unsigned long int idlePeriod = 30000;                     // idlePeriod you need to change something within menu otherwise quit menu
  unsigned long timeSaved;                                  // used to help determine idle time
  if (!Amp.DirectOut) offset = offset + Amp.PreAmpGain;     // if amp not direct out add gain amp
  if (muteEnabled) changeMute();                            // if mute enabled disable mute
 #ifdef debugPreAmp                                         // if debugPreAmp enabled write message
  Serial.println(F("setupMenu: Settings in EEPROM starting setup menu"));
  listContentEEPROM();
 #endif
  button.loop();                                            // make sure button is clean
  timeSaved = millis();                                     // save current time
  while (!quit) {                                           // run as long as option for is not choosen
    if (millis() > timeSaved + idlePeriod) {                // timeout to verify if still somebody doing something
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
      sprintf(volInChar, "%i", choice);
      Screen.drawButtonUTF8(110, 60, U8G2_BTN_BW1, 0, 1, 1, volInChar);
      Screen.sendBuffer();
    }
    write = false;                         // we assume nothing changed, so dont write menu
    if (attenuatorChange != 0) {           // if attenuatorChange is changed using rotary
      choice = choice + attenuatorChange;  // change choice
      attenuatorChange = 0;                // reset attenuatorChange
      if (choice > 5) {                    // code to keep attenuator between 1 and 4
        choice = 1;
      }
      if (choice < 1) {
        choice = 5;
      }
      write = true;           // output is changed so we have to rewrite screen
      timeSaved = millis();  // save time of last change
    }
    button.loop();             // detect if button is pressed
    if (button.isPressed()) {  // choose the correct function
      if (choice == 1) setupMenuBalance();
      if (choice == 2) {
        setupMenuInitVol();
        restoreOldVolAndChan = true;
      }
      if (choice == 3) setupMenuChangeNameInputChan();
      if (choice == 4) setupMenuGeneral();
      if (choice == 5) quit = true;
      button.loop();          // be sure button is clean
      write = true;           // output is changed so we have to rewrite screen
      timeSaved = millis();   // save time of last change
    }
  }

  // we quit menu, save all data and restore sound and volume
  EEPROM.put(0, Amp);          // save new values
  EEPROM.put(110, VolLevels);  // save volume levels
  if (restoreOldVolAndChan) {
    attenuatorMain = orgAttenuatorLevel;  // restore orginal attenuator level
    defineVolume(0);                      // define volume levels
    setRelayVolume(0, 0);                 // set volume level to 0
    delay(delayPlop);
    setRelayChannel(Amp.SelectedInput);   // set amp on prefered channel
    delay(delayPlop);
    setRelayVolume(attenuatorLeftTmp, attenuatorRightTmp);  // set relays to the temp  level
    delay(delayPlop);
    setRelayVolume(attenuatorLeft, attenuatorRight);  // set relays to the old  level
  }
  Screen.clearBuffer();                    // clean memory screen
  writeFixedValuesScreen();                // display info on screen
  writeVolumeScreen(attenuatorMain);       // display volume level on oled screen
  Screen.sendBuffer();                     // write memory to screen
 #ifdef debugPreAmp                        // if debugPreAmp enabled write message
  Serial.println(F("setupMenu: Settings in EEPROM ending setup menu"));
  listContentEEPROM();
 #endif
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// procedure to change the names display for the input channels
void setupMenuChangeNameInputChan() {
  const int shortPressTime = 1000;                                          // short time press
  const int longPressTime = 1000;                                           // long time press
  bool write = true;                                                        // used determine if we need to write volume level to screen
  bool quit = false;                                                        // determine if we should quit the loop
  bool isPressing = false;                                                  // defines if button is pressed
  bool isLongDetected = false;                                              // defines if button is pressed long
  bool isShortDetected = false;                                             // defines if button is pressed short
  int inputChan = 1;                                                        // inputchannel
  int selectedChar = 0;                                                     // char to be changed
  int curCharPos = -1;                                                      // char position within CharsAvailable
  unsigned long int idlePeriod = 30000;                                     // idlePeriod you need to change something within menu otherwise quit menu
  unsigned long timeSaved;                                                  // used to help determine idle time
  unsigned long pressedTime = 0;                                            // used for detecting pressed time
  unsigned long releasedTime = 0;                                           // used for detecting pressen time
  char charsAvailable[40] = { "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890 -:" };  // list of chars available for name input channel
  Screen.clearBuffer();
  Screen.setFont(fontH10);
  Screen.setCursor(5, 10);
  Screen.print(F("Input channel name"));                                    // print header
  Screen.setFont(fontH08fixed);
  for (int i = 1; i < 5; i++) {                                             // print current names of channels
    Screen.setCursor(0, (20 + (i * 10)));
    Screen.print(Amp.InputTekst[i]);
  }
  Screen.sendBuffer();                                                      // send content to screen
  button.loop();                                                            
  timeSaved = millis();
  while (!quit) {                              // loop through the different channel names
    if (millis() > timeSaved + idlePeriod) {   // verify if still somebody doing something
      quit = true;
      break;
    }
    selectedChar = 0;                          // select the first char of the channel name
    while ((!quit) && (!isLongDetected)) {     // loop through name of channel
      if (millis() > timeSaved + idlePeriod) { // verify if still somebody doing something
        quit = true;
        break;
      }
      write = true;                                                 // force to write first char
      while ((!isShortDetected) && (!quit) && (!isLongDetected)) {  // loop to change a char
        if (millis() > timeSaved + idlePeriod) {                    // verify if still somebody doing something
          quit = true;
          break;
        }
        if (write) {  // if char is changed write char
          char buf[2];
          sprintf(buf, "%c", Amp.InputTekst[inputChan][selectedChar]);  // change first char
          strcpy(volInChar, buf);
          Screen.setDrawColor(0);  // clean letter part in buffer
          Screen.drawBox(99, 44, 16, 16);
          Screen.setDrawColor(1);
          Screen.setFont(fontH10);
          Screen.setCursor(100, 54);
          Screen.print(volInChar);  // write letter
          Screen.setDrawColor(0);   // clean channel name part in buffer
          Screen.drawBox((5 * selectedChar), (10 + (inputChan * 10)), 6, 11);
          Screen.setDrawColor(1);
          Screen.setFont(fontH08fixed);
          Screen.drawButtonUTF8((5 * selectedChar), (20 + (inputChan * 10)), U8G2_BTN_INV, 0, 0, 0, volInChar);  // write char inverse
          Screen.sendBuffer();
          for (int charPos = 0; charPos < 38; charPos++) {  // detect which pos is of current char in charoptions
            if (Amp.InputTekst[inputChan][selectedChar] == charsAvailable[charPos]) {
              curCharPos = charPos;
              break;
            }
          }
          write = false;  // all write actions done, write is false again
        }
        if (attenuatorChange != 0) {                   // if attenuatorChange is changed using rotary
          write = true;                                // something will change so we need to write
          curCharPos = curCharPos + attenuatorChange;  // change attenuatorLeft init
          attenuatorChange = 0;                        // reset attenuatorChange
          if (curCharPos > 38) {                       // code to keep curcharpos between 0 and 38
            curCharPos = 0;
          }
          if (curCharPos < 0) {
            curCharPos = 38;
          }
          Amp.InputTekst[inputChan][selectedChar] = charsAvailable[curCharPos];  // change the char to the new char
          timeSaved = millis();
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
          if (pressDuration < shortPressTime) isShortDetected = true;
        }
        if (isPressing == true && isLongDetected == false) {
          long pressDuration = millis() - pressedTime;
          if (pressDuration > longPressTime) isLongDetected = true;
        }
      }
      Screen.setDrawColor(0);  // clean channel name  part in buffer
      Screen.drawBox(0, (10 + (inputChan * 10)), 128, 12);
      Screen.setDrawColor(1);
      Screen.setCursor(0, (20 + (inputChan * 10)));
      Screen.print(Amp.InputTekst[inputChan]);
      Screen.sendBuffer();
      isShortDetected = false;
      selectedChar++;
      if (selectedChar > 10) selectedChar = 0;  // only allow chars within specific range to be changed.
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
        if (pressDuration < shortPressTime) isShortDetected = true;
      }
      if (isPressing == true && isLongDetected == false) {
        long pressDuration = millis() - pressedTime;
        if (pressDuration > longPressTime) isLongDetected = true;
      }
    }
    button.loop();
    isShortDetected = false;
    isLongDetected = false;
    isPressing = false;
    selectedChar = 0;
    inputChan++;
    if (inputChan > 4) quit = true;
    timeSaved = millis();
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// change brightness, amp attenuation and start delay
void setupMenuGeneral() {
  const int shortPressTime = 1000;       // short time press
  const int longPressTime = 1000;        // long time press
  bool write = true;                     // used determine if we need to write volume level to screen
  bool quit = false;                     // determine if we should quit the loop
  bool isPressing = false;               // defines if button is pressed
  bool isLongDetected = false;           // defines if button is pressed long
  bool isShortDetected = false;          // defines if button is pressed short
  unsigned long int idlePeriod = 30000;  // idlePeriod you need to change something within menu otherwise quit menu
  unsigned long timeSaved;               // used to help determine idle time
  unsigned long pressedTime = 0;         // time button was pressed
  unsigned long releasedTime = 0;        // time buttons was released
  Screen.clearBuffer();                  // write menu including all variables
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
  Screen.print(Amp.ContrastLevel);
  Screen.setCursor(110, 31);
  if (Amp.PreAmpGain < 10) Screen.print(F(" "));
  Screen.print(Amp.PreAmpGain);
  Screen.setCursor(110, 42);
  if (Amp.startDelayTime < 10) Screen.print(F(" "));
  Screen.print(Amp.startDelayTime);
  Screen.sendBuffer();
  button.loop();                          // verify button is clean
  while ((!isLongDetected) && (!quit)) {  // loop this page as long as no long press and no timeout run menu
    write = true;
    timeSaved = millis();
    /////////////////////////change brightness
    while ((!isShortDetected) && (!quit) && (!isLongDetected)) {  // loop to change brightness
      if (millis() > timeSaved + idlePeriod) {                   // timeout to verify if still somebody doing something
        quit = true;
        break;
      }
      if (write) {               // if changed rewrite value of brightness
        Screen.setDrawColor(0);  // clean brightness part in buffer
        Screen.drawBox(108, 12, 20, 12);
        Screen.setDrawColor(1);
        strcpy(volInChar, " ");  // convert int to char using right spacing
        char buf[2];
        sprintf(buf, "%i", Amp.ContrastLevel);
        strcat(volInChar, buf);
        Screen.drawButtonUTF8(110, 20, U8G2_BTN_BW1, 0, 1, 1, volInChar);  // print value using a box
        Screen.sendBuffer();                                               // copy memory to screen
      }
      write = false;                                                       // assume no changes
      if (attenuatorChange != 0) {                                         // if attenuatorChange is changed using rotary
        write = true;                                                      // brightness  is changed
        Amp.ContrastLevel = Amp.ContrastLevel + attenuatorChange;          // change brightness
        attenuatorChange = 0;                                              // reset attenuatorChange
        if (Amp.ContrastLevel > 7) {                                       // code to keep attenuator between 1 and 7
          Amp.ContrastLevel = 7;
          write = false;
        }
        if (Amp.ContrastLevel < 0) {
          Amp.ContrastLevel = 0;
          write = false;
        }
        Screen.setContrast((((Amp.ContrastLevel * 2) + 1) << 4) | 0x0f);    // set new value of brightness (1-254)
        timeSaved = millis();                                               // save time of last change
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
        if (pressDuration < shortPressTime) isShortDetected = true;
      }
      if (isPressing == true && isLongDetected == false) {
        long pressDuration = millis() - pressedTime;
        if (pressDuration > longPressTime) isLongDetected = true;
      }
    }                        // finished changing brightness
    Screen.setDrawColor(0);  // write value to screen without box
    Screen.drawBox(108, 12, 20, 12);
    Screen.setDrawColor(1);
    Screen.setCursor(110, 20);
    Screen.print(F(" "));
    Screen.print(Amp.ContrastLevel);
    Screen.sendBuffer();
    isShortDetected = false;                                      // reset short push detected
    write = true;                                                 // set write to true so we start correctly preamp gain
    timeSaved = millis();                                        // save time of last change
    /////////////////////////change amp gain
    while ((!isShortDetected) && (!quit) && (!isLongDetected)) {  // loop to change amp offset
      if (millis() > timeSaved + idlePeriod) {                   // timeout to verify if still somebody doing something
        quit = true;
        break;
      }
      if (write) {               // if amp gain is changed
        Screen.setDrawColor(0);  // clean part of amp gaing in memory
        Screen.drawBox(108, 23, 20, 12);
        Screen.setDrawColor(1);
        if (Amp.PreAmpGain < 10) {  // change int into char with correct spacing
          strcpy(volInChar, " ");
        } 
        else {
          strcpy(volInChar, "");
        }
        char buf[3];
        sprintf(buf, "%i", Amp.PreAmpGain);
        strcat(volInChar, buf);
        Screen.drawButtonUTF8(110, 31, U8G2_BTN_BW1, 0, 1, 1, volInChar);  //write amp gain with a box
        Screen.sendBuffer();
      }
      write = false;
      if (attenuatorChange != 0) {                                     // if attenuatorChange is changed using rotary
        write = true;                                                  // amp gain is changed
        Amp.PreAmpGain = Amp.PreAmpGain + attenuatorChange;            // change amp gain
        attenuatorChange = 0;                                          // reset attenuatorChange
        if (Amp.PreAmpGain > 63) {                                     // code to keep attenuator between 0 and 63
          Amp.PreAmpGain = 63;
          write = false;
        }
        if (Amp.PreAmpGain < 0) {
          Amp.PreAmpGain = 0;
          write = false;
        }
        timeSaved = millis();  // save time of last change
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
        if (pressDuration < shortPressTime) isShortDetected = true;
      }
      if (isPressing == true && isLongDetected == false) {
        long pressDuration = millis() - pressedTime;
        if (pressDuration > longPressTime) isLongDetected = true;
      }
    }      // amp gain is set
    Screen.setDrawColor(0);           // clean amp gain  part in buffer
    Screen.drawBox(108, 23, 20, 12);
    Screen.setDrawColor(1);
    Screen.setCursor(110, 31);        // write value of amp gain without box in correct setup
    if (Amp.PreAmpGain < 10) Screen.print(F(" "));
    Screen.print(Amp.PreAmpGain);
    Screen.sendBuffer();
    isShortDetected = false;                                      // reset short push detected
    write = true;                                                 // set write to true so we start correctly preamp gain
    timeSaved = millis();                                         // save time of last change
    /////////////////////////change delay after power on
    while ((!isShortDetected) && (!quit) && (!isLongDetected)) {  // loop to change startup delay
      if (millis() > timeSaved + idlePeriod) {                    // verify if still somebody doing something
        quit = true;
        break;
      }
      if (write) {                // if amp gain is changed
        Screen.setDrawColor(0);   // clean delay part in buffer
        Screen.drawBox(108, 34, 20, 12);
        Screen.setDrawColor(1);
        if (Amp.startDelayTime < 10) {  // change value from int into char using correct spacing
          strcpy(volInChar, " ");
        } 
        else {
          strcpy(volInChar, "");
        }
        char buf[3];
        sprintf(buf, "%i", Amp.startDelayTime);
        strcat(volInChar, buf);
        Screen.drawButtonUTF8(110, 42, U8G2_BTN_BW1, 0, 1, 1, volInChar);  // write value in char in a box
        Screen.sendBuffer();
      }
      write = false;
      if (attenuatorChange != 0) {                                             // if attenuatorChange is changed using rotary
        write = true;                                                          // delay is changed
        Amp.startDelayTime = Amp.startDelayTime + attenuatorChange;            // change delay
        attenuatorChange = 0;                                                  // reset attenuatorChange
        if (Amp.startDelayTime > 99) {                                         // code to keep delay between 0 and 99
          Amp.PreAmpGain = 99;
          write = false;
        }
        if (Amp.startDelayTime < 0) {
          Amp.startDelayTime = 0;
          write = false;
        }
        timeSaved = millis();  // save time of last change
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
        if (pressDuration < shortPressTime) isShortDetected = true;
      }
      if (isPressing == true && isLongDetected == false) {
        long pressDuration = millis() - pressedTime;
        if (pressDuration > longPressTime) isLongDetected = true;
      }
    }                        // start delay is set
    Screen.setDrawColor(0);  // clean start delay part in buffer
    Screen.drawBox(108, 34, 20, 12);
    Screen.setDrawColor(1);
    Screen.setCursor(110, 42);  // write value delay to buffer without box
    if (Amp.startDelayTime < 10) Screen.print(F(" "));
    Screen.print(Amp.startDelayTime);
    Screen.sendBuffer();
    isShortDetected = false;  // reset short push detected
  }
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////
//
void setupMenuInitVol() {
  const int shortPressTime = 1000;                                 // short time press
  const int longPressTime = 1000;                                  // long time press
  bool write = true;                                               // used determine if we need to write volume level to screen
  bool quit = false;                                               // determine if we should quit the loop
  bool isPressing = false;                                         // defines if button is pressed
  bool isLongDetected = false;                                     // defines if button is pressed long
  bool isShortDetected = false;                                    // defines if button is pressed short
  int offset = -63;                                                // offset to define vol level shown on screen
  unsigned long int idlePeriod = 30000;                            // idlePeriod you need to change something within menu otherwise quit menu
  unsigned long timeSaved;                                         // used to help determine idle time
  unsigned long pressedTime = 0;                                   // time button was pressed
  unsigned long releasedTime = 0;                                  // time button was released
  if (!Amp.DirectOut) offset = offset + Amp.PreAmpGain;            // determine offset between internal volume and vol displayed on screen
  Screen.clearBuffer();
  Screen.setFont(fontH10);
  Screen.setCursor(38, 10);
  Screen.print(F("Init volume"));  // write header
  button.loop();
  timeSaved = millis();
  while ((!isLongDetected) && (!quit)) {       // loop this page as long as rotary button not long pressed and action is detected
    if (millis() > timeSaved + idlePeriod) {   // verify if still somebody doing something
      quit = true;
      break;
    }
    write = true;  // something changed
    Screen.setFont(fontH08fixed);
    while ((!isShortDetected) && (!quit) && (!isLongDetected)) {  // loop to determine  volume/channel or general volume
      if (millis() > timeSaved + idlePeriod) {                   // verify if still somebody doing something
        quit = true;
        break;
      }
      Screen.setCursor(0, 20);
      Screen.print(F("Volume per channel"));                             // write line defining what to change
      if (write) {                                                     // if something changed
        if (Amp.VolPerChannel) {                                  // if we have vol/channel
          Screen.drawButtonUTF8(110, 20, U8G2_BTN_BW1, 0, 1, 1, "YES");  // write yes in a box
          for (int i = 1; i < 5; i++) {                                // write the port description and the current levels
            Screen.setCursor(0, (20 + (i * 10)));
            Screen.print(Amp.InputTekst[i]);
            Screen.setCursor(110, (20 + (i * 10)));
            Screen.print(chvolInChar3(VolLevels[i] + offset));
          }
        } 
        else {  // if we have a general volume level
          Screen.setCursor(110, 24);
          Screen.drawButtonUTF8(110, 20, U8G2_BTN_BW1, 0, 1, 1, " NO");  // write no in a box
          Screen.setCursor(0, 31);                                       // write general description and the current level
          Screen.print(Amp.InputTekst[0]);
          Screen.setCursor(110, 31);
          Screen.print(chvolInChar3(VolLevels[0] + offset));
        }
        Screen.sendBuffer();  // copy memory to screen
      }
      write = false;
      if (attenuatorChange != 0) {                           // if rotary is turned
        Amp.VolPerChannel = (!Amp.VolPerChannel);  // invert the setting if we use vol/channel yes no
        attenuatorChange = 0;                                // reset attenuatorChange
        Screen.setDrawColor(0);                                // clean memory part of yes/no of volume per channel and rest of screen
        Screen.drawBox(0, 14, 127, 50);
        Screen.drawBox(108, 12, 20, 12);
        Screen.setDrawColor(1);
        write = true;
        timeSaved = millis();  // save time of last change
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
        if (pressDuration < shortPressTime) isShortDetected = true;
      }
      if (isPressing == true && isLongDetected == false) {
        long pressDuration = millis() - pressedTime;
        if (pressDuration > longPressTime) isLongDetected = true;
      }
    }                        // volume/channel is set
    Screen.setDrawColor(0);  // clean volume part in buffer
    Screen.drawBox(108, 12, 20, 12);
    Screen.setDrawColor(1);
    Screen.setFont(fontH08fixed);
    if (Amp.VolPerChannel) {  // write correct value without box
      Screen.setCursor(110, 20);
      Screen.print("YES");
    } 
    else {
      Screen.setCursor(110, 20);
      Screen.print(" NO");
    }
    Screen.sendBuffer();
    isShortDetected = false;
    ///////////////  set values per channel or generic volume
    if (Amp.VolPerChannel) {                       // we have volume / channel
      for (int i = 1; i < 5; i++) {                // run loop for  4 volumes per channel
        if (millis() > timeSaved + idlePeriod) {   // verify if still somebody doing something
          quit = true;
          break;
        }
        if ((!quit) && (!isLongDetected)) {  // only when we not quiting the for loop, if we quit commands are not needed
          write = true;
          setRelayVolume(0, 0);
          delay(delayPlop);
          setRelayChannel(i);  // select the correct input channel
        }
        while ((!isShortDetected) && (!quit) && (!isLongDetected)) {  // loop to set value for a specific channel
          if (millis() > timeSaved + idlePeriod) {                    // timeout to verify if still somebody doing something
            quit = true;
            break;
          }
          if (write) {                                              // if volume level changed
            attenuatorMain = VolLevels[i];                          // set the main volume to the level of the channel
            defineVolume(0);                                        // calculate vol levels
            setRelayVolume(attenuatorLeftTmp, attenuatorRightTmp);  //  set relays to the temp  level
            delay(delayPlop);
            setRelayVolume(attenuatorLeft, attenuatorRight);        // set relays to final level
            Screen.setDrawColor(0);                                 // clean volume part in buffer
            Screen.drawBox(108, (12 + (i * 10)), 20, 12);
            Screen.setDrawColor(1);
            Screen.drawButtonUTF8(110, (20 + (i * 10)), U8G2_BTN_BW1, 0, 1, 1, (chvolInChar3(VolLevels[i] + offset)));  // write vol level in box
            Screen.sendBuffer();
          }
          write = false;
          if (attenuatorChange != 0) {  // if attenuatorChange is changed using rotary
            write = true;
            VolLevels[i] = VolLevels[i] + attenuatorChange;  // change volume level of specific channel
            attenuatorChange = 0;                            // reset attenuatorChange
            if (VolLevels[i] > 63) {                         // code to keep attenuator between 0 and 63
              VolLevels[i] = 63;
              write = false;
            }
            if (VolLevels[i] < 0) {
              VolLevels[i] = 0;
              write = false;
            }
            timeSaved = millis();  // save time of last change
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
            if (pressDuration < shortPressTime) isShortDetected = true;
          }
          if (isPressing == true && isLongDetected == false) {
            long pressDuration = millis() - pressedTime;
            if (pressDuration > longPressTime) isLongDetected = true;
          }
        }                      // volume level for specific channel is set
        Screen.setDrawColor(0);  // clean volume part in buffer
        Screen.drawBox(108, (12 + (i * 10)), 20, 12);
        Screen.setDrawColor(1);
        Screen.setCursor(110, (20 + (i * 10)));
        Screen.print(chvolInChar3(VolLevels[i] + offset));  // write correct volume level without box around it
        Screen.sendBuffer();
        isShortDetected = false;
      }
    } 
    else {  // no volume/channel, so we set generic volume
      write = true;
      while ((!isShortDetected) && (!quit) && (!isLongDetected)) {
        if (millis() > timeSaved + idlePeriod) {  // timeout to verify if still somebody doing something
          quit = true;
          break;
        }
        if (write) {                                              // value is changed
          attenuatorMain = VolLevels[0];                          // set main volume level to generic volume level
          defineVolume(0);                                        // calculate volume levels
          setRelayVolume(attenuatorLeftTmp, attenuatorRightTmp);  // set relays to the temp  level
          delay(delayPlop);
          setRelayVolume(attenuatorLeft, attenuatorRight);        // set relays to final version
          Screen.setDrawColor(0);                                 // clean volume part in buffer
          Screen.drawBox(108, 22, 20, 12);
          Screen.setDrawColor(1);
          Screen.drawButtonUTF8(110, 31, U8G2_BTN_BW1, 0, 1, 1, (chvolInChar3(VolLevels[0] + offset)));  // write new volume level in a box
          Screen.sendBuffer();
        }
        write = false;
        if (attenuatorChange != 0) {                       // if attenuatorChange is changed using rotary
          write = true;                                    // volume level is changed
          VolLevels[0] = VolLevels[0] + attenuatorChange;  // change volume level
          attenuatorChange = 0;                            // reset attenuatorChange
          if (VolLevels[0] > 63) {                         // code to keep attenuator between 0 and 63
            VolLevels[0] = 63;
            write = false;
          }
          if (VolLevels[0] < 0) {
            VolLevels[0] = 0;
            write = false;
          }
          timeSaved = millis();
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
          if (pressDuration < shortPressTime) isShortDetected = true;
        }
        if (isPressing == true && isLongDetected == false) {
          long pressDuration = millis() - pressedTime;
          if (pressDuration > longPressTime) isLongDetected = true;
        }
      }                      // volume level for generic is set
      Screen.setDrawColor(0);  // clean volume part in buffer
      Screen.drawBox(108, 22, 20, 12);
      Screen.setDrawColor(1);
      Screen.setCursor(110, 31);
      Screen.print(chvolInChar3(VolLevels[0] + offset));  // write correct volume level without box around it
      Screen.sendBuffer();
      isShortDetected = false;
    }
  }
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  set balance value using menu
void setupMenuBalance() {
  int balanceRight;                                                // balance value right channel
  int balanceLeft;                                                 // balance value left channel
  const int longPressTime = 500;                                   // long time press
  bool write = true;                                               // used determine if we need to write volume level to screen
  bool quit = false;                                               // determine if we should quit the loop
  bool isPressing = false;                                         // defines if button is pressed
  bool isLongDetected = false;                                     // defines if button is pressed long
  int offset = -63;                                                // offset to define vol level shown on screen
  unsigned long int idlePeriod = 30000;                            // idlePeriod you need to change something within menu otherwise quit menu
  unsigned long timeSaved;                                         // used to help determine idle time
  unsigned long pressedTime = 0;                                   // time button is pressed
  button.loop();                                                   // reset status of button
  if (!Amp.DirectOut) offset = offset + Amp.PreAmpGain;            // calculate correct value of offset of internal vol level
  Screen.setFont(fontH10);                                         // set font
  Screen.clearBuffer();
  if (!daughterBoard) {                                            // we need 2 boards to set balance.
    Screen.setCursor(0, 30);
    Screen.print(F("daughterboard missing"));
    Screen.sendBuffer();
    delay(2000);    
  } 
  else {  // we have 2 boards
    Screen.setCursor(44, 10);
    Screen.print(F("balance"));                                      // print heading
    Screen.setCursor(11, 38);
    Screen.print(F("-------------+-------------"));
    Screen.setCursor(0, 63);
    Screen.print(F("L:               dB       R:"));
    write = true;                                                  // we assume something changed to write first time value
    timeSaved = millis();                                          // save time last change
    while ((!isLongDetected) && (!quit)) {                         // loop to set balance as long not quit and button not long pressed
      if (millis() > timeSaved + idlePeriod) {                     // verify if still somebody doing something
        quit = true;
        break;
      }
      if (write) {                                                        // balance is changed
        Screen.drawGlyph((63 + (Amp.BalanceOffset * 4)), 50, 0x005E);     // write current balance using position
        balanceRight = (Amp.BalanceOffset >> 1);                          // divide BalanceOffset using a bitshift to the right
        balanceLeft = balanceRight - Amp.BalanceOffset;                   // difference is balance left
        Screen.setFont(fontH10figure);
        Screen.setDrawColor(0);  // clean balance part in buffer
        Screen.drawBox(16, 52, 24, 11);
        Screen.drawBox(110, 52, 24, 11);
        Screen.setDrawColor(1);
        Screen.setCursor(15, 63);  // write balance values into screen memory
        Screen.print(chvolInChar2(balanceLeft));
        Screen.setCursor(110, 63);
        Screen.print(chvolInChar2(balanceRight));
        Screen.sendBuffer();  // copy memory to screen
      }
      write = false;
      if (attenuatorChange != 0) {  // if attenuatorChange is changed using rotary
        write = true;               // balance is changed
        Screen.setFont(fontH10);
        Screen.setDrawColor(0);     // clean balance pointer in part in buffer
        Screen.drawBox((63 + (Amp.BalanceOffset * 4)), 39, 9, 8);
        Screen.setDrawColor(1);
        Amp.BalanceOffset = Amp.BalanceOffset + attenuatorChange;        // calculate new balance offset
        attenuatorChange = 0;                                            // reset value
        if (Amp.BalanceOffset > 14) {                                    // keep value between 10 and -10
          Amp.BalanceOffset = 14;
          write = false;  // value was already 14, no use to write
        }
        if (Amp.BalanceOffset < -14) {
          Amp.BalanceOffset = -14;
          write = false;  // value was alread -14, no use to write
        }
        Screen.drawGlyph((63 + (Amp.BalanceOffset * 4)), 50, 0x005E);  // write current balance pointer using position
        defineVolume(0);
        setRelayVolume(attenuatorLeftTmp, attenuatorRightTmp);         //  set relays to the temp  level
        delay(delayPlop);
        setRelayVolume(attenuatorLeft, attenuatorRight);               // set new volume level with balance level
        timeSaved = millis();                                          // set time to last change
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
        if (pressDuration > longPressTime) isLongDetected = true;
      }
    }
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// procedure to handle moving to and coming out of standby mode
void changeStandby() {
  if (Amp.Alive) {     // move from alive to standby, turn off screen, relays and amp
 #ifdef debugPreAmp    // if debugPreAmp enabled write message
    Serial.print(F(" changeStandby : moving to standby,  "));
 #endif
    digitalWrite(ledStandby, HIGH);    // turn on standby led to indicate device is in standby
    Amp.Alive = false;                 // we are in standby, alive is false
    EEPROM.put(0, Amp);                // write new status to EEPROM
    setRelayVolume(0, 0);              // set volume relays off
    delay(delayPlop);                  // wait to stabilize
    setRelayChannel(0);                // disconnect all input channels
    delay(100);                        // wait to stabilze
    digitalWrite(startDelay, LOW);     // disconnect amp from output
    delay(100);                        // wait to stabilze
    digitalWrite(directOutState, LOW); // switch relay off
    digitalWrite(headphoneOnOff, LOW); // switch relay off
    Screen.clearDisplay();             // clear display
    Screen.setPowerSave(1);            // turn screen off
    digitalWrite(powerOnOff, LOW);     // make pin of power on low, power of amp is turned off
 #ifdef debugPreAmp                    // if debugPreAmp enabled write message
    Serial.println(F(" status is now standby "));
 #endif
  } 
  else {
 #ifdef debugPreAmp  // if debugPreAmp enabled write message
    Serial.print(F(" changeStandby : moving to active,  "));
 #endif
    Amp.Alive = true;                         // preamp is alive
    EEPROM.put(0, Amp);                       // write new status to EEPROM
    digitalWrite(ledStandby, LOW);            // turn off standby led to indicate device is powered on
    digitalWrite(powerOnOff, HIGH);           // make pin of standby high, power of amp is turned on
    delay(1000);                              // wait to stabilize
    Screen.setPowerSave(0);                   // turn screen on
    waitForXseconds();                        // wait to let amp warm up
    digitalWrite(startDelay, HIGH);           // connect amp to output
    delay(100);                               // wait to stabilize
    if (Amp.DirectOut) {                      // if DirectOut is active set port high to active relay
      digitalWrite(directOutState, HIGH);     // turn the preamp in DirectOut mode
    }

    if (Amp.HeadPhoneActive) {                // headphone is active
     digitalWrite(headphoneOnOff, HIGH);      // switch headphone relay on
    }
    if (Amp.VolPerChannel) {                               // if we have a dedicated volume level per channel give attenuatorlevel the correct value
     attenuatorMain = VolLevels[Amp.SelectedInput];        // if vol per channel select correct level
    } 
    else {                                               // if we dont have the a dedicated volume level per channel
     attenuatorMain = VolLevels[0];                        // give attenuator level the generic value
    }
    defineVolume(0);                                   // define new volume level
    if (!muteEnabled) {                                // if mute not enabled write volume to relay
      setRelayChannel(Amp.SelectedInput);              // select correct input channel
      delay(delayPlop);                                // wait to stabilize
      setRelayVolume(attenuatorLeft, attenuatorRight);
    }
    else {
      setRelayVolume(0, 0);      // switch volume off
      setRelayChannel(0);        // set relays to no input channel
    }

    writeFixedValuesScreen();                 //display info on oled screen
    writeVolumeScreen(attenuatorMain);        // display volume level on oled screen
 #ifdef debugPreAmp                           // if debugPreAmp enabled write message
    Serial.println(F(" status is now active "));
 #endif
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Interrupt Service Routine for a change to Rotary Encoder pin A
void rotaryTurn() {
  pinAstateCurrent = digitalReadFast(rotaryPinA);    // Lees de huidige staat van Pin A
  pinBstateCurrent = digitalReadFast(rotaryPinB);
  if ((pinAStateLast == LOW) && (pinAstateCurrent == HIGH)) {
    if (pinBstateCurrent == HIGH) {attenuatorChange--;}
    else {attenuatorChange++;}
  }
  else { 
    if (pinBstateCurrent == LOW) {attenuatorChange--;}
    else {attenuatorChange++;} 
  }
  pinAStateLast = pinAstateCurrent; 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function to change to next inputchannel
void changeInput(int change) {
 #ifdef debugPreAmp  // if debugPreAmp enabled write message                                                
  Serial.print(F(" changeInput: old selected Input: "));
  Serial.print(Amp.SelectedInput);
 #endif
  if (muteEnabled) {  // if mute enabled and change volume change mute status to off
    changeMute();     // disable mute
  }
  Amp.SelectedInput = Amp.SelectedInput + change;        // increase or decrease current channel by 1
  if (Amp.SelectedInput > 4) { Amp.SelectedInput = 1; }  // implement round robbin for channel number
  if (Amp.SelectedInput < 1) { Amp.SelectedInput = 4; }
  EEPROM.put(0, Amp);                               // save new channel number in eeprom
  if (Amp.VolPerChannel) {                          // if we have a dedicated volume level per channel give attenuatorlevel
    attenuatorMain = VolLevels[Amp.SelectedInput];  // if vol per channel select correct level
  }
  setRelayVolume(0, 0);                     // set volume to zero
  delay(delayPlop);                         // wait  to stabilize
  setRelayChannel(Amp.SelectedInput);       // set relays to new input channel
  delay(100);                               // wait to stabilize
  defineVolume(0);                          // define new volume level
  if (!muteEnabled) {                       // if mute not enabled write volume to relay
    setRelayVolume(attenuatorLeft, attenuatorRight);
  }
  writeFixedValuesScreen();                   //display info on oled screen
  writeVolumeScreen(attenuatorMain);          // update screen
 #ifdef debugPreAmp                           // if debugPreAmp enabled write message
  Serial.print(F(", new Selected Input: "));  // write new channel number to debugPreAmp
  Serial.println(Amp.SelectedInput);
 #endif
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Functions to change volume
void defineVolume(int increment) {
  int attenuatorLeftOld = attenuatorLeft;    // save old value left channel
  int attenuatorRightOld = attenuatorRight;  // save old value right channel
  volumeChanged = true;                      // assume volume is changed
  if ((muteEnabled) && (increment != 0)) {   // if mute enabled and change volume change mute status to off
    changeMute();                            // disable mute
  }
 #ifdef debugPreAmp  // if debugPreAmp enabled write message
  Serial.print(F("defineVolume: volume was : "));
  Serial.println(attenuatorMain);
 #endif
  attenuatorMain = attenuatorMain + increment;  // define new attenuator level
  if (attenuatorMain > 63) {                    // volume cant be higher as 63
    attenuatorMain = 63;
    volumeChanged = false;                      //as attenuator level was already 63 no use to write
  }
  if (attenuatorMain < 0) {                     // volume cant be lower as 0
    attenuatorMain = 0;
    volumeChanged = false;                      //as attenuator level was already 0 no use to write
  }
  if (volumeChanged) {                          // als volume veranderd is
    if (attenuatorMain == 0) {                  // If volume=0  both left and right set to 0
      attenuatorRight = 0;
      attenuatorLeft = 0;
    } 
    else if (!daughterBoard) {                // if  no daughterboard we ignore balance
      attenuatorRight = attenuatorMain;
      attenuatorLeft = attenuatorMain;
    } 
    else {                                                      //amp in normal state, we use balance
      int balanceRight = ((Amp.BalanceOffset) >> 1);              // divide BalanceOffset using a bitshift to the right
      int balanceLeft = balanceRight - (Amp.BalanceOffset);       // balance left is the remaining part of
      attenuatorRight = attenuatorMain + balanceRight;            // correct volume right with balance
      attenuatorLeft = attenuatorMain + balanceLeft;              // correct volume left with balance
      if (attenuatorRight > 63) { attenuatorRight = 63; }         // volume higher as 63 not allowed
      if (attenuatorLeft > 63) { attenuatorLeft = 63; }           // volume higher as 63 not allowed
      if (attenuatorRight < 0) { attenuatorRight = 0; }           // volume lower as 0 not allowed
      if (attenuatorLeft < 0) { attenuatorLeft = 0; }             // volume lower as 0 not allowed
    }
    attenuatorLeftTmp = attenuatorLeft & attenuatorLeftOld;       //define intermediate volume levels to prevent plop
    attenuatorRightTmp = attenuatorRight & attenuatorRightOld;    //define intermediate volume levels to prevent plop
 #ifdef debugPreAmp                                               // if debugPreAmp enabled write message
    Serial.print(F("defineVolume: volume is now : "));
    Serial.print(attenuatorMain);
    Serial.print(F(" volume left = "));
    Serial.print(attenuatorLeft);
    Serial.print(F(". Bytes, old, temp, new "));
    Serial.print(attenuatorLeftOld, BIN);
    Serial.print(" , ");
    Serial.print(attenuatorLeftTmp, BIN);
    Serial.print(" , ");
    Serial.print(attenuatorLeft, BIN);
    Serial.print(F(" ,volume right = "));
    Serial.print(attenuatorRight);
    Serial.print(F(".  Bytes, old, temp, new "));
    Serial.print(attenuatorRightOld, BIN);
    Serial.print(" , ");
    Serial.print(attenuatorRightTmp, BIN);
    Serial.print(" , ");
    Serial.print(attenuatorRight, BIN);
    Serial.print(F(",  headphones : "));
    Serial.println(Amp.HeadPhoneActive);
 #endif
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Function to change mute
void changeMute() {
  if (muteEnabled) {                          // if mute enable we turn mute off
    muteEnabled = false;                      // change value of boolean
    setRelayChannel(Amp.SelectedInput);  // set relays to support original inputchnannel
    delay(100);
    defineVolume(0);                          // define new volume level
    setRelayVolume(attenuatorLeft, attenuatorRight);
    writeFixedValuesScreen();                 //display info on  screen
    writeVolumeScreen(attenuatorMain);
 #ifdef debugPreAmp  // if debugPreAmp enabled write message
    Serial.print(F("ChangeMute : Mute now disabled, setting volume to: "));
    Serial.println(attenuatorMain);
 #endif
  } 
  else {
    muteEnabled = true;        // mute is not enabled so we turn mute on
    setRelayVolume(0, 0);      // switch volume off
    delay(delayPlop);          // set relays to zero volume
    setRelayChannel(0);        // set relays to no input channel
    writeFixedValuesScreen();  //display info on oled screen
    writeVolumeScreen(0);
 #ifdef debugPreAmp            // if debugPreAmp enabled write message
    Serial.println(F("changeMute: Mute enabled"));
 #endif
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// write stable values on the screen (mute, headphones, passive)
void writeFixedValuesScreen() {
  Screen.clearDisplay();
  Screen.setFont(fontgrahp);
  if (Amp.HeadPhoneActive) {                    // headphone is active
    Screen.drawGlyph(0, 36, 0x0043);            // write headphone symbol
  } 
  else if (Amp.DirectOut) {                   // amp is direct out
    Screen.drawTriangle(8, 16, 18, 26, 8, 36);  // write amp sign
    Screen.drawLine(2, 26, 22, 26);
    Screen.setDrawColor(0);
    Screen.drawTriangle(10, 21, 15, 26, 10, 31);
    Screen.setDrawColor(1);
    Screen.drawLine(2, 35, 19, 18);
    Screen.drawLine(2, 34, 19, 17);
  } 
  else {  // amp is active write amp sign
    Screen.drawTriangle(8, 16, 18, 26, 8, 36);
    Screen.drawLine(2, 26, 22, 26);
    Screen.setDrawColor(0);
    Screen.drawTriangle(10, 21, 15, 26, 10, 31);
    Screen.setDrawColor(1);
  }
  Screen.setFont(fontH14);  // write inputchannel
  Screen.setCursor(0, 63);
  Screen.print(Amp.InputTekst[Amp.SelectedInput]);
  if (Amp.BalanceOffset != 0) {                   // write balance values
    int balanceRight = (Amp.BalanceOffset >> 1);  // divide BalanceOffset using a bitshift to the right
    int balanceLeft = balanceRight - Amp.BalanceOffset;
    Screen.setDrawColor(0);                       // clean balance part in buffer
    Screen.drawBox(0, 0, 128, 10);
    Screen.setDrawColor(1);
    Screen.setFont(fontH10);
    Screen.setCursor(0, 10);
    Screen.print("L:");
    Screen.setFont(fontH10figure);
    Screen.print(chvolInChar2(balanceLeft));
    Screen.setFont(fontH10);
    Screen.setCursor(98, 10);
    Screen.print("R:");
    Screen.setFont(fontH10figure);
    Screen.print(chvolInChar2(balanceRight));
    Screen.sendBuffer();
  } 
  else {
    Screen.setDrawColor(0);                               // clean balance part in buffer
    Screen.drawBox(0, 0, 128, 10);
    Screen.setDrawColor(1);
  }
  Screen.sendBuffer();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// intialisation of the screen after powerup of screen.
void oledSchermInit() {
  digitalWrite(oledReset, LOW);                                        // set screen in reset mode
  delay(15);                                                           // wait to stabilize
  digitalWrite(oledReset, HIGH);                                       // set screen active
  delay(15);                                                           // wait to stabilize
  Screen.setI2CAddress(oledI2CAddress * 2);                            // set oled I2C address
  Screen.begin();                                                      // init the screen
  Screen.setContrast((((Amp.ContrastLevel * 2) + 1) << 4) | 0x0f);     // set contrast level, reduce number of options
  Screen.sendBuffer();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// write volume level to screen
void writeVolumeScreen(int volume) {
  int offset = -63;
  if (!Amp.DirectOut) {        // if the amp is not in direct out mode adjust volume displayed on screen
    offset = offset + Amp.PreAmpGain;
  }
  volume = volume + offset;    // change volumelevel shown on screen, adding offset
  Screen.setDrawColor(0);      // clean volume part in buffer
  Screen.drawBox(35, 13, 80, 31);
  Screen.setDrawColor(1);
  if (muteEnabled) {           // if mute is enabled write MUTE
    Screen.setFont(fontH14);
    Screen.drawStr(35, 34, "MUTE");
  }
  else {                            // mute is not enabled, write volume on screen.
    Screen.setFont(fontH21cijfer);  // write volume
    Screen.setCursor(35, 37);
    Screen.print(chvolInChar3(volume));
    Screen.setFont(fontH10);
    Screen.drawStr(87, 37, "dB");
  }
  Screen.sendBuffer();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// set relays in status to support requested volume
void setRelayVolume(int relayLeft, int relayRight) {
  Wire.beginTransmission(mcp23017I2CAddressBottom);  // write volume left to left ladder
  Wire.write(0x13);
  Wire.write(relayLeft);
  Wire.endTransmission();
  if (daughterBoard) {                                 // if we have second board
    Wire.beginTransmission(mcp23017I2CAddressTop);  // write volume right to right ladder
    Wire.write(0x13);
    Wire.write(relayRight);
    Wire.endTransmission();
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// set relays in status to support requested channnel
void setRelayChannel(uint8_t relay) {
  uint8_t inverseWord;
  if (relay == 0) {      // if no channel is selected, drop all ports, this is done during  mute
    inverseWord = 0xef;  // we write inverse, bit 4 = 0 as this one is not inverted
  } 
  else {
    inverseWord = 0xFF ^ (0x01 << (relay - 1));                           // bitshift the number of ports to get a 1 at the correct port and inverse word
    if ((1 == bitRead(inputPortType, (relay - 1))) && (daughterBoard)) {  // determine if this is an xlr port
      bitClear(inverseWord, 4);                                           // if XLR set bit 4 to 0
    }
  }
  Wire.beginTransmission(mcp23017I2CAddressBottom);  // write port settings to board
  Wire.write(0x12);
  Wire.write(inverseWord);
  Wire.endTransmission();
  if (daughterBoard) {                                 // if we have an additional board
    Wire.beginTransmission(mcp23017I2CAddressTop);  // write port settings to top board (right channel)
    Wire.write(0x12);
    Wire.write(inverseWord);
    Wire.endTransmission();
  }
 #ifdef debugPreAmp  // if debugPreAmp enabled write message
  Serial.print(F("setRelayChannel: bytes written to relays : "));
  Serial.println(inverseWord, BIN);
 #endif
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//initialize the MCP23017 controling the relays;
void mCP23017init(uint8_t mCP23017I2Caddress) {
  Wire.beginTransmission(mCP23017I2Caddress);
  Wire.write(0x00);  // IODIRA register, input port
  Wire.write(0x00);  // set all of port B to output
  Wire.endTransmission();
  Wire.beginTransmission(mCP23017I2Caddress);
  Wire.write(0x12);  // gpioA
  Wire.write(0xEF);  // set all ports high except bit 4
  Wire.endTransmission();
  Wire.beginTransmission(mCP23017I2Caddress);
  Wire.write(0x01);  // IODIRB register, volume level
  Wire.write(0x00);  // set all of port A to outputs
  Wire.endTransmission();
  Wire.beginTransmission(mCP23017I2Caddress);
  Wire.write(0x13);  // gpioB
  Wire.write(0x00);  // set all ports low,
  Wire.endTransmission();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// detect log time press on remote controle
bool detectLongPress(uint16_t aLongPressDurationMillis) {
  if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) {        // if repeat and not detected yet
    if (millis() - aLongPressDurationMillis > milliSOfFirstReceive) {  // if this status takes longer as..
      longPressJustDetected = true;                                    // longpress detected
    }

  } 
  else {                             // no longpress detected
    milliSOfFirstReceive = millis();  // reset value of first press
    longPressJustDetected = false;    // set boolean
  }
  return longPressJustDetected;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// detect if the EEPROM contains an init config
void checkIfEepromHasInit() {
  char versionOfData[9] = "PreAmpV3";                            // unique string to check if eeprom already written
  EEPROM.get(0, Amp);                                      // get variables within init out of EEPROM
  for (byte index = 1; index < 9; index++) {                    // loop to compare strings
    if (Amp.UniqueString[index] != versionOfData[index]) {  // compare if strings differ, if so
      writeEEprom();                                            // write eeprom
      break;                                                    // jump out of for loop
    }
 #ifdef debugPreAmp
    if (index == 8) {  // index 8 is only possible if both strings are equal
      Serial.println(F("CheckIfEepromHasInit: init configuration found"));
    }
 #endif
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// write the EEProm with the correct values
void writeEEprom() {
 #ifdef debugPreAmp
  Serial.println(F("WriteEEprom: no init config detected, writing new init config"));
 #endif
  SavedData start = {
    "PreAmpV3",  // unique string
    false,       // boolean, volume level per channel
    1,           // channel used for start
    0,           // balance offset
    5,           // ContrastLevel
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
char* chvolInChar2(int volume) {
  if (volume == 0) {
    strcpy(volInChar, " 0");
    return (volInChar);
  } 
  else {
    if (volume < 0) strcpy(volInChar, "-");
    if (volume > 0) strcpy(volInChar, "+");
    char buf[2];
    sprintf(buf, "%i", abs(volume));
    strcat(volInChar, buf);
    return (volInChar);
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////
// change format of volume for displaying on screen, 3 chars
char* chvolInChar3(int volume) {
  if (volume == 0) {
    strcpy(volInChar, "  0");
    return (volInChar);
  } 
  else {
    if (volume < 0) strcpy(volInChar, "-");
    if (volume > 0) strcpy(volInChar, "+");
    if ((volume > -10) && (volume < 10)) {
      strcat(volInChar, " ");
      char buf[2];
      sprintf(buf, "%i", abs(volume));
      strcat(volInChar, buf);
      return (volInChar);
    } 
    else {
      char buf[3];
      sprintf(buf, "%i", abs(volume));
      strcat(volInChar, buf);
      return (volInChar);
    }
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////
//  debugPreAmp proc to show content of eeprom
#ifdef debugPreAmp
 void listContentEEPROM() {
  Serial.print(F("unique string         : "));
  Serial.println(Amp.UniqueString);
  Serial.print(F("volume per channel    : "));
  Serial.println(Amp.VolPerChannel);
  Serial.print(F("selected input chan   : "));
  Serial.println(Amp.SelectedInput);
  Serial.print(F("Balance offset        : "));
  Serial.println(Amp.BalanceOffset);
  Serial.print(F("ContrastLevel screen  : "));
  Serial.println(Amp.ContrastLevel);
  Serial.print(F("Volume offset         : "));
  Serial.println(Amp.PreAmpGain);
  Serial.print(F("startup delay         : "));
  Serial.println(Amp.startDelayTime);
  Serial.print(F("Headphones active     : "));
  Serial.println(Amp.HeadPhoneActive);
  Serial.print(F("DirectOut active      : "));
  Serial.println(Amp.DirectOut);
  Serial.print(F("Prev stat DirectOut   : "));
  Serial.println(Amp.PrevStatusDirectOut);
  Serial.print(F("Pre amp active/standby: "));
  Serial.println(Amp.Alive);
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
  Serial.println(Amp.InputTekst[0]);
  Serial.print(F("Name channel 1        : "));
  Serial.println(Amp.InputTekst[1]);
  Serial.print(F("Name channel 2        : "));
  Serial.println(Amp.InputTekst[2]);
  Serial.print(F("Name channel 3        : "));
  Serial.println(Amp.InputTekst[3]);
  Serial.print(F("Name channel 4        : "));
  Serial.println(Amp.InputTekst[4]);
 }
#endif
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// function to check i2c bus
#ifdef debugPreAmp
 void scanI2CBus() {
  uint8_t error;                                                      // error code
  uint8_t address;                                                    // address to be tested
  int numberOfDevices;                                                // number of devices found
  Serial.println(F("ScanI2CBus: I2C addresses defined within the code are : ")); // print content of code
  Serial.print(F("Screen             : 0x"));
  Serial.println(oledI2CAddress, HEX);
  Serial.print(F("Ladderboard bottom : 0x"));
  Serial.println(mcp23017I2CAddressBottom, HEX);
  if (daughterBoard) {
    Serial.print(F("Ladderboard top    : 0x"));
    Serial.println(mcp23017I2CAddressTop, HEX);
  } 
  else {
    Serial.println(F("No top board defined "));
  }
  Serial.println(F("ScanI2C: Scanning..."));
  numberOfDevices = 0;
  for (address = 1; address < 127; address++) {                                   // loop through addresses
    Wire.beginTransmission(address);                                              // test address
    error = Wire.endTransmission();                                               // resolve errorcode
    if (error == 0) {                                                             // if address exist code is 0
      Serial.print("I2C device found at address 0x");                             // print address info
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
      numberOfDevices++;
    }
  }
  if (numberOfDevices == 0) {
    Serial.println(F("scanI2C:No I2C devices found"));
  }
  else {
    Serial.print(F("scanI2CBus: done, number of device found : "));
    Serial.println(numberOfDevices);
  }
 }
#endif