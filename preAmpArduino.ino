///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// v0.2, integration of oled screen
// v0.3 optimize and change, adapt volume part for screen and rotator
// v0.4 included functions to support relay, include 2 buttons (mute and channel) and restructured setup due to lack of readabilitie
// v0.5 included IR support, fixed issue with LSB B bank controling volume. 
// v0.6 included eeprom support, 3 to 4 channels, headphone support, passiveAmp support,
// v0.7 added IR codes, changed channelinput to + and -, added long delay to led preamp stabilize, added menu, changed array and struct in eeprom


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// definitions for EPROM writing
#include <EEPROM.h>
struct SavedData {                                    // definition of the data stored in eeprom
  bool VolPerChannel;                                 // boolean, if True volume level is fixed with channel change. Otherwise Volume is Attenuator
  uint8_t SelectedInput;                              // last selected input
  int BalanceOffset;                                  // balance offset of volume 
  bool HeadphoneActive;                               // headphones active, no output to amp
  bool AmpPassive;                                    // defines if preamp is used passive mode
  };
SavedData InitData;                                   // initdata is a structure defined by SavedData containing the values
int VolLevels [5];                                    // Vollevels is an array storing initial volume level when switching to channel, used if volperchannel is true
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// general definitions 
#include <Wire.h>                                     // include functions for i2c
#define PowerOn     A0                                // pin connected to the relay handeling power on/off of the amp
#define Headphone   A1                                // pin connected to the relay handeling headphones active / not active
#define AmpPassiveState  A2                           // pin connected to the relay handeling passive/active state of amp
#define ledStandby  11                                // connected to a led that is on if device is in standby
const char* initTekst = "V 0.7";                      // current version of the code, shown in startscreen, content should be changed in this line
bool debugEnabled = true;                             // define if debug is enabled, value should be changed in this line, either true or false
bool Alive = true;                                    // alive defines if we are in standby mode or acitve
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// definitons for the IR receiver
#define DECODE_NEC                                    // only add NEC prot to support apple remote, saves memory
#include <IRremote.hpp>                               // include drivers for IR 
#define IR_RECEIVE_PIN 2                              // datapin of IR is connected to pin 2
#define apple_left 8                                  // below the IR codes received for apple and ???
#define eindhoven_left 33
#define apple_right 7                               
#define eindhoven_right 32
#define apple_up 11                                 
#define eindhoven_up 136
#define apple_down 13                             
#define eindhoven_down 140
#define apple_middle 93                               
#define apple_menu 2                                  
#define apple_forward 94                              
uint8_t lastcommand;                                  // previous command coming from IR, used within IR procedure
uint8_t delaytimer;                                   // timer to delay between volume changes, actual value set in main loop
unsigned long timeOfLastIRChange;                     // used within IR procedures to determine if command is repeat
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// definitions for the oled screen
#define OLED_Address 0x3C                             // 3C is address used by oled controler
#define OLED_Command_Mode 0x80                        // Control byte indicates next byte will be a command
#define OLED_Data_Mode 0x40                           // Control byte indicates next byte will be data
const uint8_t ContrastLevel=0x3f;                     // level of contrast, between 00 anf ff, default is 7f
const uint8_t Voloffset=0;                            // diffence between volumelevel and volumelevel displayed on the screen
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// definitions for the attenuator board
#define MCP23017_I2C_ADDRESS_bottom 0x24             // I2C address of the relay board left
#define MCP23017_I2C_ADDRESS_top 0x11                // I2C address of the relay board right
int AttenuatorLevel;                                 // geluidssterkte, wordt bij opstarten gezet aan de hand van waarde in EEPROM
volatile int AttenuatorChange = 0;                   // change of volume out of interupt function
bool muteEnabled = false;                            // status of Mute
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// definitions for the rotary
#define  rotaryButton   5                            //pin 5 is the switch of the rotary
volatile int rotaryPinA = 3;                         //encoder pin A, volatile as its addressed within the interupt of the rotary
volatile int rotaryPinB = 4;                         //encoder pin B, volatile as its addressed within the interupt of the rotary
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// definitions for the buttons                      (definitions for rotary button are with rotary)
#define buttonChannel  6                             // pin 6 is change input channel round robin, pullup
#define buttonStandby  7                             // pin 7 is standby on/off, pullup
#define buttonHeadphone 8                            // pin 8 is headphone on and off.
#define buttonPassive 9                              // pin 9 is bypass preamp part
#define buttonMute  10                               // pin 10 mute on/off.
const char* inputTekst[5] = {                        // definitions used to display, content could be changed
  // generic volume
  "Generic volume",
  //channel 1
  "XLR  1",
  //channel 2
  "XLR  2",
  //channel 3
  "RCA  1",
  // channel 4
  "RCA  2"
  };
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// definitions for the compiler as we have nested functions
void changeVolume(int increment);                              // change volume
void rotaryTurn();                                             // interupt handler if rotary is turned
void setVolumeOled(int volume);                                // write volume level to screen
void writeOLEDtopright(const char *OLED_string);               // write tekst to screen top right
void writeOLEDbottemright(const char *OLED_string);            // write tekst to screen bottem right
void sendCommandOled(unsigned char command);                   // send a command to the Oled screeen
void sendDataOled(unsigned char data);                         // send data to the Oled screen
void writeOLEDmenu(const char *OLED_string, uint8_t pos );     // write to oled screen, used for setup / warmup screen
void OledSchermInit();                                         // init procedure for Oledscherm
void MCP23017init();                                           // init procedure for relay extender
void setRelayChannel(uint8_t Word);                            // set relays to selected input channel
void setRelayVolume(int Word);                                 // set relays to selected volume level
void changeStandby();                                          // moves from active to standby and back to active
void changeHeadphone();                                        // change status of headphones
void changePassive();                                          // change status of preamp between active and passive
void changeMute();                                             // enable disable mute
void changeInput(int change);                                  // change the input channel
void setupMenu();                                              // run the setup menu.
void WaitForXseconds();                                        // wait for number of seconds, used to warm up amp 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup()
{
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 
  // general
  pinMode (PowerOn, OUTPUT);                                                //control the relay that provides power to the rest of the amp
  pinMode (ledStandby, OUTPUT);                                             //led will be on when device is in standby mode
  pinMode (Headphone, OUTPUT);                                              //control the relay that switches headphones on and off
  pinMode (AmpPassiveState, OUTPUT);                                        //control the relay that switches the preamp between passive and active
  digitalWrite(PowerOn, HIGH);                                              // make pin of standby high, relays&screen on and power of amp is turned on
  digitalWrite(ledStandby, LOW);                                            // turn off standby led to indicate device is in powered on
  digitalWrite(Headphone, LOW);                                             // make pin controling headphones low
  digitalWrite(AmpPassiveState, LOW);                                       // turn the preamp in active state
  delay(1000);                                                              // wait till all powered up
  if (debugEnabled) {                                                       // if debuglevel on start monitor screen
    Serial.begin (9600);
  }
  Wire.begin();                                                             // open i2c channel
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // initialize the oled screen
  OledSchermInit();                                                         // intialize the Oled screen
  WaitForXseconds();                                                        // wait to let amp warm up
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // initialize the relay extender
  MCP23017init();                                                           // initialize the relays                                                              
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Set up pins for rotary:
  pinMode (rotaryPinA,INPUT_PULLUP);                                        // pin A rotary is high and its an input port
  pinMode (rotaryPinB,INPUT_PULLUP);                                        // pin A rotary is high and its an input port
  pinMode (rotaryButton, INPUT_PULLUP);                                     // input port and enable pull-up
  attachInterrupt(digitalPinToInterrupt(rotaryPinA), rotaryTurn, RISING);   // if pin encoderA changes run rotaryTurn
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // IR
  IrReceiver.begin(IR_RECEIVE_PIN);                                          // Start the IR receiver
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Button setup
  pinMode (buttonMute, INPUT_PULLUP);                                        //button to mute 
  pinMode (buttonChannel, INPUT_PULLUP);                                     //button to change channel
  pinMode (buttonStandby, INPUT_PULLUP);                                     //button to go into standby/active
  pinMode (buttonHeadphone, INPUT_PULLUP);                                   //button to switch between AMP and headphone
  pinMode (buttonPassive, INPUT_PULLUP);                                     //button to switch between passive and active mode
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 
  // // Set inital setup amp depending on data stored within eeprom
  EEPROM.get(0, InitData);                                                   // get variables within init out of EEPROM
  EEPROM.get(10,VolLevels);                                                  // get the aray setting volume levels
  if (InitData.AmpPassive) {                                                 // if amppassive is active set port high to active relay
    digitalWrite(AmpPassiveState, HIGH);                                     // turn the preamp in passive mode
  }
  setRelayVolume(0);                                                         //set volume relays to 0, just to be sure
  delay(15);                                                                 // wait to stabilize
  setRelayChannel(InitData.SelectedInput);                                   // select stored input channel
  delay(15);                  
  if (InitData.HeadphoneActive) {                                            // headphone is active
    digitalWrite(Headphone, HIGH);                                           // switch headphone relay on
  } 
  if (InitData.VolPerChannel) {                                              // if we have a dedicated volume level per channel give attenuatorlevel the correct value
    AttenuatorLevel=VolLevels[InitData.SelectedInput];                       // if vol per channel select correct level
  }
  else {                                                                     // if we dont have the a dedicated volume level per channel
    AttenuatorLevel=VolLevels[0];                                            // give attenuator level the generic value
  } 
  setRelayVolume(AttenuatorLevel);                                           // set volume correct level
  setVolumeOled(AttenuatorLevel);                                            //display volume level on oled screen
  writeOLEDtopright(inputTekst[InitData.SelectedInput]);                     //display input channel on oled screen
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Main loop
//////////////////////////////////////////////////////////////////////////////////////////////
void loop()
{
  if (Alive) {                                              // we only react if we are in alive state, not in standby
    if (AttenuatorChange != 0) {                            // if AttenuatorChange is changed by the interupt we change  Volume
      changeVolume(AttenuatorChange);                       // Attenuatorchange is either pos or neg depending on rotation
      AttenuatorChange = 0;                                 // reset the value to 0
    }
    if (digitalRead(buttonMute) == LOW) {                   // if button mute is pushed
      delay(500);                                           // wait to prevent multiple switches
      changeMute();                                         // change status of mute
    }
    if (digitalRead(buttonChannel) == LOW) {                // if button channel switch is pushed
      delay(500);                                           // wait to prevent multiple switches
      changeInput(1);                                       // change input channel
    }
    if (digitalRead(buttonHeadphone) == LOW) {              // if button headphones switch is pushed
      delay(500);                                           // wait to prevent multiple switches
      changeHeadphone();                                    // change input 
    }
    if (digitalRead(buttonPassive) == LOW) {                // if button passive switch is pushed
      delay(500);                                           // wait to prevent multiple switches
      changePassive();
    }  
    if (digitalRead(rotaryButton) == LOW) {                 // if rotary button is pushed go to setup menu
      delay(500);                                           // wait to prevent multiple switches
      setupMenu(); 
    }   
  }
  if (digitalRead(buttonStandby) == LOW) {                   // if button standby is is pushed
      delay(500);                                            // wait to prevent multiple switches
      changeStandby();                                       // changes status
  
  }
  if (IrReceiver.decode()) {                                 // if we receive data on the IR interface
    if ((millis() - timeOfLastIRChange) < 300) {             // simple function to increase speed of volume change by reducing wait time
      delaytimer=50;
    } else {
      delaytimer=200;
    }
 //   Serial.println(IrReceiver.decodedIRData.command);
    switch (IrReceiver.decodedIRData.command){               // read the command field containing the code sent by the remote
      case apple_up:
      case eindhoven_up:
        if (Alive) {                                         // we only react if we are in alive state, not in standby
          changeVolume(1);
          delay(delaytimer);
        }
        break;
      case apple_down:
      case eindhoven_down:
        if (Alive) {                                         // we only react if we are in alive state, not in standby
          changeVolume(-1);
          delay(delaytimer);
        }
        break;
      case apple_left:
      case eindhoven_left:
        if (Alive) {                                         // we only react if we are in alive state, not in standby
           changeInput(-1);
           delay(200);
        }
        break;
      case apple_right:
      case eindhoven_right: 
        if (Alive) {                                         // we only react if we are in alive state, not in standby
           changeInput(1);
           delay(200);
        }
        break;       
      case apple_forward:
        changeStandby();
        break;
      case apple_middle:
        if (Alive) {
          changeMute(); 
          delay(200); 
        }
        break;
    }
 //   lastcommend=IrReceiver.decodedIRData.command;
    timeOfLastIRChange = millis();                            // store time to detect if button is still pressed or not 
    IrReceiver.resume();                                      // open IR receiver for next command
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// delay the startup to wait for pre amp to stabilize, clears screen at start and end of proc FORMATTING TO BE DONE
void WaitForXseconds() {
  char valueinchar[3];
  sendCommandOled(0x01);                                          // Clear Display and set DDRAM location 00h
  writeOLEDtopright(" preamp is warming up ");                    // write balance TO BE CHANGED
  writeOLEDtopright(initTekst);                                   // write version number
  for (int i=25; i>0;i--) {                                       // run for 25 times, waiting in total 25 seconds
    delay(1000);                                                  // delay for a second
    sprintf(valueinchar, "%d", i);                                // convert int to char array
    writeOLEDtopright(valueinchar);                               // write number off seconds left
  }  
  sendCommandOled(0x01);                                          // Clear Display and set DDRAM location 00h
}                                                                 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// procedure to handle change of headphones status                   nog maken is iets schrijven op het oled scherm
void changeHeadphone()
{
  if (InitData.HeadphoneActive) {                                            // headphone is active so we have to change to inactive
    if (debugEnabled) {
      Serial.print(F(" changeHeadphone : moving from active to inactive,  "));
    }
    setRelayVolume(0);                                                       // switch volume off
    delay(15);                                                              // wait to stabilize
    digitalWrite(Headphone, LOW);                                            // switch headphone relay off
    if (InitData.VolPerChannel) {                                            // if we have a dedicated volume level per channel give attenuatorlevel the correct value
      AttenuatorLevel=VolLevels[InitData.SelectedInput];                     // if vol per channel select correct level
    }
    setRelayVolume(AttenuatorLevel);                                         // set volume correct level
    setVolumeOled(AttenuatorLevel);                                          // update volume on screen
    InitData.HeadphoneActive=false;                                          // status off headphone active changed to false
    EEPROM.put(0,InitData);                                                  // write new status to EEPROM
    if (debugEnabled) {
      Serial.println (" status is now inactive ");
    }
  } else {                                                                   // headphone is not active so we have to make it active
    if (debugEnabled) {
      Serial.print(F(" changeHeadphone : moving from inactive to active,  "));
    }
    setRelayVolume(0); 
    delay(15);                                                               // switch volume off
    if(InitData.AmpPassive) {                                                // amp must be active if we use headphone
      InitData.AmpPassive=false;                                             // make ampassive false
      digitalWrite(AmpPassiveState, LOW);                                    // set relay low.
    }
    delay(15);                                                               // wait to stabilize
    digitalWrite(Headphone, HIGH);                                           // switch headphone relay on
    delay(15);                                                               // wait to stabilize
    if (InitData.VolPerChannel) {                                            // if we have a dedicated volume level per channel give attenuatorlevel the correct value
      AttenuatorLevel=VolLevels[InitData.SelectedInput];                     // we have a dedicated volumelevel for the headphones
    }                                                                        // else we keep current volume level
    setRelayVolume(AttenuatorLevel);                                         // set volume correct level
    setVolumeOled(AttenuatorLevel);                                          // update volume on screen
    InitData.HeadphoneActive=true;                                           // status off headphone active changed to false
    EEPROM.put(0,InitData);                                                  // write new status to EEPROM
    if (debugEnabled) {
      Serial.println (F(" status is now active "));
    }
  }
}                                       
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// procedure to switch between active and passive state of preamp              nog doen: scherm aansutren
void changePassive()                                        
{
  if (!InitData.HeadphoneActive) {
    if (InitData.AmpPassive) {                                                 // Amp is in passive state so we have to change to active
      if (debugEnabled) {
        Serial.print (F(" changePassive : moving from passive to active,  "));
      }
      setRelayVolume(0);                                                       // switch volume off
      delay(15);                                                               // wait to stabilize
      digitalWrite(AmpPassiveState, LOW);                                      // switch headphone relay off
      delay(300);                                                              // wait to stabilize
      setRelayVolume(AttenuatorLevel);                                         // turn volume on again
      InitData.AmpPassive=false;                                               // make the boolean the correct value
      EEPROM.put(0,InitData);                                                  // write new status to EEPROM
      if (debugEnabled) {
        Serial.println (F(" status is now active "));
      }
    } else {                                                                   // Amp is in active mode so we have to change it to passive
      if (debugEnabled) {
        Serial.print (F(" changePassive : moving from active to passive,  "));
      }
      setRelayVolume(0);                                                       // switch volume off
      delay(15);                                                               // wait to stabilize
      digitalWrite(AmpPassiveState, HIGH);                                     // switch headphone relay on
      delay(300);                                                              // wait to stabilize
      setRelayVolume(AttenuatorLevel);                                         // turn volume on again
      InitData.AmpPassive=true;                                                // make the boolean the correct value
      EEPROM.put(0,InitData);                                                  // write new status to EEPROM   
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
  char valueinchar[6];                                         // used for converting int to char
  int Vol;  
//  set balance value
  sendCommandOled(0x01);                                          // Clear Display and set DDRAM location 00h
  writeOLEDtopright(" balance ");                                 // write balance
  writeOLEDmenu("x",200+InitData.BalanceOffset);                  // write current balance using position
  while (digitalRead(rotaryButton)==HIGH) {                       // as long as rotary button not pressed
    if (AttenuatorChange != 0) {                                  // if AttenuatorChange is changed using rotary 
      InitData.BalanceOffset=InitData.BalanceOffset + AttenuatorChange;  // Attenuatorchange is either pos or neg depending on rotation
      AttenuatorChange = 0;                                       // reset value
      if (InitData.BalanceOffset > 6) {                           // keep value between -6 and 6
        InitData.BalanceOffset  = 6;
      }  
      if (InitData.BalanceOffset  < -6) {
        InitData.BalanceOffset = -6;
      }
      sendCommandOled(0x01);                                       // Clear Display and set DDRAM location 00h
      writeOLEDtopright(" balance ");                              // write balance
      writeOLEDmenu("x",200+InitData.BalanceOffset);               // write the x on the correct position depending on offset
      setRelayVolume(0);                                           // volume to 0
      delay(15);                                                   // delay 15
      setRelayVolume(AttenuatorLevel);                             // set new volume level with balance level
      delay(500);                                                  // delay to prevent looping
    }
  }
  delay(1000);
//  set vol per channel yes or no
  sendCommandOled(0x01);                                       // Clear Display and set DDRAM location 00h
  writeOLEDtopright("vol per channel");
  if (InitData.VolPerChannel) {sprintf(valueinchar, "%s", "Yes");}
  else {sprintf(valueinchar, "%s", "No ");}
  writeOLEDmenu(valueinchar,200); 
  while (digitalRead(rotaryButton)==HIGH) {
    if (AttenuatorChange != 0) {  
      InitData.VolPerChannel=(!InitData.VolPerChannel);
      AttenuatorChange = 0; 
      if (InitData.VolPerChannel) {sprintf(valueinchar, "%s", "Yes");}
      else {sprintf(valueinchar, "%s", "No ");}
      writeOLEDmenu(valueinchar,200);
      delay(500);
    }
  }
  delay(1000);
// set vol level
  sendCommandOled(0x01);                                       // Clear Display and set DDRAM location 00h
  for (int i=0; i<5; i++){                                     // run loop for generic and 4 volumes per channel
    if (i!=0) {                                                // use for the generic the channel thats used now
      setRelayChannel(i);                                      // otherwise select the correct volume
    }      
    writeOLEDtopright(inputTekst[i]);                          // write the name of the variable we are changing
    Vol=VolLevels[i]-63+Voloffset;                             // vol is de value displayed on the screen
    sprintf(valueinchar, "%d", Vol);                           // create an array of chars showing volume level
    writeOLEDmenu(valueinchar,200);                            // write current volumelevel init
    while (digitalRead(rotaryButton)==HIGH) {                  // as long as rotary button not pressed
      if (AttenuatorChange != 0) {                             // if AttenuatorChange is changed using rotary
        VolLevels[i]=VolLevels[i]+AttenuatorChange;            // change Attenuator init
        AttenuatorChange = 0;                                  // reset attenuatorchange
        if (VolLevels[i] > 63) {                               // code to keep attenuator between 0 and 63
          VolLevels[i] = 63;
        }  
        if (VolLevels[i] < 0) {
          VolLevels[i]= 0;
        }  
        Vol=VolLevels[i]-63+Voloffset;                         // vol is de value displayed on the screen
        sprintf(valueinchar, "%d", Vol);                       // write current volume to a array with chars
        writeOLEDmenu(valueinchar,200);                        // write volume init to screen
        setRelayVolume(0);                                     // set volumelevel to new value, first go to 0
        delay(15);
        setRelayVolume(VolLevels[i]);                          // set volumelevel to new value
      }                                                        // delay to prevent moving to fast
    }
    delay(500);
    if (!InitData.VolPerChannel) {                             // if we do not use volume per channel skip for loop
      delay(500);
      break;
    }
  }
  EEPROM.put(0,InitData);                                      // save new values
  EEPROM.put(10,VolLevels);                                    // save volume levels
  sendCommandOled(0x01);                                       // Clear Display and set DDRAM location 00h
  setRelayChannel(InitData.SelectedInput);                     // set amp on prefered channel
  if (InitData.VolPerChannel) {                                // choose volume level 
    AttenuatorLevel=VolLevels[InitData.SelectedInput];
  } else {
    AttenuatorLevel=VolLevels[0];
  }
  setRelayVolume(0);                                           // set volume level to 0
  setRelayVolume(AttenuatorLevel);                             // set volume correct level
  setVolumeOled(AttenuatorLevel);                              //display volume level on oled screen
  writeOLEDtopright(inputTekst[InitData.SelectedInput]);       //display input channel on oled screen
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// procedure to handle moving to and coming out of standby mode
void changeStandby()
{
  if (Alive) {                                                     // move from alive to standby, turn off screen, relays and amp
    if (debugEnabled) {
      Serial.print (F(" changeStandby : moving from active to standby,  "));
    }
    digitalWrite(PowerOn, LOW);                                    // make pin of standby low, relay&screen off and power of amp is turned off
    digitalWrite(ledStandby, HIGH);                                // turn on standby led to indicate device is in standby
    delay(3000);                                                   // we first wait for stable state
    sendCommandOled(0x08);                                         // turn display OFF, cursor OFF, blink OFF
    setRelayVolume(0);                                             // set volume relays off
    setRelayChannel(0);                                            // disconnect all input channels
    if (debugEnabled) {
      Serial.println (F(" status is now standby "));
    }
    Alive=false;                                                   // we are in standby, alive is false
  } 
  else {
    if (debugEnabled) {
      Serial.print (F(" changeStandby : moving from standby to active,  "));
    }
    digitalWrite(PowerOn, HIGH);                                   // make pin of standby low, relays&screen on and power of amp is turned on
    digitalWrite(ledStandby, LOW);                                 // turn off standby led to indicate device is in powered on
    delay(1000);                                                   // we first wait for stable state
    sendCommandOled(0x01);                                         // Clear Display and set DDRAM location 00h
    sendCommandOled(0x0C);                                         // turn display ON, cursor OFF, blink OFF
    void WaitForXseconds();                                        // wait for 25 seconds
    setRelayVolume(0);                                             // set the relays in 0 
    delay(15);                                                     // delay for 15ms
    setRelayChannel(InitData.SelectedInput);                       // select correct input channel
    delay(15);                                                     // delay for 15 ms
    setRelayVolume(AttenuatorLevel);                               // set the relays in the start volume
    setVolumeOled(AttenuatorLevel);                                // display volume level on oled screen
    writeOLEDtopright(inputTekst[InitData.SelectedInput]);         // display input channel on oled screen  
    if (debugEnabled) {
      Serial.println (F(" status is now active "));
    }
    Alive=true; 
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
  InitData.SelectedInput=InitData.SelectedInput+change;                     // increase or decrease current channel by 1
  if (InitData.SelectedInput > 4) { InitData.SelectedInput = 1; }           // implement round robbin for channel number
  if (InitData.SelectedInput < 1) { InitData.SelectedInput = 4; }
  setRelayVolume(0);                                                        // set volume to zero
  delay(15);                                                                // wait 15ms to stabilize
  setRelayChannel(InitData.SelectedInput);                                  // set relays to new input channel
  delay(15); 
  if (InitData.VolPerChannel) {                                            // if we have a dedicated volume level per channel give attenuatorlevel 
    AttenuatorLevel=VolLevels[InitData.SelectedInput];                      // if vol per channel select correct level
  }
  if (!muteEnabled){                                                        // if mute not enabled write volume to relay
    setRelayVolume(AttenuatorLevel);                                        
  }                                         
  writeOLEDtopright(inputTekst[InitData.SelectedInput]);                     // update oled screen to new channel
  EEPROM.put(0,InitData);                                                    // save new channel number in eeprom
  if (debugEnabled) {
    Serial.print(F(", new Selected Input: "));                               // write new channel number to debug
    Serial.println (InitData.SelectedInput);
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Function to change a specific attenuator level
//////////////////////////////////////////////////////////////////////////////////////////////
// Functions to change volume 
void changeVolume(int increment) 
{
  if (not muteEnabled)
  {
    if (debugEnabled) {
      Serial.print (F("ChangeVolume: volume was : "));   
      Serial.println (AttenuatorLevel); 
    }
    AttenuatorLevel = AttenuatorLevel + increment;                            // define new attenuator level
    if (AttenuatorLevel > 63) {                                               // volume cant be higher as 63
      AttenuatorLevel=63; 
    }
    if (AttenuatorLevel < 0 ) {                                               // volume cant be lower as 0
      AttenuatorLevel=0;
    } 
    setRelayVolume(0);                                                        // set volume first to zero, looks like eindhoven is doing this
    delay(15);                                                                // wait 15mS, needed according to eindhoven
    setRelayVolume(AttenuatorLevel);                                          // set relays according new volume level
    setVolumeOled(AttenuatorLevel);                                           // update screen 
    if (debugEnabled) {
      Serial.print (F("ChangeVolume: volume is now : "));
      Serial.println (AttenuatorLevel);
    }
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Function to change mute 
void changeMute() 
{
  if (muteEnabled) {
    if (debugEnabled) {
       Serial.print(F("ChangeMute : Mute now disabled, setting volume to: "));
       Serial.println (AttenuatorLevel);
    }
    setRelayChannel(InitData.SelectedInput);                                       // set relays to support origical inputchnannel
    delay(15);                                                                   
    setRelayVolume(AttenuatorLevel);                                              //  set relays to support orgical volume
    writeOLEDbottemright("      ");                                               //  update oled screen
    muteEnabled = false;
  } 
  else {
    setRelayVolume(0);
    delay(15);                                                                      // set relays to zero volume
    setRelayChannel(0);                                                             // set relays to no input channel
    writeOLEDbottemright(" Muted");                                                 // update oled screen
    muteEnabled = true;
    if (debugEnabled)     {
      Serial.println (F("Changemute: Mute enabled"));
    }
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// send data to the oled display
void sendDataOled(unsigned char data)             
{
  Wire.beginTransmission(OLED_Address);
  Wire.write(OLED_Data_Mode);
  Wire.write(data);
  Wire.endTransmission();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// send command to oled display
void sendCommandOled(unsigned char command)       // send a command to the display
{
  Wire.beginTransmission(OLED_Address);
  Wire.write(OLED_Command_Mode);
  Wire.write(command);
  Wire.endTransmission();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// function to store bitmaps for large volume into CGRAM
void StoreLargecijfer()                          
{
  const uint8_t Bitmaps[8][8] = {                       // array of bitmaps for large figures, copied and adapted from jos van eindhoven
  {//bitmap 0
  0x07, 0x0f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f},
  {//bitmap 1
  0x1f, 0x1f, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00},
  {//bitmap 2
  0x1c, 0x1e, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f},
  {//bitmap 3
  0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x0f, 0x07},
  {//bitmap 4
  0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x1f, 0x1f},
  {//bitmap 5}
  0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1e, 0x1c},
  {//bitmap 6
  0x1f, 0x1f, 0x1f, 0x00, 0x00, 0x00, 0x1f, 0x1f},
  {//bitmap 7
  0x1f, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x1f, 0x1f}
};
  sendCommandOled(0x40);                         // set CGRAM first address
  for (int i=0; i<8; i++){                       // walk through the array bitmaps, we have 8 bitmaps
      for (int c=0; c<8; c++){                   // every bitmap consists out of 8 lines
        sendDataOled(Bitmaps[i][c]);             // sent data to cgram
      }
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// intialistion of the oled screen after powerdown of screen.
void OledSchermInit() 
                        
{
    delay(200);
  // sendCommandOled(0x2E);                         // Function Set, set RE=1, enable double height
  // sendCommandOled(0x08);                         // Extended Function Set, 5-dot, disable cursor-invert, 2-line mode
  // sendCommandOled(0x72);                         // Function Selection B, first command, followed by data command
  // sendDataOled(0x00);                            // Set ROM A with 8 custom characters
  // sendCommandOled(0x79);                         // set SD=1, enable OLED Command Set
  // sendCommandOled(ContrastLevel);                // Set Contrast Control [brightness]
  // sendCommandOled(0x7F);                         // default is 0x7F, 00 is min,  0xFF is max
  // sendCommandOled(0x78);                         // set SD=0, disable OLED Command Set
  // sendCommandOled(0x28);                         // Function Set, set RE=0
  // StoreLargecijfer();                            // write large cijfers to dram
  // sendCommandOled(0x01);                         // Clear Display and set DDRAM location 00h
  // sendCommandOled(0x0C);                         // turn display ON, cursor OFF, blink OFF
    sendCommandOled(0x3A);             //FunctionSet: N=1 BE=0 RE=1 IS=0
    sendCommandOled(0x09);             //4‐line mode
    sendCommandOled(0x05);             //View 0°
    sendCommandOled(0x38);             //FunctionSet: N=1 DH=0 RE=0 IS=0
    sendCommandOled(0x3A);             //FunctionSet: N=1 BE=0 RE=1 IS=0
    sendCommandOled(0x72);             //ROM Selection (RE muss 1 sein)
    sendDataOled(0x00);                //ROM_A = 0x00, ROM_B = 0x04, ROM_C = 0x0C
    sendCommandOled(0x38);             //FunctionSet: N=1 DH=0 RE=0 IS=0
    sendCommandOled(0x0D);             //Display blink cursor on
    sendCommandOled(0x01);             //Clear display
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 // write volume level to screen
void setVolumeOled(int volume)                  
{
  const uint8_t Cijfers [10][6] = {                     // definitions of 0 till 9 used on the oled screen, stored in ram
  //0:
  {0,1,2,3,4,5},
  //1:
  {1,31,32,4,31,4},
  //2
  {6,6,2,3,7,7},
  //3
  {6,6,2,7,7,5},
  //4
  {3,4,2,32,32,31},
  //5
  {31,6,6,7,7,5},
  //6
  {0,6,6,3,7,5},
  //7
  {1,1,2,32,0,32},
  //8
  {0,6,2,3,7,5},
  //9
  {0,6,2,32,32,31},
  };
  volume=volume-63;
  if (!InitData.AmpPassive) {                    // if the amp is not in passive mode adjust volume displayed on screen
   volume = volume + Voloffset;                  // change volumelevel from 0 - 64 to (0 - offset)- to (64 - offset)
  }
  sendCommandOled(0xC0);                         // place cursor second row first character
  if (volume < 0) {                              // if volumlevel  is negative
    sendDataOled(1);                             // minus sign
  }
  else {
    sendDataOled(32);                            // no minus sign
  }
  unsigned int lastdigit=abs(volume%10);
  unsigned int firstdigit=abs(volume/10);         // below needs to be rewritten to be more efficient
  sendCommandOled(0x81);
  for (int c=0; c<3; c++){                        // write top of first figure
    sendDataOled(Cijfers[firstdigit][c]);  
  }
  for (int c=0; c<3; c++){                        // write top of second figure
    sendDataOled(Cijfers[lastdigit][c]);  
  }
  sendCommandOled(0xC1);
  for (int c=3; c<6; c++){                        // write bottem of first figure
    sendDataOled(Cijfers[firstdigit][c]);  
  }
  for (int c=3; c<6; c++){                        // every botomm of last firege
  sendDataOled(Cijfers[lastdigit][c]);  
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// write selected content to the right top of screen
void writeOLEDtopright(const char *OLED_string)   
{
    sendCommandOled(0x06); // set cursor in the middle
 // sendCommandOled(0x89); // set cursor in the middle
  unsigned char i = 0;                             // index 0
  while (OLED_string[i])                           // zolang er data is
  {
    sendDataOled(OLED_string[i]);                  // send characters in string to OLED, 1 by 1
    i++;
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// used to write values within menu to change varialbles.
void writeOLEDmenu(const char *OLED_string, uint8_t pos )   {

  sendCommandOled(pos);                           // set cursor in the middle
  //  sendCommandOled(0xC8);                           // set cursor in the middle
  unsigned char i = 0;                             // index 0
  while (OLED_string[i])                           // zolang er data is
  {
    sendDataOled(OLED_string[i]);                  // send characters in string to OLED, 1 by 1
    i++;
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// write selected content to right bottem of the screen
void writeOLEDbottemright(const char *OLED_string)   
{
  sendCommandOled(0xC9); // set cursor in the middle
  unsigned char i = 0;                             // index 0
  while (OLED_string[i])                           // zolang er data is
  {
    sendDataOled(OLED_string[i]);                  // send characters in string to OLED, 1 by 1
    i++;
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// set relays in status to support reqested volume
void setRelayVolume(int Volume) {
  int VolumeRight;
  int VolumeLeft; 
  if (Volume==0) {                                              // If volume=0 switch off all relays
    VolumeRight=0;
    VolumeLeft=0;
  } 
  else if (InitData.HeadphoneActive) {                          // if headphone is active we ignore balance
      VolumeRight=Volume;
      VolumeLeft=Volume;
  } 
  else {
      int BalanceRight = InitData.BalanceOffset >> 1;           // divide balanceOffset using a bitshift to the right    
      int BalanceLeft = BalanceRight - InitData.BalanceOffset;  // balance left is the remaining part of 
      VolumeRight = Volume + BalanceRight;                      // correct volume right with balance
      VolumeLeft = Volume + BalanceLeft;                        // correct volume left with balance
      if (VolumeRight > 63) VolumeRight = 63;                   // volume higher as 63 not allowed 
      if (VolumeLeft > 63) VolumeLeft = 63;                     // volume higher as 63 not allowed
      if (VolumeRight < 0) VolumeRight = 0;                     // volume lower as 0 not allowed
      if (VolumeLeft < 0) VolumeLeft = 0;                       // volume lower as 0 not allowed
  }               
  Wire.beginTransmission(MCP23017_I2C_ADDRESS_bottom);            // write volume left to left ladder
  Wire.write(0x13);
  Wire.write(VolumeLeft);
  Wire.endTransmission();
  // Wire.beginTransmission(MCP23017_I2C_ADDRESS_top);          // write volume right to right ladder
  // Wire.write(0x13);
  // Wire.write(VolumeRight);
  // Wire.endTransmission();
  if (debugEnabled) {
      Serial.print (F(" setRelayVolume : volume left = "));
      Serial.print (VolumeLeft);
      Serial.print (F(" ,volume right = "));
      Serial.print (VolumeRight);
      Serial.print(F(",  headphones : "));
      Serial.println (InitData.HeadphoneActive);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// set relays in status to support requested channnel
void setRelayChannel(uint8_t relay)             
{
  uint8_t inverseWord;                          
  if (relay==0){                                // if no channel is selected, drop all ports, this is done during  mute
    inverseWord=0xff;
  }
  else{
    inverseWord= 0xFF ^ (0x01 << (relay-1));    // bitshift the number of ports to get a 1 at the correct port and inverse word
  }
  Wire.beginTransmission(MCP23017_I2C_ADDRESS_bottom);
  Wire.write(0x12);
  Wire.write(inverseWord);
  Wire.endTransmission();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//initialize the MCP23017 controling the relays; set I/O pins to outputs; first do bank B, next A otherwise init fails. Default = icon=0
void MCP23017init()
{
  Wire.beginTransmission(MCP23017_I2C_ADDRESS_bottom);
  Wire.write(0x01);                            // IODIRB register
  Wire.write(0x00);                            // set all of port B to output
  Wire.write(0x13);                            // gpioB
  Wire.write(0xff);                            // set all ports first high, 
  Wire.write(0x00);                            // set all ports low
  Wire.write(0x00);                            // IODIRA register
  Wire.write(0x00);                            // set all of port A to outputs
  Wire.write(0x12);                            // gpioA
  Wire.write(0xFF);                            // set all ports high, low is active
  Wire.endTransmission();
}



