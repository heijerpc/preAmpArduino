///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// v0.2, integration of oled screen
// v0.3 optimize and change, adapt volume part for screen and rotator
// v0.4 included functions to support relay, include 2 buttons (mute and channel) and restructured setup due to lack of readabilitie
// v0.5 included IR support, fixed issue with LSB B bank controling volume. 
// v0.6 included eeprom support, 3 to 4 channels, headphone support, passiveAmp support,
// v0.7 added IR codes, changed channelinput to + and -, added long delay to led preamp stabilize, added menu, changed array and struct in eeprom
//      changed oledscreen type and procedures to write data to screen
// v0.81 added portselection to second board, if volume >63 <0 dont do anything.
// v0.9  changed oled screen to full graph
//       changed setup of volume as we had large blups during volume change
//       changed init string for 017chip
//       changed setup menu, quit after 90 sec, return to orginal volume 
//       change volume changes if volume below 0 or above 63
//       changed again volume setup using and function and previous volume level
//       included eeprom programming
//       changed proc how to handle keeping button pressed on IR controler
//        to do: full graph/, update comments,

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// below definitions could be change by user depending on setup, no code changes needed
bool debugEnabled = true  ;                             // boolean, defines if debug is enabled, either true or false
bool daughterboard = true;                            // boolean, defines if a daughterboard is used to support XLR and balance, either true or false
const uint8_t ContrastLevel=0x3f;                     // level of contrast, between 00 anf ff, default is 7f
const uint8_t Voloffset=10;                           // vol on screen is between -63 and 0, Voloffset is added to value displayed on the screen
const char* inputTekst[5] = {                         // definitions used to display, content could be changed, max length ???
  // generic volume, used in setup menu to define general volume, used when amp powers up and no vol per channel is selected                                  
  "General volume",                                 
  //channel 1, tekst on display while using channel 1, this is a XLR input channel                                         
  "XLR  1",
  //channel 2, tekst on display while using channel 2, this is a XLR input channel
  "XLR  2",
  //channel 3, tekst on display while using channel 3, this is a RCA input channel
  "RCA  1",
  // channel 4, tekst on display while using channel 4, this is a RCA input channel
  "RCA  2"
  };

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// definitions for EPROM writing
#include <EEPROM.h>
struct SavedData {                                    // definition of the data stored in eeprom
  char uniquestring[7];                               // unique indentifier to see if eeprom is programmed
  bool VolPerChannel;                                 // boolean, if True volume level per channel is defined, otherwise volume stays as is
  uint8_t SelectedInput;                              // last selected input
  int BalanceOffset;                                  // balance offset of volume, difference between left and right
  int BalanceMultiplier;                              // balance mutiplier
  bool HeadphoneActive;                               // boolean, headphones active, no output to amp
  bool AmpPassive;                                    // boolean, preamp is used passive or active mode
  };
SavedData InitData;                                   // initdata is a structure defined by SavedData containing the values
int VolLevels [5];                                    // Vollevels is an array storing initial volume level when switching to channel, used if volperchannel is true
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// pin definitions
#define PowerOnOff     A0                             // pin connected to the relay handeling power on/off of the amp
#define HeadphoneOnOff   A1                           // pin connected to the relay handeling headphones active / not active
#define AmpPassiveState  A2                           // pin connected to the relay handeling passive/active state of amp
#define StartDelay A3                                 // pin connected to the relay which connects amp to output
volatile int rotaryPinA = 3;                          // encoder pin A, volatile as its addressed within the interupt of the rotary
volatile int rotaryPinB = 4;                          // encoder pin B,
#define rotaryButton   5                              // pin is connected to the switch part of the rotary
#define buttonChannel  6                              // pin is conncected to the button to change input channel round robin, pullup
#define buttonStandby  7                              // pin is connected to the button to change standby on/off, pullup
#define buttonHeadphone 8                             // pin  is connected to the button to change headphone on/off, pullup
#define buttonPassive 9                               // pin  is connected to the button to change preamp passive on/off, pullup
#define buttonMute  10                                // pin  is connected to the button to change mute on/off, pullup
#define ledStandby  11                                // connected to a led that is on if amp is in standby
#define oledReset 12                                  // connected to the reset port of Oled screen, used to reset Oled screen
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// general definitions 
#include <Wire.h>                                     // include functions for i2c
const char* initTekst = "V 0.92";                     // current version of the code, shown in startscreen, content should be changed in this line
bool Alive = true;                                    // boolean, defines if we are in standby mode or acitve
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// definitons for the IR receiver
#define DECODE_NEC                                    // only add NEC prot to support apple remote, saves memory
#include <IRremote.hpp>                               // include drivers for IR 
#define IR_RECEIVE_PIN 2                              // datapin of IR is connected to pin 2
#define apple_left 9                                  // below the IR codes received for apple and ???
#define philips_left 33
#define apple_right 6                            
#define philips_right 32
#define apple_up 10                         
#define philips_up 136
#define apple_down 12                           
#define philips_down 140
#define apple_middle 92                            
#define apple_menu 3                                
#define apple_forward 95                              
int delaytimer;                                       // timer to delay between volume changes using IR, actual value set in main loop
unsigned long sMillisOfFirstReceive;                  // used within IR procedures to determine if command is a repeat
bool sLongPressJustDetected;                          // used within IR procedures to determine if command is a repeat
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// definitions for the oled screen
#define OLED_Address 0x3C                             // 3C is address used by oled controler
#define U8X8_DO_NOT_SET_WIRE_CLOCK                    // library setting to leave clock unchanged due to extenders.
#define U8X8_USE_ARDUINO_AVR_SW_I2C_OPTIMIZATION      // advised, no real clue why
#define font1x1 u8x8_font_pcsenior_r                  // used font for 1 x 1 characters
#define font1x2 u8x8_font_8x13B_1x2_r                 // used font for 1 x 2 characters
#define font2x3 u8x8_font_courB18_2x3_n               // used font for 2 x 3 characters
#include <U8x8lib.h>                                  // include graphical based character mode library
U8X8_SSD1309_128X64_NONAME0_HW_I2C u8x8;              // define the screen type used.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// definitions for the attenuator board
#define MCP23017_I2C_ADDRESS_bottom 0x25              // I2C address of the relay board bottom
//#define MCP23017_I2C_ADDRESS_bottom 0x24              // I2C address of the relay board bottom, jos
#define MCP23017_I2C_ADDRESS_top 0x26                 // I2C address of the relay board daughterboard
#define delay_plop 15                                 // delay timer between volume changes
int AttenuatorMain;                                   // actual internal volume level, 
int AttenuatorRight;                                  // actual volume value sent to relay
int AttenuatorLeft;                                   // actual volume value sent to relay
int AttenuatorRightTmp;                               // intermediate volume sent to relay
int AttenuatorLeftTmp;                                // intermeditate volume sent to relay
volatile int AttenuatorChange = 0;                    // change of volume out of interupt function
bool muteEnabled = false;                             // boolean, status of Mute
bool VolumeChanged = false;                           // defines if volume is changed
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// definitions for the compiler as we have nested functions
void defineVolume(int increment);                              // change volume
void rotaryTurn();                                             // interupt handler if rotary is turned
void writeOledVolume(int volume);                              // write volume level to screen
void writeOledScreenLeft();                                    // write info on left side screen (mute, passive, input)
void OledSchermInit();                                         // init procedure for Oledscherm
void MCP23017init(uint8_t MCP23017_I2C_ADDRESS);               // init procedure for relay extender
void setRelayChannel(uint8_t Word);                            // set relays to selected input channel
void setRelayVolume(int RelayLeft, int RelayRight);            // set relays to selected volume level
void changeStandby();                                          // changes status of preamp between active and standby
void changeHeadphone();                                        // change status of headphones between active and standby 
void changePassive();                                          // change status of preamp between active and passive
void changeMute();                                             // enable and disable mute
void changeInput(int change);                                  // change the input channel
void setupMenu();                                              // run the setup menu.
void waitForXseconds();                                        // wait for number of seconds, used to warm up amp
bool detectLongPress(uint16_t aLongPressDurationMillis);       // detect if multiple times key on remote controle is pushed
void checkIfEepromHasInit();                                   // check if EEPROM is configured correctly
void writeEEprom();                                            // if EEPROM doesnt contain content add content
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup()
{
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 
  // pin modes
  pinMode (PowerOnOff, OUTPUT);                                             //control the relay that provides power to the rest of the amp
  pinMode (HeadphoneOnOff, OUTPUT);                                         //control the relay that switches headphones on and off
  pinMode (AmpPassiveState, OUTPUT);                                        //control the relay that switches the preamp between passive and active
  pinMode (StartDelay, OUTPUT);                                             //control the relay that connects amp to output ports
  pinMode (rotaryPinA, INPUT_PULLUP);                                       // pin A rotary is high and its an input port
  pinMode (rotaryPinB, INPUT_PULLUP);                                       // pin B rotary is high and its an input port
  pinMode (rotaryButton, INPUT_PULLUP);                                     // input port and enable pull-up
  pinMode (buttonChannel, INPUT_PULLUP);                                    // button to change channel
  pinMode (buttonStandby, INPUT_PULLUP);                                    // button to go into standby/active
  pinMode (buttonHeadphone, INPUT_PULLUP);                                  // button to switch between AMP and headphone
  pinMode (buttonPassive, INPUT_PULLUP);                                    // button to switch between passive and active mode
  pinMode (buttonMute, INPUT_PULLUP);                                       // button to mute 
  pinMode (ledStandby, OUTPUT);                                             //led will be on when device is in standby mode
  pinMode (oledReset, OUTPUT);                                              // pinmode output to set reset of oled screen
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 
// write init state to output pins
  digitalWrite(PowerOnOff, LOW);                                            // keep amp turned off
  digitalWrite(HeadphoneOnOff, LOW);                                        // turn headphones off
  digitalWrite(AmpPassiveState, LOW);                                       // turn the preamp in active state
  digitalWrite(StartDelay, LOW);                                            // disconnect amp from output  
  digitalWrite(ledStandby, LOW);                                            // turn off standby led to indicate device is becoming active
  digitalWrite(oledReset, LOW);                                             // keep the Oled screen in reset mode
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // first power up pre-amp
  digitalWrite(PowerOnOff, HIGH);                                           // turn power relay amp on
  delay(2000);                                                              // wait till all powered up
   ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // enable debug to write to console if debug is enabled 
  if (debugEnabled) {                                                       // if debuglevel on start monitor screen
    Serial.begin (9600);
  }
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Set up pins for rotary:
  attachInterrupt(digitalPinToInterrupt(rotaryPinA), rotaryTurn, RISING);   // if pin encoderA changes run rotaryTurn
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // startup IR
  IrReceiver.begin(IR_RECEIVE_PIN);                                          // Start the IR receiver
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // initialize the oled screen and relay extender
  Wire.begin();
  delay(100);
  OledSchermInit();                                                         // intialize the Oled screen
  delay(100);
  MCP23017init(MCP23017_I2C_ADDRESS_bottom);                                // initialize the relays  
  if (daughterboard){                                                       // if we have a daughterboard
    MCP23017init(MCP23017_I2C_ADDRESS_top);                                 // initialize daugterboard
  }
  setRelayVolume(0,0);                                                      //set volume relays to 0, just to be sure
  delay(15);                                                                // wait to stabilize
  setRelayChannel(0);                                                       // disconnect all input channels
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // wait till amp is stable
  waitForXseconds();                                                        // wait to let amp warm up 
  digitalWrite(StartDelay, HIGH);                                           // connect amp to output 
  delay(100);                                                               // wait to stabilize
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 
  // // Set inital setup amp depending on data stored within eeprom
  checkIfEepromHasInit();                                                   // check if eeprom has config file, otherwise write config
  EEPROM.get(0, InitData);                                                  // get variables within init out of EEPROM
  EEPROM.get(30,VolLevels);                                                 // get the aray setting volume levels
  if (debugEnabled) {                                                       // if debug enabled write message
    Serial.println(F("initprog: the following values read from EEPROM")); 
    Serial.print(F("unique string         : "));
    Serial.println(InitData.uniquestring);
    Serial.print(F("volume per channel    : "));
    Serial.println(InitData.VolPerChannel); 
    Serial.print(F("selected input chan   : ")); 
    Serial.println(InitData.SelectedInput);  
    Serial.print(F("Balance offset        : ")); 
    Serial.println(InitData.BalanceOffset);  
    Serial.print(F("Balance multiplier    : ")); 
    Serial.println(InitData.BalanceMultiplier); 
    Serial.print(F("Headphones active     : ")); 
    Serial.println(InitData.HeadphoneActive); 
    Serial.print(F("AmpPassive active     : ")); 
    Serial.println(InitData.AmpPassive); 
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
  }
  if (InitData.AmpPassive) {                                                // if amppassive is active set port high to active relay
    digitalWrite(AmpPassiveState, HIGH);                                    // turn the preamp in passive mode
  }
  setRelayChannel(InitData.SelectedInput);                                  // select stored input channel
  delay(15);                  
  if (InitData.HeadphoneActive) {                                           // headphone is active
    digitalWrite(HeadphoneOnOff, HIGH);                                     // switch headphone relay on
  } 
  if (InitData.VolPerChannel) {                                             // if we have a dedicated volume level per channel give attenuatorlevel the correct value
    AttenuatorMain=VolLevels[InitData.SelectedInput];                       // if vol per channel select correct level
  }
  else {                                                                    // if we dont have the a dedicated volume level per channel
    AttenuatorMain=VolLevels[0];                                            // give attenuator level the generic value
  }
  defineVolume(0);                                                          // calculate correct volume levels
  setRelayVolume(AttenuatorLeft,AttenuatorRight);                           // set relays to the correct level
  writeOledScreenLeft();                                                    //display info on oled screen
  writeOledVolume(AttenuatorMain);                                          //display volume level on oled screen
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Main loop
//////////////////////////////////////////////////////////////////////////////////////////////
void loop()
{
  if (Alive) {                                                 // we only react if we are in alive state, not in standby
    if (AttenuatorChange != 0) {                               // if AttenuatorChange is changed by the interupt we change  volume
      defineVolume(AttenuatorChange);                          // Attenuatorchange is either pos or neg, calculate correct volume levels
      if (VolumeChanged) {
        setRelayVolume(AttenuatorLeftTmp,AttenuatorRightTmp);  //  set relays to the temp  level
        delay(delay_plop);
        setRelayVolume(AttenuatorLeft,AttenuatorRight);       // set relay to the correct level
        writeOledVolume(AttenuatorMain);                      // display volume level on oled screen
      }
      AttenuatorChange = 0;                                    // reset the value to 0
    }
    if (digitalRead(buttonMute) == LOW) {                      // if button mute is pushed
      delay(500);                                              // wait to prevent multiple switches
      changeMute();                                            // change status of mute
    }
    if (digitalRead(buttonChannel) == LOW) {                   // if button channel switch is pushed
      delay(500);                                              // wait to prevent multiple switches
      changeInput(1);                                          // change input channel
    }
    if (digitalRead(buttonHeadphone) == LOW) {                 // if button headphones switch is pushed
      delay(500);                                              // wait to prevent multiple switches
      changeHeadphone();                                       // change input 
    }
    if (digitalRead(buttonPassive) == LOW) {                   // if button passive switch is pushed
      delay(500);                                              // wait to prevent multiple switches
      changePassive();                                         // change active/passive state
    }  
    if (digitalRead(rotaryButton) == LOW) {                    // if rotary button is pushed go to setup menu
      delay(500);                                              // wait to prevent multiple switches
      setupMenu();                                             // start setup menu
    }   
  }
  if (digitalRead(buttonStandby) == LOW) {                   // if button standby is is pushed
      delay(500);                                            // wait to prevent multiple switches
      changeStandby();                                       // changes status
  
  }
  if (IrReceiver.decode()) {                                 // if we receive data on the IR interface
    if (detectLongPress(1500)) {                             // simple function to increase speed of volume change by reducing wait time
      delaytimer=50;
    } else {
      delaytimer=500;
    }
 //   Serial.println(IrReceiver.decodedIRData.command);
    switch (IrReceiver.decodedIRData.command){               // read the command field containing the code sent by the remote
      case apple_up:
        if (Alive) {                                         // we only react if we are in alive state, not in standby
          defineVolume(1);                                   // calculate correct volume levels, plus 1
          if (VolumeChanged) {
            setRelayVolume(AttenuatorLeftTmp,AttenuatorRightTmp); //  set relays to the temp  level
            delay(delay_plop);
            setRelayVolume(AttenuatorLeft,AttenuatorRight);    // set relay to the correct level
            writeOledVolume(AttenuatorMain);                  // display volume level on oled screen
          }
          delay(delaytimer);
        }
        break;
      case apple_down:
        if (Alive) {                                        // we only react if we are in alive state, not in standby
          defineVolume(-1);                                  // calculate correct volume levels, minus 1
          if (VolumeChanged) {
            setRelayVolume(AttenuatorLeftTmp,AttenuatorRightTmp); //  set relays to the temp  level
            delay(delay_plop);
            setRelayVolume(AttenuatorLeft,AttenuatorRight);    // set relay to the correct level
            writeOledVolume(AttenuatorMain);                  // display volume level on oled screen
          }
          delay(delaytimer);
        }
        break;
      case apple_left:
        if (Alive) {                                         // we only react if we are in alive state, not in standby
           changeInput(-1);                                  // change input channel
           delay(300);
        }
        break;
      case apple_right:
        if (Alive) {                                         // we only react if we are in alive state, not in standby
           changeInput(1);                                   // change input channel 
           delay(300);
        }
        break;       
      case apple_forward:
        changeStandby();                                     // switch status of standby
        break;
      case apple_middle: 
        if (Alive) {                                         // we only react if we are in alive state, not in standby
          changeMute();                                      // change mute
          delay(300); 
        }
        break;
      case apple_menu: 
        if (Alive) {                                         // we only react if we are in alive state, not in standby
          changePassive();                                      // change mute
          delay(300); 
        }
        break;
      } 
    IrReceiver.resume();                                      // open IR receiver for next command
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// delay the startup to wait for pre amp to stabilize, clears screen at start and end of proc 
void waitForXseconds() {
  char valueinchar[3]; 
  u8x8.clear(); 
  u8x8.setFont(font1x2);                                          // set font 
  u8x8.setCursor(5, 3);                                           // set cursor position
  u8x8.print(initTekst);                                          // write version number
  delay(1000);
  if (debugEnabled) {                                             // if debug enabled write message
    Serial.println (F("waitForXseconds: waiting for preamp to stabilize "));
  }
  u8x8.clearDisplay();                                            // clear screen
  u8x8.drawString(3, 2, "please wait");                           // write please wait
  for (int i=10; i>0;i--) {                                       // run for 10 times, waiting in total 10 seconds
    sprintf(valueinchar, "%02d", i);                              // convert int to char array
    u8x8.setCursor(7, 5);                                         // set cursor at position
    u8x8.print(valueinchar);                                      //
    delay(1000);                                                  // delay for a second
  }  
  u8x8.clearDisplay();                                            // clear screen
  if (debugEnabled) {
    Serial.println (F("waitForXseconds: preamp wait time expired "));
  }
}                                                                 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// procedure to handle change of headphones status                  
void changeHeadphone()
{
  if (InitData.HeadphoneActive) {                                            // headphone is active so we have to change to inactive
    if (debugEnabled) {
      Serial.print(F(" changeHeadphone : moving from active to inactive,  "));
    }
    InitData.HeadphoneActive=false;                                          // status off headphone active changed to false
    EEPROM.put(0,InitData);                                                  // write new status to EEPROM
    setRelayVolume(0,0);                                                     // switch volume off
    delay(15);                                                               // wait to stabilize
    digitalWrite(HeadphoneOnOff, LOW);                                       // switch headphones off
    delay(100);                                                              // wait to stabilize
    defineVolume(0);                                                         // define new volume levels
    setRelayVolume(AttenuatorLeft,AttenuatorRight);                          // set volume correct level
    writeOledScreenLeft();                                                   //display info on oled screen
    writeOledVolume(AttenuatorMain);                                         //display volume on oled screen
    if (debugEnabled) {
      Serial.println (" status is now inactive ");
    }
  } else {                                                                   // headphone is not active so we have to make it active
    if (debugEnabled) {
      Serial.print(F(" changeHeadphone : moving from inactive to active,  "));
    }
    InitData.HeadphoneActive=true;                                           // status off headphone active changed to false
    EEPROM.put(0,InitData);                                                  // write new status to EEPROM
    setRelayVolume(0,0);                                                     // switch volume off
    delay(15);                                                                // wait to stabilize
    if(InitData.AmpPassive) {                                                // amp must be active if we use headphone
      InitData.AmpPassive=false;                                             // make ampassive false
      digitalWrite(AmpPassiveState, LOW);                                    // set relay low, amp is active
      delay(100);                                                            // wait to stabilize
    }
    digitalWrite(HeadphoneOnOff, HIGH);                                      // switch headphone relay on
    delay(100);                                                               // wait to stabilize
    defineVolume(0);                                                         // define new volume level
    if (!muteEnabled){                                                       // if mute not enabled write volume to relay
       setRelayVolume(AttenuatorLeft,AttenuatorRight);                                        
    }   
    writeOledScreenLeft();                                                   //display info on oled screen
    writeOledVolume(AttenuatorMain);                                        //display volume level on screen
    if (debugEnabled) {
      Serial.println (F(" status is now active "));
    }
  }
}  
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// procedure to switch between active and passive state of preamp              nog doen: scherm aansutren
void changePassive()                                        
{
  if (!InitData.HeadphoneActive) {                                             // only allow switch if headphones is not active
    if (InitData.AmpPassive) {                                                 // Amp is in passive state so we have to change to active
      if (debugEnabled) {
        Serial.print (F(" changePassive : moving from passive to active,  "));
      }
      InitData.AmpPassive=false;                                               // make the boolean the correct value
      EEPROM.put(0,InitData);                                                  // write new status to EEPROM
      setRelayVolume(0,0);                                                     // switch volume off
      delay(15);                                                                // wait to stabilize
      digitalWrite(AmpPassiveState, LOW);                                      // switch headphone relay off
      delay(100);                                                              // wait to stabilize
      defineVolume(0);                                                         // define new volume level
      if (!muteEnabled){                                                       // if mute not enabled write volume to relay
        setRelayVolume(AttenuatorLeft,AttenuatorRight);                                           
      }   
      writeOledScreenLeft();                                                   //display info on oled screen
      writeOledVolume(AttenuatorMain);                                        //display volume level on screen
      if (debugEnabled) {
        Serial.println (F(" status is now active "));
      }
    } else {                                                                   // Amp is in active mode so we have to change it to passive
      if (debugEnabled) {
        Serial.print (F(" changePassive : moving from active to passive,  "));
      }
      InitData.AmpPassive=true;                                                // make the boolean the correct value
      EEPROM.put(0,InitData);                                                  // write new status to EEPROM  
      setRelayVolume(0,0);                                                     // switch volume off
      delay(15);                                                                // wait to stabilize
      digitalWrite(AmpPassiveState, HIGH);                                     // switch headphone relay on
      delay(100);                                                              // wait to stabilize
      defineVolume(0);                                                         // define new volume level
      setRelayVolume(AttenuatorLeft,AttenuatorRight);  
      writeOledScreenLeft();                                                   //display info on oled screen
      writeOledVolume(AttenuatorMain);                                        //display volume level on screen
       if (debugEnabled) {
        Serial.println (F(" status is now passive "));
      }
    }
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// procedure to switch to setup menu to change values writen in eeprom
void setupMenu()
{
  int orgAttenuatorLevel = AttenuatorMain;                         // save orginal attenuator level
  int volume;                                                       // volume level
  bool write = true;                                                // used determine if we need to write volume level to screen 
  bool quit = false;
  int offset = -63;                                                 // offset to define vol level shown on screen
  unsigned long int idlePeriod = 60000;                             // idlePeriod you need to change something within menu otherwise quit menu
  unsigned long time_saved;
  u8x8.setFont(font1x2);                                            // set font 
  u8x8.clearDisplay();                                              // clear screen
  u8x8.drawString(3, 0, "setup menu");                              // write setup menu
//  set balance value
  if (daughterboard) {                                              // balance only set if we have a daughterboard
    u8x8.drawString(4, 2, "balance");                               // write balance to screen
    u8x8.drawString(1 ,5,"------+------");                          // write table to screen
    u8x8.drawString(7+InitData.BalanceOffset,7,"x");                // write current balance using position
    time_saved = millis();
    while (digitalRead(rotaryButton)==HIGH) {                       // as long as rotary button not pressed
      if (millis() > time_saved + idlePeriod){
        quit = true;
        break;
      }
      if (AttenuatorChange != 0) {                                  // if AttenuatorChange is changed using rotary 
        u8x8.drawString(7+InitData.BalanceOffset,7," ");            // remove the x from the screen as we expect a new one
        InitData.BalanceOffset=InitData.BalanceOffset + AttenuatorChange;  // Attenuatorchange is either pos or neg depending on rotation
        AttenuatorChange = 0;                                       // reset value
        if (InitData.BalanceOffset > 6) {                           // keep value between -6 and 6
          InitData.BalanceOffset  = 6;
          write = false;                                            // value was already 6, no use to write
        }  
        if (InitData.BalanceOffset  < -6) {
          InitData.BalanceOffset = -6;
          write = false;                                             // value was alread -6, no use to write
        }
        u8x8.drawString(7+InitData.BalanceOffset,7,"x");             // write current balance using position
        if (write) {
          defineVolume(0);
          setRelayVolume(AttenuatorLeftTmp,AttenuatorRightTmp);      //  set relays to the temp  level
          delay(delay_plop);
          setRelayVolume(AttenuatorLeft,AttenuatorRight);            // set new volume level with balance level
        }
        time_saved = millis();                                       // reset time now to current time
        write = true;
      }
    }
    delay(1000);
  }
//  set vol per channel yes or no

  u8x8.clearDisplay();                                              // clear screen
  u8x8.drawString(3, 0, "setup menu");                              // write setup menu
  u8x8.drawString(1, 2, "vol per channel");                         // write menu to screen
  u8x8.setCursor(6, 6);
  if (InitData.VolPerChannel) {u8x8.print("Yes");}                  // make current status displayable
  else{u8x8.print("No ");}
  time_saved = millis();
  while (digitalRead(rotaryButton)==HIGH) {                        // as long as button is not pressed
    if (millis() > (time_saved + idlePeriod)){
      quit = true;
      break;
    }
    if (AttenuatorChange != 0) {                                   // if rotary is turned
      InitData.VolPerChannel=(!InitData.VolPerChannel);            // flip status
      u8x8.setCursor(6, 6);
      if (InitData.VolPerChannel) {u8x8.print("Yes");}             // make current status displayable
      else{u8x8.print("No ");}
      AttenuatorChange = 0;                                        // reset attenuatorchange
      delay(500);                                                  // delay to prevent looping
      time_saved = millis();                                       // reset time now to current time
    }
  }
  delay(1000);

// set volume levels
  if (!quit) {
    if (!InitData.AmpPassive) {                                  // if the amp is not in passive mode adjust offset to display correct vol level
      offset = offset + Voloffset;                               // change volumelevel shown on screen, adding offset
    } 
    for (int i=0; i<5; i++){                                     // run loop for generic and 4 volumes per channel
      if (i!=0) {                                                // if we set generic we use current input channel
        setRelayChannel(i);                                      // otherwise select the correct channel
      }
      if ((i==0) and InitData.VolPerChannel) {                   // if we have vol per channel generic volume is of no use,
        continue;                                                // so skip to next channel
      }
      if (quit) {
        continue;
      }
      u8x8.clearDisplay();                                       // clear screen
      u8x8.drawString(3, 0, "setup menu");                       // write setup menu
      u8x8.setCursor(1, 2);
      u8x8.print(inputTekst[i]);                                 // write the name of the variable we are changing
      volume=VolLevels[i] + offset;  
      u8x8.setCursor(6, 6);
      u8x8.print("   ");
      u8x8.setCursor(6, 6);
      if (volume < 0) {u8x8.print(volume);}
      if (volume > 0 ) {u8x8.print("+");u8x8.print(volume);}
      if (volume == 0 ) {u8x8.print(" ");u8x8.print(volume);}
      time_saved = millis();
      while (digitalRead(rotaryButton)==HIGH) {                  // as long as rotary button not pressed
        if (millis() > time_saved + idlePeriod){
          quit = true;
          break;
        }
        if (AttenuatorChange != 0) {                             // if AttenuatorChange is changed using rotary
          VolLevels[i]=VolLevels[i]+AttenuatorChange;            // change AttenuatorLeft init
          AttenuatorChange = 0;                                  // reset attenuatorchange
          if (VolLevels[i] > 63) {                               // code to keep attenuator between 0 and 63
            VolLevels[i] = 63;
            write = false;  
          }  
          if (VolLevels[i] < 0) {
            VolLevels[i]= 0;
            write = false;  
          }
          if (write) {
            AttenuatorMain = VolLevels[i];                        // save orginal attenuator level 
            defineVolume(0);
            setRelayVolume(AttenuatorLeftTmp,AttenuatorRightTmp); //  set relays to the temp  level
            delay(delay_plop);
            setRelayVolume(AttenuatorLeft,AttenuatorRight);
            volume=VolLevels[i] + offset;                          // vol is de value displayed on the screen
            u8x8.setCursor(6, 6);
            u8x8.print("   ");
            u8x8.setCursor(6, 6);
            if (volume < 0) {u8x8.print(volume);}
            if (volume > 0 ) {u8x8.print("+");u8x8.print(volume);}
            if (volume == 0 ) {u8x8.print(" ");u8x8.print(volume);}
          }
          write = true;
          time_saved = millis();                                          // reset time now to current time
        }  
      }
      delay(500);                                                 // delay to prevent moving to fast
      if (!InitData.VolPerChannel) {                              // if we do not use volume per channel skip for loop
        break;
      }
    }
    delay(1000);
  }
  // save all data and restore sound and volume
  EEPROM.put(0,InitData);                                      // save new values
  EEPROM.put(30,VolLevels);                                    // save volume levels
  AttenuatorMain=orgAttenuatorLevel;                          // restore orginal attenuator level
  defineVolume(0);
  setRelayVolume(0,0);                                         // set volume level to 0
  delay(15);
  setRelayChannel(InitData.SelectedInput);                     // set amp on prefered channel
  delay(15);
  setRelayVolume(AttenuatorLeftTmp,AttenuatorRightTmp); //  set relays to the temp  level
  delay(delay_plop);
  setRelayVolume(AttenuatorLeft,AttenuatorRight);
  u8x8.clearDisplay();                                         // clear screen
  writeOledScreenLeft();                                       //displaya info on screen
  writeOledVolume(AttenuatorMain);                            //display volume level on oled screen
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// procedure to handle moving to and coming out of standby mode
void changeStandby()
{
  if (Alive) {                                                     // move from alive to standby, turn off screen, relays and amp
    if (debugEnabled) {
      Serial.print (F(" changeStandby : moving from active to standby,  "));
    }
    Alive=false;                                                   // we are in standby, alive is false
    setRelayVolume(0,0);                                           // set volume relays off
    delay(15);                                                      // wait to stabilize
    setRelayChannel(0);                                            // disconnect all input channels
    delay(100);                                                    // wait to stabilze
    digitalWrite(StartDelay, LOW);                                 // disconnect amp from output 
    delay(100);                                                    // wait to stabilze
    u8x8.setPowerSave(1);                                          // turn screen off
    digitalWrite(PowerOnOff, LOW);                                 // make pin of power on low, power of amp is turned off
    delay(3000);                                                   // we first wait for stable state
    digitalWrite(ledStandby, HIGH);                                // turn on standby led to indicate device is in standby
    if (debugEnabled) {
      Serial.println (F(" status is now standby "));
    }
  } 
  else {
    if (debugEnabled) {
      Serial.print (F(" changeStandby : moving from standby to active,  "));
    }
    Alive=true;                                                    // preamp is alive
    digitalWrite(ledStandby, LOW);                                 // turn off standby led to indicate device is powered on
    digitalWrite(PowerOnOff, HIGH);                                // make pin of standby high, power of amp is turned on
    delay(3000);                                                   // 
    u8x8.setPowerSave(0);                                          // turn screen on
    waitForXseconds();                                             // wait to let amp warm up 
    digitalWrite(StartDelay, HIGH);                                // connect amp to output 
    delay(100);                                                    // wait to stabilize
    setRelayChannel(InitData.SelectedInput);                       // select correct input channel
    delay(100);                                                    // wait to stabilize
    defineVolume(0);                                               // define new volume level
    if (!muteEnabled){                                             // if mute not enabled write volume to relay
       setRelayVolume(AttenuatorLeft,AttenuatorRight);                                    
    }   
    writeOledScreenLeft();                                         //display info on oled screen
    writeOledVolume(AttenuatorMain);                              // display volume level on oled screen
    if (debugEnabled) {
      Serial.println (F(" status is now active "));
    }
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Interrupt Service Routine for a change to Rotary Encoder pin A
void rotaryTurn()
{
  if (digitalRead (rotaryPinB)){
  	AttenuatorChange=AttenuatorChange-1;   // rotation left, turn volume down
  }
	else {
     AttenuatorChange=AttenuatorChange+1;  // rotatate right, turn volume Up!
  }  
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function to change to next inputchannel
void changeInput(int change)
{
  if (debugEnabled) {                                                       // if debug enabled write current state
    Serial.print (F(" changeInput: old selected Input: "));
    Serial.print (InitData.SelectedInput);
  }
  if (muteEnabled) {                                                       // if mute enabled and change volume change mute status to off
    changeMute();                                                           // disable mute 
  }
  InitData.SelectedInput=InitData.SelectedInput+change;                     // increase or decrease current channel by 1
  if (InitData.SelectedInput > 4) { InitData.SelectedInput = 1; }           // implement round robbin for channel number
  if (InitData.SelectedInput < 1) { InitData.SelectedInput = 4; }
  EEPROM.put(0,InitData);                                                  // save new channel number in eeprom
  if (InitData.VolPerChannel) {                                            // if we have a dedicated volume level per channel give attenuatorlevel 
    AttenuatorMain=VolLevels[InitData.SelectedInput];                     // if vol per channel select correct level
  }
  setRelayVolume(0,0);                                                       // set volume to zero
  delay(15);                                                               // wait 5ms to stabilize
  setRelayChannel(InitData.SelectedInput);                               // set relays to new input channel
  delay(100);                                                    // wait to stabilize
  defineVolume(0);                                               // define new volume level
  if (!muteEnabled){                                             // if mute not enabled write volume to relay
    setRelayVolume(AttenuatorLeft,AttenuatorRight);                                    
  }                                  
  writeOledScreenLeft();                                                   //display info on oled screen
  writeOledVolume(AttenuatorMain);                                          // update screen 
  if (debugEnabled) {
    Serial.print(F(", new Selected Input: "));                            // write new channel number to debug
    Serial.println (InitData.SelectedInput);
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Functions to change volume 
void defineVolume(int increment) 
{
  int AttenuatorLeftOld=AttenuatorLeft;                                     // save old values 
  int AttenuatorRightOld=AttenuatorRight;                                   // save old values
  VolumeChanged = true;
  if ((muteEnabled) && (increment!=0)) {                                    // if mute enabled and change volume change mute status to off
    changeMute();                                                           // disable mute 
  }
  if (debugEnabled) {
    Serial.print (F("DefineVolume: volume was : "));   
    Serial.println (AttenuatorMain); 
  }
  AttenuatorMain = AttenuatorMain + increment;                             // define new attenuator level
  if (AttenuatorMain > 63) {                                               // volume cant be higher as 63
    AttenuatorMain=63; 
    VolumeChanged = false;                                                    //as attenuator level was already 63 no use to write
  }
  if (AttenuatorMain < 0 ) {                                               // volume cant be lower as 0
    AttenuatorMain=0;
    VolumeChanged = false;                                                 //as attenuator level was already 0 no use to write 
  } 
  if (VolumeChanged){                                                      // als volume veranderd is
    if (AttenuatorMain==0) {                                               // If volume=0  both left and right set to 0
      AttenuatorRight=0;
      AttenuatorLeft=0;
    } 
    else if ((InitData.HeadphoneActive) or (!daughterboard)) {     // if headphone is active or no daughterboard we ignore balance
      AttenuatorRight=AttenuatorMain;
      AttenuatorLeft=AttenuatorMain;
    } 
    else {
      int BalanceRight = ((InitData.BalanceOffset * 2) >> 1);              // divide balanceOffset using a bitshift to the right    
      int BalanceLeft = BalanceRight - (InitData.BalanceOffset * 2);     // balance left is the remaining part of 
      AttenuatorRight = AttenuatorMain + BalanceRight;                   // correct volume right with balance
      AttenuatorLeft = AttenuatorMain + BalanceLeft;                     // correct volume left with balance
      if (AttenuatorRight > 63) {AttenuatorRight = 63;}                  // volume higher as 63 not allowed 
      if (AttenuatorLeft > 63) {AttenuatorLeft = 63;}                     // volume higher as 63 not allowed
      if (AttenuatorRight < 0) {AttenuatorRight = 0;}                     // volume lower as 0 not allowed
      if (AttenuatorLeft < 0) {AttenuatorLeft = 0;}                       // volume lower as 0 not allowed
    }
    AttenuatorLeftTmp=AttenuatorLeft & AttenuatorLeftOld;            //define intermediate volume levels
    AttenuatorRightTmp=AttenuatorRight & AttenuatorRightOld;         //define intermediate volume levels
    if (debugEnabled) {
      Serial.print (F("DefineVolume: volume is now : "));
      Serial.print (AttenuatorMain);
      Serial.print (F(" volume left = "));
      Serial.print (AttenuatorLeft);
      Serial.print (F(". Bytes, old, temp, new "));
      Serial.print ( AttenuatorLeftOld,BIN);
      Serial.print (" , ");
      Serial.print (AttenuatorLeftTmp,BIN);
      Serial.print (" , ");
      Serial.print ( AttenuatorLeft,BIN);
      Serial.print (F(" ,volume right = "));
      Serial.print (AttenuatorRight);
      Serial.print (F(".  Bytes, old, temp, new "));
      Serial.print ( AttenuatorRightOld,BIN);
      Serial.print (" , ");
      Serial.print ( AttenuatorRightTmp,BIN);
      Serial.print (" , ");
      Serial.print (AttenuatorRight,BIN);
      Serial.print(F(",  headphones : "));
      Serial.println (InitData.HeadphoneActive);
    }
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Function to change mute 
void changeMute() 
{
  if (muteEnabled) {                                                      // if mute enable we turn mute off
    muteEnabled = false;                                                  // change value of boolean
    setRelayChannel(InitData.SelectedInput);                              // set relays to support origical inputchnannel
    delay(100);                                                                   
    defineVolume(0);                                                        // define new volume level
    setRelayVolume(AttenuatorLeft,AttenuatorRight);                                                                    
    writeOledScreenLeft();                                                   //display info on oled screen
    writeOledVolume(AttenuatorMain);    
    if (debugEnabled) {
       Serial.print(F("ChangeMute : Mute now disabled, setting volume to: "));
       Serial.println (AttenuatorMain);
    }
  } 
  else {
    muteEnabled = true;                                                   // change value of boolean 
    setRelayVolume(0,0);                                                  // switch volume off
    delay(15);                                                             // set relays to zero volume
    setRelayChannel(0);                                                   // set relays to no input channel
    writeOledScreenLeft();                                                //display info on oled screen
    if (debugEnabled)     {
      Serial.println (F("Changemute: Mute enabled"));
    }
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// write left side screen, procedure to write left side of screen, (mute, headphones, passive)
void writeOledScreenLeft()
{
  u8x8.setFont(font1x2);                                            // set font 
  if (InitData.HeadphoneActive) { 
   u8x8.drawString(0, 4, "HEADPHONE"); 
  } 
  else if (InitData.AmpPassive) {
  u8x8.drawString(0, 4, "PASSIVE"); 
  }
  else {
  u8x8.drawString(0, 4, "         ");
  }
  if (muteEnabled) {
    u8x8.drawString(0, 2, "MUTE"); 
  }
  else {
    u8x8.drawString(0, 2, "    "); 
  }
  u8x8.setCursor(0, 6);
  u8x8.print(inputTekst[InitData.SelectedInput]);  
}         
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// intialistion of the oled screen after powerdown of screen.
void OledSchermInit()                      
{
  digitalWrite(oledReset, LOW);                   // set oled screen in reset mode
  delay(15);                                      // wait to stabilize
  digitalWrite(oledReset, HIGH);                  // set screen active
  delay(15);                                      // wait to stabilize
  u8x8.setI2CAddress(OLED_Address * 2);           // set oled address before we begin
  u8x8.begin();                                   // open com to screen
  u8x8.setPowerSave(1);                           // set screen off
  u8x8.setFlipMode(1);                            // flip screen
  u8x8.setContrast(ContrastLevel);                // set contrast level
  u8x8.setPowerSave(0);                           // turn screen on
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 // write volume level to screen
void writeOledVolume(int volume)                 
{
  int offset = -63;
  char valueinchar[8];                                        // used for converting int to string of char
  if (!InitData.AmpPassive) {                                 // if the amp is not in passive mode adjust volume displayed on screen
    offset = offset + Voloffset;
  }
  volume = volume + offset;                                   // change volumelevel shown on screen, adding offset
  u8x8.setFont(u8x8_font_profont29_2x3_n); 
  u8x8.setCursor(9,1);
  u8x8.print("   ");
  u8x8.setCursor(8,1);
  if (volume < 0) {u8x8.print(volume);}
  if (volume > 0 ) {u8x8.print("+");u8x8.print(volume);}
  if (volume == 0 ) {u8x8.print(" ");u8x8.print(volume);}
  u8x8.setFont(font1x1);
  u8x8.setCursor(14,2);
  u8x8.print("dB");
    
  if (AttenuatorRight!= AttenuatorLeft) {
    int AttenuatorLeftDisplay = AttenuatorLeft + offset;
    int AttenuatorRightDisplay = AttenuatorRight + offset;
    u8x8.setFont(font1x1);
    u8x8.setCursor(0,0);
    sprintf(valueinchar, "L%i R%i", AttenuatorLeftDisplay,AttenuatorRightDisplay ); 
    u8x8.print(valueinchar); 
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// set relays in status to support requested volume
void setRelayVolume(int RelayLeft, int RelayRight) 
  {
  Wire.beginTransmission(MCP23017_I2C_ADDRESS_bottom);            // write volume left to left ladder
  Wire.write(0x13);
  Wire.write(RelayLeft);
  Wire.endTransmission();
  if (daughterboard) {
    Wire.beginTransmission(MCP23017_I2C_ADDRESS_top);             // write volume right to right ladder
    Wire.write(0x13);
    Wire.write(RelayRight);
    Wire.endTransmission(); 
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// set relays in status to support requested channnel
void setRelayChannel(uint8_t relay)             
{
  uint8_t inverseWord;                          
  if (relay==0){                                 // if no channel is selected, drop all ports, this is done during  mute
    inverseWord=0xef;                            // we write inverse, bit 4 = 0 as this one is not inverted
  }
  else{
    inverseWord= 0xFF ^ (0x01 << (relay-1));    // bitshift the number of ports to get a 1 at the correct port and inverse word
   if (relay<3){                               // if input 1 or 2 selected, xlr, so zero_neg relay should be off.
     bitClear(inverseWord,4);                  // set bit 4 to 0
   }
  } 
  Wire.beginTransmission(MCP23017_I2C_ADDRESS_bottom);
  Wire.write(0x12);
  Wire.write(inverseWord);
  Wire.endTransmission();
  if (daughterboard) {
    Wire.beginTransmission(MCP23017_I2C_ADDRESS_top);          // write volume right to right ladder
    Wire.write(0x12);
    Wire.write(inverseWord);
    Wire.endTransmission(); 
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//initialize the MCP23017 controling the relays;
void MCP23017init(uint8_t MCP23017_I2C_ADDRESS)
{
  Wire.beginTransmission(MCP23017_I2C_ADDRESS);
  Wire.write(0x00);                            // IODIRA register
  Wire.write(0x00);                            // set all of port B to output
  Wire.endTransmission();
  Wire.beginTransmission(MCP23017_I2C_ADDRESS);
  Wire.write(0x12);                            // gpioA
  Wire.write(0xEF);                            // set all ports high except bit 4
  Wire.endTransmission();
  Wire.beginTransmission(MCP23017_I2C_ADDRESS);
  Wire.write(0x01);                            // IODIRB register
  Wire.write(0x00);                            // set all of port A to outputs
  Wire.endTransmission();
  Wire.beginTransmission(MCP23017_I2C_ADDRESS);
  Wire.write(0x13);                            // gpioB
  Wire.write(0x00);                            // set all ports low, 
  Wire.endTransmission();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// detect log time press on remote controle
bool detectLongPress(uint16_t aLongPressDurationMillis) {
  if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) {  // if repeat and not detected yet
  //if (!sLongPressJustDetected && (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT)) {  // if repeat and not detected yet
      if (millis() - aLongPressDurationMillis > sMillisOfFirstReceive) {                       // if this status takes longer as..
          sLongPressJustDetected = true;                                                       // longpress detected
      }
   
  } else {                                                                                     // no longpress detected
      sMillisOfFirstReceive = millis();                                                        // reset value of first press
      sLongPressJustDetected = false;                                                          // set boolean
  }
  return sLongPressJustDetected; 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// detect if the EEPROM contains an init config
void checkIfEepromHasInit() {
  char uniquestring[7]="PreAmp";                                            // unique string to check if eeprom already written
  EEPROM.get(0, InitData);                                                  // get variables within init out of EEPROM
  for (byte index=1; index<7; index++) {                                    // loop to compare strings
    if (InitData.uniquestring[index]!=uniquestring[index]) {                // compare if strings differ, if so
      writeEEprom();                                                        // write eeprom
      break;                                                                // jump out of for loop
    }
    if (debugEnabled) {                      
      if (index==6) {                                                       // index 6 is only possible if both strings are equal
        Serial.println (F("checkIfEepromHasInit: init configuration found"));   
      }
    }
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// write the EEProm with the correct values
void writeEEprom() {                                                  
  if (debugEnabled) {
    Serial.println (F("writeEEprom: no init config detected, writing new init config"));   
  }
  SavedData start = {
    "PreAmp",                                               // unique string
    false,                                                  // boolean, volume level per channel
    1,                                                      // channel used for start
    0,                                                      // balance offset 
    1,                                                      // balance multiplier
    false,                                                  // headphones active 
    false                                                   // ampassive active
    };
  int VolLevelsInit [5] = {                         // array of volumelevels if volumelevel per channel is active
    //Attenuator generic
    30,
    //AttenuatorCh1
    30,
    //AttenuatorCh2
    30,
    //AttenuatorCh3
    30,
    //AttenuatorCh4
    30
    };
  EEPROM.put(0, start);                                    // write init data to eeprom
  EEPROM.put(30,VolLevelsInit);                            // write volslevel to eeprom
}

