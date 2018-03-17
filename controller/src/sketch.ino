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

uint8_t buffer[16];
uint8_t bufferSize = 0;

struct Command
{
    typedef enum : uint8_t
    {
        DEBUG,
        SYNCGRID,
        BUTTONDOWN,
        BUTTONUP,
        DIALCHANGE,
    } Type;
};

void bufferDiscard(uint8_t count)
{
    for (uint8_t i = 0; i < count; ++i)
    {
        buffer[i] = buffer[i + count];
    }

    bufferSize -= count;
}

void sendDEBUG(String string)
{
    Serial.write(Command::DEBUG);
    Serial.write(string.length());
    Serial.print(string);
}

bool recvSYNCGRID()
{
    if (bufferSize < 9) return false;
    
    // skip type
    uint8_t reads = 1;

    for (uint8_t i = 0; i < numKeys; i += 8)
    {
        uint8_t data = buffer[reads];
        byteToChunk(data, i);

        reads += 1;
    } 

    trellis.writeDisplay();

    bufferDiscard(9);

    return true;
};

void sendSYNCGRID()
{
    Serial.write(Command::SYNCGRID);

    for (uint8_t i = 0; i < numKeys; i += 8)
    {
        char data = chunkToByte(i);
        Serial.write(data);
    } 
};

void sendBUTTONDOWN(uint8_t button)
{
    Serial.write(Command::BUTTONDOWN);
    Serial.write(button);
};

void sendBUTTONUP(uint8_t button)
{
    Serial.write(Command::BUTTONUP);
    Serial.write(button);
}

void sendDIALCHANGE(uint8_t dial, uint8_t value)
{
    Serial.write(Command::DIALCHANGE);
    Serial.write(dial);
    Serial.write(value);
}

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

void setup() {
  Serial.begin(115200);

  sendDEBUG("test mark");

  pinMode(13, INPUT);
  pinMode(12, INPUT); 
  pinMode( 8, INPUT);

  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);

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
bool down3 = false;
bool down4 = false;

bool buffer2[numKeys];

int dials[3];

bool executeBuffer()
{
    if (bufferSize == 0) return false;

    if (buffer[0] == Command::SYNCGRID)
    {
        return recvSYNCGRID();
    }

    return false;
}

static uint8_t enc_prev_pos   = 0;
static uint8_t enc_flags      = 0;

static int8_t enc_change = 0;
static uint32_t enc_timer = 0;

void read_encoder()
{
    uint32_t time = millis();

    bool update = time - enc_timer >= 100;

    if (enc_change != 0 && update)
    {
        enc_timer = time;
        sendDIALCHANGE(2, 128 + enc_change);
        enc_change = 0;
    }

    uint8_t enc_cur_pos = 0;
    // read in the encoder state first
    if (bit_is_clear(PIND, 3)) 
    {
        enc_cur_pos |= (1 << 0);
    }

    if (bit_is_clear(PIND, 2)) 
    {
        enc_cur_pos |= (1 << 1);
    }

    // if any rotation at all
    if (enc_cur_pos != enc_prev_pos)
    {
        if (enc_prev_pos == 0x00)
        {
            // this is the first edge
            if (enc_cur_pos == 0x01) 
            {
                enc_flags |= (1 << 0);
            }
            else if (enc_cur_pos == 0x02) 
            {
                enc_flags |= (1 << 1);
            }
        }

        if (enc_cur_pos == 0x03)
        {
            // this is when the encoder is in the middle of a "step"
            enc_flags |= (1 << 4);
        }
        else if (enc_cur_pos == 0x00)
        {
            // this is the final edge
            if (enc_prev_pos == 0x02) 
            {
                enc_flags |= (1 << 2);
            }
            else if (enc_prev_pos == 0x01) 
            {
                enc_flags |= (1 << 3);
            }

            // check the first and last edge
            // or maybe one edge is missing, if missing then require the middle state
            // this will reject bounces and false movements
            if (bit_is_set(enc_flags, 0) && (bit_is_set(enc_flags, 2) || bit_is_set(enc_flags, 4)))
             {
                enc_change += 1;
            }
            else if (bit_is_set(enc_flags, 2) && (bit_is_set(enc_flags, 0) || bit_is_set(enc_flags, 4))) 
            {
                enc_change += 1;
            }
            else if (bit_is_set(enc_flags, 1) && (bit_is_set(enc_flags, 3) || bit_is_set(enc_flags, 4))) 
            {
                enc_change -= 1;
            }
            else if (bit_is_set(enc_flags, 3) && (bit_is_set(enc_flags, 1) || bit_is_set(enc_flags, 4))) 
            {
                enc_change -= 1;
            }

            enc_flags = 0; // reset for next time
        }
    }

    enc_prev_pos = enc_cur_pos;
}

static uint32_t keypad_timer = 0;

void read_keypad()
{
    uint32_t time = millis();

    // can't do this more frequently than once per 30ms
    if (time - keypad_timer < 30) return;
    keypad_timer = time;

    //sendDEBUG("read keys");

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
        sendSYNCGRID();
    }
}

void invert()
{
    for (uint8_t i=0; i<numKeys; i++) 
    {
        if (trellis.isLED(i))
            trellis.clrLED(i);
        else
            trellis.setLED(i);
    }
}

void loop() 
{
    read_keypad();
    read_encoder();

    while (bufferSize < 16 
        && Serial.available() > 0)
    {
        buffer[bufferSize] = Serial.read();
        bufferSize += 1;
    }

    while (executeBuffer());

    bool held1 = digitalRead(13);

    if (held1 && !down1)
    {
        sendBUTTONDOWN(1);

        invert();

        trellis.writeDisplay();
        sendSYNCGRID();
        down1 = true;
    }
    else if (!held1 && down1)
    {
        down1 = false;
        sendBUTTONUP(1);
    }

    bool held2 = digitalRead(12);

    if (held2 && !down2)
    {
        sendBUTTONDOWN(2);
        down2 = true;
    }
    else if (!held2 && down2)
    {
        down2 = false;
        sendBUTTONUP(2);
    }

    bool held3 = digitalRead(8);

    if (held3 && !down3)
    {
        sendBUTTONDOWN(3);
        down3 = true;
    }
    else if (!held3 && down3)
    {
        down3 = false;
        sendBUTTONUP(3);
    }

    int d0 = map(analogRead(A0), 0, 1023, 0, 255);
    int d1 = map(analogRead(A1), 0, 1023, 0, 255);
    int d2 = map(analogRead(A2), 0, 1023, 0, 255);

    bool held4 = digitalRead(4) == LOW;

    if (held4 && !down4)
    {
        sendBUTTONDOWN(4);
        down4 = true;
    }
    else if (!held4 && down4)
    {
        down4 = false;
        sendBUTTONUP(4);
    }
}
