#define OLED_Address 0x3C       // 3C is address used by oled controler
#define OLED_Command_Mode 0x80  // Control byte indicates next byte will be a command
#define OLED_Data_Mode 0x40     // Control byte indicates next byte will be data

#include <Wire.h>                                   // pin 18 (SDA), 19 (SCL), SSD1311 PMOLED Controller
#include "oledScreen.h"
#include <Arduino.h>                                // needed for basis stuff

const uint8_t ContrastLevel=0x3f;                   // level of contrast, between 00 anf ff, default is 7f

const uint8_t Bitmaps[8][8] = {                     // array of bitmaps for large figures, copied and adapted from jos van eindhoven
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

const uint8_t Cijfers [10][6] = {                    // definitions of 0 till 9 used on the oled screen, stored in ram
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

void sendDataOled(unsigned char data)             // send data to the display
{
  Wire.beginTransmission(OLED_Address);
  Wire.write(OLED_Data_Mode);
  Wire.write(data);
  Wire.endTransmission();
}

void sendCommandOled(unsigned char command)       // send a command to the display
{
  Wire.beginTransmission(OLED_Address);
  Wire.write(OLED_Command_Mode);
  Wire.write(command);
  Wire.endTransmission();
}
void StoreLargecijfer()                          // function to store bitmaps for large volume into CGRAM
{
  sendCommandOled(0x40);                         // set CGRAM first address
  for (int i=0; i<8; i++){                       // walk through the array bitmaps, we have 8 bitmaps
      for (int c=0; c<8; c++){                   // every bitmap consists out of 8 lines
        sendDataOled(Bitmaps[i][c]);             // sent data to cgram
      }
  }
}

void Oled_Scherm_Init()                          // intialistion of the oled screen after powerdown of screen.
{
  delay(100);                                    // let SSD1311 finish internal POR
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



void writeVolume(int volume)                     // write volume level to screen
{
  volume = volume - Voloffset;                 // change volumelevel from 0 - 64 to (0 - offset)- to (64 - offset)
  sendCommandOled(0xC0);                       // place cursor second row first character
  if (volume < 0) {                              // if volumlevel  is negative
    sendDataOled(1);                           // minus sign
  }
  else {
    sendDataOled(32);                          // no minus sign
  }
  unsigned int lastdigit=abs(volume%10);
  unsigned int firstdigit=abs(volume/10);      // below needs to be rewritten to be more efficient
  sendCommandOled(0x81);
  for (int c=0; c<3; c++){               // every bitmap consists out of 8 lines
    sendDataOled(Cijfers[firstdigit][c]);  
  }
  for (int c=0; c<3; c++){               // every bitmap consists out of 8 lines
    sendDataOled(Cijfers[lastdigit][c]);  
  }
  sendCommandOled(0xC1);
  for (int c=3; c<6; c++){               // every bitmap consists out of 8 lines
    sendDataOled(Cijfers[firstdigit][c]);  
  }
  for (int c=3; c<6; c++){               // every bitmap consists out of 8 lines
  sendDataOled(Cijfers[lastdigit][c]);  
  }
}


void writeOLEDtopright(const char *OLED_string)   // write selected input channel to screen
{
  sendCommandOled(0x89); // set cursor in the middle
  unsigned char i = 0;                             // index 0
  while (OLED_string[i])                           // zolang er data is
  {
    sendDataOled(OLED_string[i]);                  // send characters in string to OLED, 1 by 1
    i++;
  }
}

void writeOLEDbottemright(const char *OLED_string)   // write selected input channel to screen
{
  sendCommandOled(0xC9); // set cursor in the middle
  unsigned char i = 0;                             // index 0
  while (OLED_string[i])                           // zolang er data is
  {
    sendDataOled(OLED_string[i]);                  // send characters in string to OLED, 1 by 1
    i++;
  }
}