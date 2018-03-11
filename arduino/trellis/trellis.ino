/*************************************************** 
  This is a test example for the Adafruit Trellis w/HT16K33

  Designed specifically to work with the Adafruit Trellis 
  ----> https://www.adafruit.com/products/1616
  ----> https://www.adafruit.com/products/1611

  These displays use I2C to communicate, 2 pins are required to  
  interface
  Adafruit invests time and resources providing this open source code, 
  please support Adafruit and open-source hardware by purchasing 
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.  
  MIT license, all text above must be included in any redistribution
 ****************************************************/

#include <Wire.h>
#include "Adafruit_Trellis.h"

/*************************************************** 
  This example shows reading buttons and setting/clearing buttons in a loop
  "momentary" mode has the LED light up only when a button is pressed
  "latching" mode lets you turn the LED on/off when pressed

  Up to 8 matrices can be used but this example will show 4 or 1
 ****************************************************/

Adafruit_Trellis matrix0 = Adafruit_Trellis();
Adafruit_Trellis matrix1 = Adafruit_Trellis();
Adafruit_Trellis matrix2 = Adafruit_Trellis();
Adafruit_Trellis matrix3 = Adafruit_Trellis();

// uncomment the below to add 3 more matrices
/*
// you can add another 4, up to 8
*/

// Just one
Adafruit_TrellisSet trellis =  Adafruit_TrellisSet(&matrix0, &matrix1, &matrix2, &matrix3);
// or use the below to select 4, up to 8 can be passed in
//Adafruit_TrellisSet trellis =  Adafruit_TrellisSet(&matrix0, &matrix1, &matrix2, &matrix3);

// set to however many you're working with here, up to 8
#define NUMTRELLIS 4

#define numKeys (NUMTRELLIS * 16)

// Connect Trellis Vin to 5V and Ground to ground.
// Connect the INT wire to pin #A2 (can change later!)
#define INTPIN A2
// Connect I2C SDA pin to your Arduino SDA line
// Connect I2C SCL pin to your Arduino SCL line
// All Trellises share the SDA, SCL and INT pin! 
// Even 8 tiles use only 3 wires max

char chunkToByte(uint8_t offset)
{
  uint8_t data = 0;

  for (int i = 0; i < 8; ++i)
  {
    data <<= 1;
    data |= trellis.isLED(offset + i);
  }

  return data;
}

void sendGrid()
{
  for (uint8_t i = 0; i < numKeys; i += 8)
  {
    char data = chunkToByte(i);
    Serial.print(data);
  } 

  //Serial.print('\n');
}

void byteToChunk(uint8_t data, int offset)
{
  for (int i = 0; i < 8; ++i)
  {
    bool value = data & 0x1;
    
    if (value) trellis.setLED(offset + i);
    else       trellis.clrLED(offset + i);
    
    data >>= 1;
  }
}

void readGrid()
{
  if (Serial.available() < 8) return;

  for (uint8_t i = 0; i < numKeys; i += 8)
  {
    uint8_t data = Serial.read();
    byteToChunk(data, i);
  } 

  trellis.writeDisplay();   
}

void setup() {
  Serial.begin(9600);
  Serial.println("Trellis Demo");

  pinMode(13, INPUT);
  pinMode(12, INPUT);

  // INT pin requires a pullup
  pinMode(INTPIN, INPUT);
  digitalWrite(INTPIN, HIGH);
  
  // begin() with the addresses of each panel in order
  // I find it easiest if the addresses are in order
  trellis.begin(0x70, 0x71, 0x72, 0x73);
  trellis.setBrightness(1);

  // light up all the LEDs in order
  for (uint8_t i = 0; i < numKeys; i++) 
  {
    if ((i + (i / 4) % 2) % 2 == 0) trellis.setLED(i);
  }
  trellis.writeDisplay();
  delay(100);
  for (uint8_t i = 0; i < numKeys; i++) 
  {
    if ((i + (i / 4) % 2) % 2 == 1) trellis.setLED(i);
    else trellis.clrLED(i);
  }
  trellis.writeDisplay();
  delay(100);
  
  // then turn them off
  for (uint8_t i=0; i<numKeys; i++) {
    trellis.clrLED(i);
  }
  trellis.writeDisplay();
}

bool down1 = false;
bool down2 = false;

bool buffer[numKeys];

void loop() {
  delay(30); // 30ms delay is required, dont remove me!
  
  readGrid();

  // If a button was just pressed or released...
  if (trellis.readSwitches()) 
  {
    // go through every button
    for (uint8_t i=0; i<numKeys; i++) 
    {
      // if it was pressed...
      if (trellis.justPressed(i)) 
      {
        // Alternate the LED
        if (trellis.isLED(i))
          trellis.clrLED(i);
        else
          trellis.setLED(i);
      } 
    }
    // tell the trellis to set the LEDs we requested
    trellis.writeDisplay();
    sendGrid();
  }

  bool held1 = digitalRead(13);
  
  if (held1 && !down1)
  {
    for (uint8_t i=0; i<numKeys; i++) 
    {
        if (trellis.isLED(i))
          trellis.clrLED(i);
        else
          trellis.setLED(i);
    }

    trellis.writeDisplay();
    sendGrid();
    down1 = true;
  }
  else if (!held1)
  {
    down1 = false;
  }

  bool held2 = digitalRead(12);
  
  if (held2 && !down2)
  {
    for (uint8_t i=0; i<numKeys; i++) 
    {
      buffer[i] = trellis.isLED(numKeys - i - 1);
    }
    
    for (uint8_t i=0; i<numKeys; i++) 
    {
        if (buffer[i])
          trellis.setLED(i);
        else
          trellis.clrLED(i);
    }

    trellis.writeDisplay();
    sendGrid();
    down2 = true;
  }
  else if (!held2)
  {
    down2 = false;
  }
}
