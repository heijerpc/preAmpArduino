#ifndef oledScreen_H
#define oledScreen_H

#include <Wire.h>               // pin 18 (SDA), 19 (SCL), SSD1311 PMOLED Controller
#include <Arduino.h>

const uint8_t ContrastLevel=0x3f;  // level of contrast, between 00 anf ff, default is 7f
const uint8_t Voloffset=0;         // diffence between volumelevel and volumelevel displayed on the screen

void OledSchermInit();
void setVolumeOled(int volume);                      // write volume level to screen
void writeOLEDtopright(const char *OLED_string);     // write selected input channel to screen
void writeOLEDbottemright(const char *OLED_string);  // write selected input channel to screen
#endif
