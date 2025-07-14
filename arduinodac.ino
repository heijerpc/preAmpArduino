// v0.1  first version, on/off, input select, screen on/off, bit and freq counter
#include <Arduino.h>
#include <digitalWriteFast.h>              
// Global definitions 
//#define debugDAC                                  // Comment this line when debugPreAmp mode is not needed
#define freqcounter                                 // defines if frequentie counter is included
#define oledPresent                                 // defines if Oled screen is included
// pin definitions
#define powerOnOff A0           // pin connected to the relay handeling power on/off of the dac
#define inputChanSelect A1      // pin connected to the relay handeling input select
#define screenOnOff A2          // pin connected to the relay handeling screen on/off
#define buttonInputChannel 6    // pin is conncected to the button to change input channel round robin, pullup
#define buttonStandby 4         // pin is connected to the button to change standby on/off, pullup
#define ledInput 5              // connected to a led when usb input is selected
#define ledStandby 11           // connected to a led that is on if amp is in standby mode
#ifdef freqcounter              // if frequency counter is enabled
  #define freqCount 10          // pin connected to freqency counter
  //#define frameHoldReset 6      // connected to bit reset pin of frame output
  #define silent 7              // connected to silent pin	
  #define Bit24Word 8           // connected to 24bit pin
  #define Bit32Word 9           // connected to 32bit pin
  volatile int long ticker = 0;             // current counter of ticks
  int long totalTicks = 0;                  // total number of ticks in a second
  unsigned long timeStartCountTicks = 0;    // time starting counting ticks
  unsigned long timeUpdateScreen = 0;       // time to update screen
  char frequencyValue[10];                  // frequency in char 
  char bitDepthValue[8];                    // bitdepth in char
  bool measureFreq = false;                 // determines if measurement of freq is valid
#endif
#ifdef oledPresent                                    // if oled screen is used
  #define oledReset 12                                // connected to the reset port of Oled screen, used to reset Oled screen
  const char* topTekst = "PeWalt, V 0.1";             // current version of the code, shown in startscreen top, content could be changed
  const char* middleTekst = "    ";                   //as an example const char* MiddleTekst = "Cristian, please wait";
  const char* bottemTekst = " " ;                     //as an example const char* BottemTekst = "design by: Walter Widmer" ;
  #define oledI2CAddress 0x3C                         // 3C is address used by oled controler
  #define fontH08 u8g2_font_timB08_tr                 // 11w x 11h, char 7h
  #define fontH08fixed u8g2_font_spleen5x8_mr         // 15w x 14h, char 10h
  #define fontH10 u8g2_font_timB10_tr                 // 15w x 14h, char 10h
  #define fontH10figure u8g2_font_spleen8x16_mn       //  8w x 13h, char 12h
  #define fontH14 u8g2_font_timB14_tr                 // 21w x 18h, char 13h
  #define fontgrahp u8g2_font_open_iconic_play_2x_t   // 16w x 16h pixels
  #define fontH21cijfer u8g2_font_timB24_tn           // 17w x 31h, char 23h
  #include <Wire.h>                                   // include functions for i2c
  #include <U8g2lib.h>                                // include graphical based character mode library
  U8G2_SSD1309_128X64_NONAME0_F_HW_I2C Screen(U8G2_R0);  // define the screen type used.
#endif
//  general definitions
bool dacAlive = false;         // boolean, defines if dac is on or off
bool usbSelected = false;     // boolean, defines if usb is selected  

#ifdef oledPresent              // if oled screen is used
// proc for intialisation of the screen after powerup of screen.
  void oledSchermInit() {  
    digitalWrite(oledReset, LOW);                                        // set screen in reset mode
    delay(10);                                                           // wait to stabilize
    digitalWrite(oledReset, HIGH);                                       // set screen active
    delay(110);                                                          // wait to stabilize
    Screen.setI2CAddress(oledI2CAddress * 2);                            // set oled I2C address
    Screen.initDisplay();                                                // init the screen
    Screen.clearDisplay();
    Screen.setPowerSave(1);                                                      
//  Screen.setContrast((((Amp.ContrastLevel * 2) + 1) << 4) | 0x0f);     // set contrast level, reduce number of options
    Screen.setFlipMode(1);
  }
// proc to write fixed values on OLED screen
  void writeFixedValuesScreen() {  // write stable values on the screen 
    Screen.clearBuffer();
    Screen.setFont(fontH08fixed);
    Screen.setCursor(0,16);
    Screen.print("Input     : ");
    Screen.setCursor(0,32);
    Screen.print("frequency : "); 
    Screen.setCursor(0,52);
    Screen.print("Bitdepth  : ");
    Screen.sendBuffer();
  }
  void writeValuesScreen() {  // write stable values on the screen 
    Screen.setCursor(60,16);
    if (usbSelected) {                      // usb is active
      Screen.print("USB    ");            
    } 
    else {                   // if raspberry is active
      Screen.print("Volumio");            
    }
    Screen.setCursor(60,32);
    Screen.print(frequencyValue); 
    Screen.setCursor(60,52);
    Screen.print(bitDepthValue);
    Screen.sendBuffer();
  }
#endif

// proc to change input channel, raspberry or usb
void changeInputChannel() {
  if (usbSelected) {
    usbSelected = false;
    digitalWrite(inputChanSelect, LOW);
    digitalWrite(ledInput, LOW); 
  }
  else {
    usbSelected = true;
    digitalWrite(inputChanSelect, HIGH);
    digitalWrite(ledInput, HIGH);  
  }
  #ifdef debugPreAmp                                 // if debugPreAmp enabled write message
  Serial.println(F("changeInputChannel, input channel changed "));
  #endif
}
// proc to switch dac between active and standby
void changeOnOff() {
  if (dacAlive) {
    dacAlive = false;
    digitalWrite(powerOnOff, HIGH);
    digitalWrite(ledStandby, HIGH);
    digitalWrite(screenOnOff, HIGH);
    delay(500);
    digitalWrite(screenOnOff, LOW);
    #ifdef oledPresent
      Screen.setPowerSave(1);  
    #endif
  }
  else {
    dacAlive = true;
    digitalWrite(powerOnOff, LOW);
    digitalWrite(ledStandby, LOW);
    digitalWrite(screenOnOff, HIGH);
    delay(500);
    digitalWrite(screenOnOff, LOW);
    #ifdef oledPresent
      Screen.setPowerSave(0);  
      writeFixedValuesScreen();  // write fixed values on the screen
    #endif
  }
  #ifdef debugPreAmp                                 // if debugPreAmp enabled write message
  Serial.println(F("changeOnOff: status of dac changed "));
  #endif
}
#ifdef freqcounter            // if frequency counter is enabled
// interupt routine counting ticks due to freqency counting
void intFreqCount() {
  ticker++;
}  
// determine freq and bit depth
// frequencies 44,1 48, 88,2, 96, 176,4 192 352,8 384, 705,6 768
// total ticks is devided by 100 

void determineFreqAndBitDepth() {
  totalTicks = ticker;                        // save the value of # ticks
  measureFreq = false;
  ticker = 0;                                 // reset ticker
  timeStartCountTicks = millis();
  if (totalTicks < 400)  {strcpy(frequencyValue, "no signal");}
  if (totalTicks > 400) {strcpy(frequencyValue,  " 44,1 Khz");}
  if (totalTicks > 470) {strcpy(frequencyValue,  "   48 Khz");}
  if (totalTicks > 600) {strcpy(frequencyValue,  " 88,2 Khz");}
  if (totalTicks > 950) {strcpy(frequencyValue,  "   96 Khz");}
  if (totalTicks > 1100) {strcpy(frequencyValue, "176,4 Khz");} 
  if (totalTicks > 1800) {strcpy(frequencyValue, "  192 Khz");}
  if (totalTicks > 2100) {strcpy(frequencyValue, "352,8 Khz");}
  if (totalTicks > 3600) {strcpy(frequencyValue, "  384 Khz");}
  if (totalTicks > 4100) {strcpy(frequencyValue, "705,6 Khz");}
  if (totalTicks > 7100) {strcpy(frequencyValue, "  768 Khz");}
  if (!digitalReadFast(silent)) {strcpy(bitDepthValue, "Silent ");}
  else {strcpy(bitDepthValue, "16 bits");}
  if digitalReadFast(Bit24Word) {strcpy(bitDepthValue, "24 bits");}
  if digitalReadFast(Bit32Word) {strcpy(bitDepthValue, "32 bits");}
}
#endif

// setup
void setup() { 
  #ifdef debugDAC
    Serial.begin(9600);  // if debugDAC on start monitor screen
    Serial.println(F("debug on  "));
  #endif  
  // pin modes
  pinMode(powerOnOff, OUTPUT);               // control the relay that provides power to the rest of the dac
  pinMode(inputChanSelect, OUTPUT);          // control the relay that controls inpu select
  pinMode(screenOnOff, OUTPUT);              // control a pin that switches the raspberry screen on or off
  pinMode(buttonInputChannel, INPUT_PULLUP); // button to select input channel
  pinMode(buttonStandby, INPUT_PULLUP);      // button to go into standby/active
  pinMode(ledStandby, OUTPUT);               // led will be on when device is in standby mode
  pinMode(ledInput, OUTPUT);                 // led will be on when usb input is selected

  // write init state to output pins
  digitalWrite(powerOnOff, LOW);             // keep dac turned off
  digitalWrite(inputChanSelect, LOW);        // select raspberry
  digitalWrite(screenOnOff, LOW);              // control a pin that switches the raspberry screen on or off
  digitalWrite(ledStandby, LOW);             // turn off standby led to indicate device is active
  digitalWrite(ledInput, LOW);               // turn off standby led to indicate raspberry is selected

  #ifdef freqcounter                         // if frequency counter is enabled
    attachInterrupt(digitalPinToInterrupt(freqCount), intFreqCount, RISING);
    pinMode(freqCount, INPUT);               // pin connected to frequency counter
    pinMode(silent, INPUT);               // pin connected to frequency counter
    pinMode(Bit24Word, INPUT);               // pin connected to frequency counter
    pinMode(Bit32Word, INPUT);               // pin connected to frequency counter
  #endif
  #ifdef oledPresent
    pinMode(oledReset, OUTPUT);              // set reset of oled screen
    digitalWrite(oledReset, LOW);       // keep the Oled screen in reset mode
    Wire.begin();  // start i2c communication
    delay(100);
    oledSchermInit();  // intialize the Oled screen
  #endif
  changeOnOff();
}

void loop() { //{  // Main loop
  if (dacAlive)  {                                              // we only react if we are in alive state, not in standby
    if (digitalRead(buttonInputChannel) == LOW) {  // if button channel selectd is pushed
      changeInputChannel();                        // change input channel
      delay(500);                                  // wait to prevent multiple switches
      measureFreq = false;
    }
    #ifdef freqcounter   
      if ((millis() - timeStartCountTicks) >= 1000) { // if a 1 second has passed
        if (measureFreq = true) {
          determineFreqAndBitDepth();
        }
        else {
          measureFreq = true;
          ticker = 0;                                 // reset ticker
          timeStartCountTicks = millis();  
        }
      }
      if ((millis() - timeUpdateScreen) >= 2000) { // if  2 seconds have passed update the screen
        measureFreq = false;  
        writeValuesScreen(); 
      }    
    #endif
  } 
  // if (digitalRead(powerOnOff) == LOW) {  // if button on / standby switch is pushed
  //     changeOnOff();                     // turn dac ON or standby
  //     delay(500);                        // wait to prevent multiple switches
  //     measureFreq = false;
  //}
} 