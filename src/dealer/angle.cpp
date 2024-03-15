// (c)2024, Arthur van Hoff, Artfahrt Inc.
#include <Wire.h>
#include "bus.h"
#include "angle.h"

extern BusMaster bus;

void AngleSensor::init()
{
    Wire.beginTransmission(0x36);
    int error = Wire.endTransmission();
    active = error == 0;
    if (!active) {
        dprintf("warning: angle sensor not found, error=%d", error);
        angle=360;
    }
}

//
// source: https://forum.arduino.cc/t/using-wire-library-to-read-16-bit-i2c-sensor/1003401
//
float AngleSensor::readAngle()
{ 
  if (true) {
    return -1;
  }
  unsigned char res[1];

  //7:0 - bits
  if (!bus.request(0x36, (const unsigned char[]){0x0D}, 1, res, sizeof(res))) {
    return 360;
  }
  int lowbyte = res[0];
 
  //11:8 - 4 bits
  if (!bus.request(0x36, (const unsigned char[]){0x0C}, 1, res, sizeof(res))) {
    return 360;
  }
  int highbyte = res[0];
  
  //4 bits have to be shifted to its proper place as we want to build a 12-bit number
  highbyte = highbyte << 8; //shifting to left
  //What is happening here is the following: The variable is being shifted by 8 bits to the left:
  //Initial value: 00000000|00001111 (word = 16 bits or 2 bytes)
  //Left shifting by eight bits: 00001111|00000000 so, the high byte is filled in
  
  //Finally, we combine (bitwise OR) the two numbers:
  //High: 00001111|00000000
  //Low:  00000000|00001111
  //      -----------------
  //H|L:  00001111|00001111
  int rawAngle = highbyte | lowbyte; //int is 16 bits (as well as the word)

  //We need to calculate the angle:
  //12 bit -> 4096 different levels: 360° is divided into 4096 equal parts:
  //360/4096 = 0.087890625
  //Multiply the output of the encoder with 0.087890625
  float degAngle = rawAngle * 0.087890625; 
  
  //Serial.print("Deg angle: ");
  //Serial.println(degAngle, 2); //absolute position of the encoder within the 0-360 circle
  return degAngle;
}
