///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// v2, integration of oled screen
// v3 optimize and change, adapt volume part for screen and rotator
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// If enabled volume/input data will be printed via serial
bool debugEnabled = true;

// EEPROM related stuff to save volume level
//#include "EEPROM.h"
//int dBLevelEEPROMAddressBit = 0;
//bool isVolumeSavedToEeprom = true;
//unsigned long timeOfLastVolumeChange;
//unsigned long timeBetweenVolumeSaves = 60000;
//float maximumLevelToSave = -30.0;
//float currentSavedDbLevel;

// SPI library
//#include "SPI.h"
// Arduino pin 9 & 10 = Input selector & Relay attenuator
// Arduino pin 11 = SDI
// Arduino pin 13 = CLK

// IR stuff
#include "IRremote.h"
int RECV_PIN = A3;  // IR Receiver pin
const int IRGroundPin = A4;
const int IRPowerPin = A5;
IRrecv irrecv(RECV_PIN);
decode_results results;
String lastIRoperation;
float iRIncrement = 5;
unsigned long timeOfLastIRChange;

// definitions for the oled screen
#include "oledScreen.h"                               // all routines for controling the oled screen, for additonal info see .j and .cpp file
#include <Wire.h>                                     // include functions for i2c

// definitions for the Input selector
int selectedInput = 1;                             // selected input channel at startup

// Relay attenuator stuff
const int AttenuatorCSPin = 10;
int AttenuatorLevel = 30;                          // geluidssterkte
volatile int AttenuatorChange = 0;                 // change of volume out of interupt function
bool muteEnabled = false;                          // status of Mute

// definitions for the rotary
volatile int rotaryPinA = 3;                       //encoder pin A, volatile as its addressed within the interupt of the rotary
volatile int rotaryPinB = 4;                       //encoder pin B, volatile as its addressed within the interupt of the rotary
#define  rotaryButton   8                          //pin 8 is the switch of the rotary
static  byte abOld;                                 //TEST

// definitions for the buttons 
//const int buttonPin = 2;
#define buttonMute  5                              //
#define buttonChannel  6                           //
#define buttonStandby  7                           //

// System stuff
unsigned long timeOfLastOperation;
unsigned long timeToSleep = 120000;
bool sleeping;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// definitions for the compiler as we have nested functions
void changeMute();
void changeInput();
void setAttenuatorLevel (byte level);
void changeVolume(int increment);
void setMCP23Sxx (int select_register, int register_value);
void rotaryTurn();


//////////////////////////////////////////////////////////////////////////////////////////////
// Setup
//////////////////////////////////////////////////////////////////////////////////////////////
void setup()
{
  // Serial
  if (debugEnabled) {
    Serial.begin (9600);
  }
  Wire.begin();
  // // SPI
  // // set the CS pins as output:
  // pinMode (inputSelectorCSPin, OUTPUT);
  // digitalWrite(inputSelectorCSPin,HIGH);
  // pinMode (AttenuatorCSPin, OUTPUT);
  // digitalWrite(AttenuatorCSPin,HIGH);
  // // Start SPI
  // if (debugEnabled) {
  //   Serial.println ("Starting SPI..");
  // }
  // SPI.begin();
  // // Set SPI selector IO direction to output for all pinds
  // if (debugEnabled) {
  //   Serial.println ("Setting SPI selector IO direction control registers..");
  // }
  // digitalWrite(inputSelectorCSPin,LOW);
  // SPI.transfer(B01000000); // Send Device Opcode
  // SPI.transfer(0); // Select IODIR register
  // SPI.transfer(0); // Set register
  // digitalWrite(inputSelectorCSPin,HIGH);
  //
  ////////////////////////////////////////////////////////////////////////////////////////////
  // Set up pins for rotary:
  pinMode (rotaryPinA,INPUT_PULLUP);                                   // pin A rotary is high and its an input port
  pinMode (rotaryPinB,INPUT_PULLUP);                                   // pin A rotary is high and its an input port
  // digitalWrite (rotaryPinA, HIGH);                                      // enable pull-up
  //digitalWrite (rotaryPinB, HIGH);                                      // enable pull-up
  attachInterrupt(digitalPinToInterrupt(rotaryPinA), rotaryTurn, RISING); // if pin encoderA changes run rotaryTurn

  pinMode (rotaryButton, INPUT_PULLUP);                                 // input port and enable pull-up

  ////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // IR
  irrecv.enableIRIn(); // Start the receiver
  pinMode(IRPowerPin, OUTPUT);
  pinMode(IRGroundPin, OUTPUT);
  digitalWrite(IRPowerPin, HIGH); // Power for the IR
  digitalWrite(IRGroundPin, LOW); // GND for the IR

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // initialize the oled screen
  OledSchermInit();                               // intialize the Oled screen
  setVolumeOled(AttenuatorLevel);
  //char text= "input ";
  //text = text + String(selectedInput);
  //writeOLEDtopright(text);
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Button setup
  pinMode (buttonMute, INPUT_PULLUP);
  pinMode (buttonChannel, INPUT_PULLUP);
  pinMode (buttonStandby, INPUT_PULLUP);

  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 
  // Attenuator setup
  // Set IO direction to output for all bank 0 (I/O pins 0-7)
  // digitalWrite(AttenuatorCSPin,LOW);
  // SPI.transfer(B01000000); // Send Device Opcode
  // SPI.transfer(0);         // Select IODIR register for bank 0
  // SPI.transfer(0);         // Set register
  // digitalWrite(AttenuatorCSPin,HIGH);
  // // Set IO direction to output for all bank 1 (I/O pins 8-17)
  // digitalWrite(AttenuatorCSPin,LOW);
  // SPI.transfer(B01000000); // Send Device Opcode
  // SPI.transfer(1);         // Select IODIR register for bank 1
  // SPI.transfer(0);         // Set register
  // digitalWrite(AttenuatorCSPin,HIGH);
  // // Set inital attenuator level

  setAttenuatorLevel(AttenuatorLevel);
  setVolumeOled(AttenuatorLevel);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// Main loop
//////////////////////////////////////////////////////////////////////////////////////////////
void loop()
{
  // Read the encoder and change volume, function is triggered by interupt who changes attenuatorchange
  if (AttenuatorChange != 0) {                            // if AttenuatorChange is changed by the interupt we change  Volume
    changeVolume(AttenuatorChange);                       // Attenuatorchange is either pos or neg depending on rotation
    AttenuatorChange = 0;                                 // reset the value to 0
  }


  // Decode the IR if recieved
  if (irrecv.decode(&results)) {
    if (results.value == 2011287694) {
      lastIRoperation = "volumeUp";
      changeVolume(iRIncrement);
      timeOfLastIRChange = millis();
    }
    if (results.value == 2011279502) {
      lastIRoperation = "volumeDown";
      changeVolume(-iRIncrement);
      timeOfLastIRChange = millis();
    }
    if ((results.value == 2011291790) or (results.value == 2011238542)) {
      lastIRoperation = "changeInput";
      changeInput();
      delay(500);
      timeOfLastIRChange = millis();
    }
    if (results.value == 2011265678) {
      lastIRoperation = "playPause";
      changeMute();
      timeOfLastIRChange = millis();
    }
    if (results.value == 4294967295 && lastIRoperation != "None") {
      if (lastIRoperation == "volumeUp") { changeVolume(iRIncrement); timeOfLastIRChange = millis(); }
      if (lastIRoperation == "volumeDown") { changeVolume(-iRIncrement); timeOfLastIRChange = millis(); }
      if (lastIRoperation == "changeInput") { changeInput(); delay(500); timeOfLastIRChange = millis(); }
      timeOfLastIRChange = millis();
    }
    irrecv.resume(); // Receive the next value
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Read button state buttonChannel

  if (digitalRead(buttonChannel) == LOW) {
    changeInput();
    delay(500);
  }
   if (digitalRead(buttonMute) == LOW) {
    changeMute();
    delay(500);
  }

  // Set the time. This is used by other functions
  unsigned long currentTime = millis();
  // Stop IR repeating after timeout to stop interference from other remotes
  if ((currentTime - timeOfLastIRChange) > 1000 && lastIRoperation != "None") {
    lastIRoperation = "None";
  }
}

/////////////////////////////////////////////////////
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

//////////////////////////////////////////////////////////////////////////////////////////////
// Function to change to next input
void changeInput()
{
  if (debugEnabled) {
    Serial.print ("old selected Input: "+ selectedInput);
  }
  timeOfLastOperation = millis();
  sleeping = false;
  selectedInput++;
  if (selectedInput > 3) { selectedInput = 1; }
  if (selectedInput < 1) { selectedInput = 3; }

  // switch (selectedInput) {
  //   case 1:
  //     setMCP23Sxx(9, B01001011);
  //     delay(5);
  //     break;
  //   case 2:
  //     setMCP23Sxx(9, B00110011);
  //     delay(5);
  //     break;
  //   case 3:
  //     setMCP23Sxx(9, B00101101);
  //     delay(5);
  //     break;
  //writeOLEDtopright("input " + String(selectedInput));
  //}
  writeOLEDtopright("input " + selectedInput);
  if (debugEnabled) {
    Serial.println (", new Selected Input: "+ selectedInput);
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Function to send data to the MCP23S17 (18 = GPIOA, 19 = GPIOB)
void setMCP23Sxx (int register_value) 
{
//  digitalWrite(CSPin,LOW);
//  SPI.transfer(B01000000);
//  SPI.transfer(select_register);
//  SPI.transfer(register_value);
//  digitalWrite(CSPin,HIGH);
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Function to mirror and reverse a byte. This is used because the relays connected to one side of the setMCP23Sxx are reversed to the other.
int reverseAndMirrorByte (byte myByte)
{
  byte myByteReversedAndMirrored;
  for (int reverseBit = 0; reverseBit < 8; reverseBit++) {
    if (bitRead(myByte, reverseBit) == 1) {
      bitWrite(myByteReversedAndMirrored, (7 - reverseBit), 0);
    } else {
      bitWrite(myByteReversedAndMirrored, (7 - reverseBit), 1);
    }
  }
  return myByteReversedAndMirrored;
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Function to reverse a byte. This is used because the relays are connected to the setMCP23Sxx in reverse order.
int reverseByte (byte myByte)
{
  byte myByteReversed;
  for (int reverseBit = 0; reverseBit < 8; reverseBit++) {
    if (bitRead(myByte, reverseBit) == 1) {
      bitWrite(myByteReversed, (7 - reverseBit), 1);
    } else {
      bitWrite(myByteReversed, (7 - reverseBit), 0);
    }
  }
  return myByteReversed;
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Function to set a specific attenuator level
void setAttenuatorLevel (byte level) 
{
  muteEnabled = false;
  sleeping = false;
  if (debugEnabled) {
    Serial.print ("setAttenuatorLevel Attenuator Level: ");
    Serial.println (level);
  }
  byte real_level = 255 - level;
  byte gpioaValue = reverseByte(real_level);
  byte gpiobValue = reverseAndMirrorByte(gpioaValue);
 // setMCP23Sxx(AttenuatorCSPin, 18, gpioaValue);
 // setMCP23Sxx(AttenuatorCSPin, 19, gpiobValue);
 // delay(1);
 // setMCP23Sxx(AttenuatorCSPin, 18, B00000000);
  //setMCP23Sxx(AttenuatorCSPin, 19, B00000000);
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Functions to change volume 
void changeVolume(int increment) 
{
  timeOfLastOperation = millis();
  sleeping = false;
  if (not muteEnabled)
  {
    if (debugEnabled) {
      Serial.print ("setAttenuatorLevel: volume was");   
      Serial.println (AttenuatorLevel); 
    }
    AttenuatorLevel = AttenuatorLevel + increment;                            // define new attenuator level
    if (AttenuatorLevel > 63) {                                               // volume cant be higher as 63
      AttenuatorLevel=63; 
    }
    if (AttenuatorLevel < 0 ) {                                               // volume cant be lower as 0
      AttenuatorLevel=0;
    } 
    //setAttenuatorLevel(AttenuatorLevel);                                     // schrijf new volume level to the ladder
    setVolumeOled(AttenuatorLevel);                                        // schrijf new volume level to the screen
    if (debugEnabled) {
      Serial.print ("setAttenuatorLevel: volume is now");
      Serial.println (AttenuatorLevel);
    }
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Function to change mute on input selector
void changeMute() 
{
  timeOfLastOperation = millis();
  sleeping = false;
  if (muteEnabled) {
    if (debugEnabled) {
      Serial.print ("ChangeMute Mute now disabled, setting volume to: ");
      Serial.println (AttenuatorLevel);
    }
    setAttenuatorLevel(AttenuatorLevel);
    writeOLEDbottemright("     ");
    muteEnabled = false;
  } 
  else {
    setAttenuatorLevel(0);
    writeOLEDbottemright(" Muted");
    muteEnabled = true;
    if (debugEnabled) {
      Serial.println ("Changemute Mute enabled");
    }
  }
}
