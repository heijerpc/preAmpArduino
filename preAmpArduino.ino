///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// v0.2, integration of oled screen
// v0.3 optimize and change, adapt volume part for screen and rotator
// v0.4 included functions to support relay, include 2 buttons (mute and channel) and restructured setup due to lack of readabilitie
//
// to do:
// rotary geeft puls bij power on, staat op 31 ipv 30
// codes toevoegen IR
// relay aansturing
// led aansturen
// optimize code driving oled screen

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// EEPROM related stuff to save volume level
//#include "EEPROM.h"
//int dBLevelEEPROMAddressBit = 0;
//bool isVolumeSavedToEeprom = true;
//unsigned long timeOfLastVolumeChange;
//unsigned long timeBetweenVolumeSaves = 60000;
//float maximumLevelToSave = -30.0;
//float currentSavedDbLevel;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// general definitions 
#include <Wire.h>                                     // include functions for i2c
const char* initTekst = "V 0.5";                      // current version of the code, shown in startscreen
bool debugEnabled = true;                             // define if debug is enabled, value should be changed in this line, either true or false
bool Alive = true;                                    // alive defines if we are in standby mode or acitve
#define PowerOnOff  10                                // pin connected to the relay handeling power on/off of the amp
#define ledStandby  11                                // connected to a led that is on if device is in standby
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// definitons for the IR receiver
#include <IRremote.hpp>                               // include drivers for IR 
#define IR_RECEIVE_PIN 9                              // datapin of IR is connected to pin 9
#define apple_left 0x77E190                           //7856528
#define apple_right 0x77E160                          //7856480
#define apple_up 136                                  //7856464    0x77E150
#define eindhoven_up 16 
#define apple_down 140                                //7856464    0x77E130
#define eindhoven_down 17
#define apple_middle 0x77E1A0                         //7856544 
#define apple_menu 0x77E1C0                           //7856576
#define apple_repeat 0xFFFFFF                         // 16777215
uint8_t lastcommand;                                  // preivious command coming from IR
uint8_t delaytimer;                                       // timer to delay between volume changes, actual value set in main loop
// decode_results results;
// String lastIRoperation;
// float iRIncrement = 5;
unsigned long timeOfLastIRChange;
// keycodes: (6 LSB bits)
// numeric 0-9 : 0x00 - 0x09
//    vol-  vol+ : 0x11 0x10
//     ch-   ch+ : 0x21 0x20
//   bal.l bal.r : 0x1b 0x1a
//   mute  audio : 0x0d 0xcb
//    stop  play : 0x36 0x35
//pause pwrToggle: 0x30 0x0c
//    left right : 0x55 0x56 // RC5X
//       down up : 0x51 0x50 // RC5X
// standby pwrOn : 0x3d 0x3f





///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// definitions for the oled screen
#define OLED_Address 0x3C                             // 3C is address used by oled controler
#define OLED_Command_Mode 0x80                        // Control byte indicates next byte will be a command
#define OLED_Data_Mode 0x40                           // Control byte indicates next byte will be data
const uint8_t ContrastLevel=0x3f;                     // level of contrast, between 00 anf ff, default is 7f
const uint8_t Voloffset=0;                            // diffence between volumelevel and volumelevel displayed on the screen
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
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// definitions for the attenuator board
#define MCP23017_I2C_ADDRESS 0x24                  // I2C address of the relay board
int AttenuatorLevel = 30;                          // geluidssterkte bij initialisatie
volatile int AttenuatorChange = 0;                 // change of volume out of interupt function
bool muteEnabled = false;                          // status of Mute

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// definitions for the rotary
#define  rotaryButton   8                          //pin 8 is the switch of the rotary
volatile int rotaryPinA = 3;                       //encoder pin A, volatile as its addressed within the interupt of the rotary
volatile int rotaryPinB = 4;                       //encoder pin B, volatile as its addressed within the interupt of the rotary

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// definitions for the buttons                      (definitions for rotary button are with rotary)
#define buttonMute  5                              // pin 5 mute on/off, pullup
#define buttonChannel  6                           // pin 6 is change input channel round robin, pullup
#define buttonStandby  7                           // pin 7 is standby on/off, pullup
const char* inputTekst[3] = {                      // definitions of 0 till 9 used on the oled screen, stored in ram
  //channel 1
  "chan 1",
  //channel 2
  "chan 2",
  //channel 3
  "chan 3"
  };
  int selectedInput = 1;                            // selected input channel at startup

// System stuff
unsigned long timeOfLastOperation;
unsigned long timeToSleep = 120000;
bool sleeping;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// definitions for the compiler as we have nested functions
void changeMute();                                             // enable disable mute
void changeInput();                                            // change the input channel
void changeVolume(int increment);                              // verander volume
void setMCP23Sxx (int select_register, int register_value);      
void rotaryTurn();                                             // interupt handle voor als aan rotary wordt gedraaid
void OledSchermInit();                                         // init procedure for Oledscherm
void setVolumeOled(int volume);                                // write volume level to screen
void writeOLEDtopright(const char *OLED_string);               // write tekst to screen top right
void writeOLEDbottemright(const char *OLED_string);            // write tekst to screen bottem right
void MCP23017init();                                           // init procedure for relay extender
void setRelayChannel(uint8_t Word);                            // set relays to selected input channel
void setRelayVolume(uint8_t Word);                             // set relays to selected volume level
void changeStandby();                                          // moves from active to standby and back to active

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Setup
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup()
{
  digitalWrite(PowerOnOff, HIGH);                              // make pin of standby high, relays&screen on and power of amp is turned on
  digitalWrite(ledStandby, LOW);                               // turn off standby led to indicate device is in powered on
  delay(3000);                                                 // wait till all powered up
  if (debugEnabled) {                                          // if debuglevel on start monitor screen
    Serial.begin (9600);
  }
  Wire.begin();                                                // open i2c channel
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // initialize the oled screen
  OledSchermInit();                                            // intialize the Oled screen
  writeOLEDtopright(initTekst);                                // write version number
  delay(1000);
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // initialize the relay extender
  MCP23017init();                                              // initialize the relays                                                              
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

  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 
  // // Set inital attenuator level
  setRelayVolume(AttenuatorLevel);                                            //set the relays in the start volume
  setVolumeOled(AttenuatorLevel);                                             //display volume level on oled screen
  writeOLEDtopright(inputTekst[selectedInput-1]);                             //display input channel on oled screen
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 
  // general
  pinMode (PowerOnOff, OUTPUT);                                               //control the relay that provides power to the rest of the amp
  pinMode (ledStandby, OUTPUT);                                               //led will be on when device is in standby mode
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
    if (digitalRead(buttonChannel) == LOW) {                // if button channel switch is pushed
      delay(500);                                           // wait to prevent multiple switches
      changeInput();                                        // change input channel

    }
    if (digitalRead(buttonMute) == LOW) {                   // if button mute is pushed
      delay(500);                                           // wait to prevent multiple switches
      changeMute();                                         // change status of mute
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
        changeVolume(1);
        delay(delaytimer);
        break;
      case apple_down:
      case eindhoven_down:
        changeVolume(-1);
        delay(delaytimer);
        break;
    }
 //   lastcommend=IrReceiver.decodedIRData.command;
    timeOfLastIRChange = millis();                            // store time to detect if button is still pressed or not 
    IrReceiver.resume();                                      // open IR receiver for next command
  }
}
  // Decode the IR if recieved
  // if (irrecv.decode(&results)) {
  //   if (results.value == 2011287694) {
  //     lastIRoperation = "volumeUp";
  //     changeVolume(iRIncrement);
  //     timeOfLastIRChange = millis();
  //   }
  //   if (results.value == 2011279502) {
  //     lastIRoperation = "volumeDown";
  //     changeVolume(-iRIncrement);
  //     timeOfLastIRChange = millis();
  //   }
  //   if ((results.value == 2011291790) or (results.value == 2011238542)) {
  //     lastIRoperation = "changeInput";
  //     changeInput();
  //     delay(500);
  //     timeOfLastIRChange = millis();
  //   }
  //   if (results.value == 2011265678) {
  //     lastIRoperation = "playPause";
  //     changeMute();
  //     timeOfLastIRChange = millis();
  //   }
  //   if (results.value == 4294967295 && lastIRoperation != "None") {
  //     if (lastIRoperation == "volumeUp") { changeVolume(iRIncrement); timeOfLastIRChange = millis(); }
  //     if (lastIRoperation == "volumeDown") { changeVolume(-iRIncrement); timeOfLastIRChange = millis(); }
  //     if (lastIRoperation == "changeInput") { changeInput(); delay(500); timeOfLastIRChange = millis(); }
  //     timeOfLastIRChange = millis();
  //   }
  //   irrecv.resume(); // Receive the next value
  // }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Read button state buttonChannel



//   // Set the time. This is used by other functions
//   unsigned long currentTime = millis();
//   // Stop IR repeating after timeout to stop interference from other remotes
//   if ((currentTime - timeOfLastIRChange) > 1000 && lastIRoperation != "None") {
//     lastIRoperation = "None";
//   }
// }
void changeStandby()
{

  if (Alive) {                                                     // move from alive to standby, turn off screen, relays and amp
    if (debugEnabled) {
      Serial.print (" changeStandby : moving from active to standby,  ");
    }
    Wire.end();                                                    // stop the i2c bus
    delay(1000);                                                   // wait to stable state
    digitalWrite(PowerOnOff, LOW);                                 // make pin of standby low, relay&screen off and power of amp is turned off
    digitalWrite(ledStandby, HIGH);                                // turn on standby led to indicate device is in standby
    delay (2000);                                                  // wait to stable state
    if (debugEnabled) {
      Serial.println (" status is now standby ");
    }
    Alive=false;                                                   // we are in standby, alive is false
  } 
  else {
    if (debugEnabled) {
      Serial.print (" changeStandby : moving from standby to active,  ");
    }
    digitalWrite(PowerOnOff, HIGH);                                // make pin of standby high, relays&screen on and power of amp is turned on
    digitalWrite(ledStandby, LOW);                                 // turn off standby led to indicate device is in powered on
    delay(3000);                                                   // we first wait for stable state
    Wire.begin();                                                  // open i2c channel
    delay(1000);                                                   // needed to get i2c channel stable
    OledSchermInit();                                              // intialize the Oled screen
    delay(100);
    writeOLEDtopright(initTekst);                                  // write version number
    delay(1000);                                                   // show the initTekst for a while
    MCP23017init();                                                // initialize the relays
    setRelayVolume(AttenuatorLevel);                               // set the relays in the start volume
    setVolumeOled(AttenuatorLevel);                                // display volume level on oled screen
    writeOLEDtopright(inputTekst[selectedInput-1]);                // display input channel on oled screen  
    if (debugEnabled) {
      Serial.println (" status is now active ");
    }
    Alive=true; 
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Interrupt Service Routine for a change to Rotary Encoder pin A
void rotaryTurn()
{
  if (digitalRead (rotaryPinB)){
  	AttenuatorChange=-1;   // rotation left, turn volume down
  }
	else {
     AttenuatorChange=1;  // rotatate right, turn volume Up!
  }  

}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function to change to next inputchannel
void changeInput()
{
  if (debugEnabled) {
    Serial.print (" changeInput: old selected Input: ");
    Serial.print (selectedInput);
  }
  timeOfLastOperation = millis();
  sleeping = false;
  selectedInput++;
  if (selectedInput > 3) { selectedInput = 1; }
  if (selectedInput < 1) { selectedInput = 3; }
  setRelayVolume(0); 
  delay(15); 
  setRelayChannel(selectedInput); 
  delay(15);  
  if (!(muteEnabled)){
    setRelayVolume(AttenuatorLevel); 
  }                                         
  writeOLEDtopright(inputTekst[selectedInput-1]);
  if (debugEnabled) {
    Serial.print(", new Selected Input: ");
    Serial.println (selectedInput);
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Function to set a specific attenuator level
//////////////////////////////////////////////////////////////////////////////////////////////
// Functions to change volume 
void changeVolume(int increment) 
{
  timeOfLastOperation = millis();
  sleeping = false;
  if (not muteEnabled)
  {
    if (debugEnabled) {
      Serial.print ("ChangeVolume: volume was : ");   
      Serial.print (AttenuatorLevel); 
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
      Serial.print (", volume is now : ");
      Serial.println (AttenuatorLevel);
    }
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Function to change mute 
void changeMute() 
{
  timeOfLastOperation = millis();
  sleeping = false;
  if (muteEnabled) {
    if (debugEnabled) {
       Serial.print ("ChangeMute : Mute now disabled, setting volume to: ");
       Serial.println (AttenuatorLevel);
    }
    setRelayChannel(selectedInput); 
    delay(15);                                                                    // set relays to support origical inputchnannel
    setRelayVolume(AttenuatorLevel);                                              //  set relays to support orgical volume
    writeOLEDbottemright("      ");                                               //  update oled screen
    muteEnabled = false;
  } 
  else {
    setRelayVolume(0);
    delay(15);                                                              // set relays to zero volume
    setRelayChannel(0);                                                             // set relays to no input channel
    writeOLEDbottemright(" Muted");                                                // update oled screen
    muteEnabled = true;
    if (debugEnabled)     {
      Serial.println ("Changemute: Mute enabled");
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
  delay(1000);
  sendCommandOled(0x2E);                         // Function Set, set RE=1, enable double height
  sendCommandOled(0x08);                         // Extended Function Set, 5-dot, disable cursor-invert, 2-line mode
  sendCommandOled(0x72);                         // Function Selection B, first command, followed by data command
  sendDataOled(0x00);                            // Set ROM A with 8 custom characters
  sendCommandOled(0x79);                         // set SD=1, enable OLED Command Set
  sendCommandOled(ContrastLevel);                // Set Contrast Control [brightness]
  sendCommandOled(0x7F);                         // default is 0x7F, 00 is min,  0xFF is max
  sendCommandOled(0x78);                         // set SD=0, disable OLED Command Set
  sendCommandOled(0x28);                         // Function Set, set RE=0
  StoreLargecijfer();                            // write large cijfers to dram
  sendCommandOled(0x01);                         // Clear Display and set DDRAM location 00h
  sendCommandOled(0x0C);                         // turn display ON, cursor OFF, blink OFF
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 // write volume level to screen
void setVolumeOled(int volume)                  
{
  volume = volume - Voloffset;                   // change volumelevel from 0 - 64 to (0 - offset)- to (64 - offset)
  sendCommandOled(0xC0);                         // place cursor second row first character
  if (volume < 0) {                              // if volumlevel  is negative
    sendDataOled(1);                             // minus sign
  }
  else {
    sendDataOled(32);                          // no minus sign
  }
  unsigned int lastdigit=abs(volume%10);
  unsigned int firstdigit=abs(volume/10);      // below needs to be rewritten to be more efficient
  sendCommandOled(0x81);
  for (int c=0; c<3; c++){               // write top of first figure
    sendDataOled(Cijfers[firstdigit][c]);  
  }
  for (int c=0; c<3; c++){               // write top of second figure
    sendDataOled(Cijfers[lastdigit][c]);  
  }
  sendCommandOled(0xC1);
  for (int c=3; c<6; c++){               // write bottem of first figure
    sendDataOled(Cijfers[firstdigit][c]);  
  }
  for (int c=3; c<6; c++){               // every botomm of last firege
  sendDataOled(Cijfers[lastdigit][c]);  
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// write selected content to the right top of screen
void writeOLEDtopright(const char *OLED_string)   
{
  sendCommandOled(0x89); // set cursor in the middle
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
void setRelayVolume(uint8_t Word)             
{
  Wire.beginTransmission(MCP23017_I2C_ADDRESS);
  Wire.write(0x13);
  Wire.write(Word);
  Wire.endTransmission();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// set relays in status to support requested channnel
void setRelayChannel(uint8_t relay)             // send data to the display
{
  uint8_t inverseWord;
  if (relay==0){
    inverseWord=0xff;
  }
  else{
    inverseWord= 0xFF ^ (0x01 << (relay-1)); 
  }
  Wire.beginTransmission(MCP23017_I2C_ADDRESS);
  Wire.write(0x12);
  Wire.write(inverseWord);
  Wire.endTransmission();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//initialize the MCP23017 controling the relays; set I/O pins to outputs
void MCP23017init()
{
  Wire.beginTransmission(MCP23017_I2C_ADDRESS);
  Wire.write(0x00);                            // IODIRA register
  Wire.write(0x00);                            // set all of port A to outputs
  Wire.write(0x01);                            // IODIRB register
  Wire.write(0x00);                            // set all of port B to outputs
  Wire.write(0x12);                            // gpioA
  Wire.write(0xFF);                            // set all ports high, low is active
  Wire.write(0x13);                            // gpioB
  Wire.write(0x00);                            // set all ports low, high is active
  Wire.endTransmission();
}

