#include <Wire.h>
#include "Adafruit_Trellis.h"

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

bool GRIDDATA[64];

struct Command
{
    typedef enum : uint8_t
    {
        DEBUG,
        SYNCGRID,
        BUTTONDOWN,
        BUTTONUP,
        DIALCHANGE,
        SET_PAD_TOGGLE,
        SET_PAD_HIGHLIGHT,
        PAD_DOWN,
        PAD_UP,
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
    data |= GRIDDATA[offset + i];
  }

  return data;
}

void byteToChunk(uint8_t data, int offset)
{
  for (int i = 0; i < 8; ++i)
  {
    bool value = data & 0x1;

    trellisSet(offset + i, value);

    data >>= 1;
  }
}

void setup() {
  Serial.begin(115200);

  sendDEBUG("test mark");

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

bool padModeToggle = true;

bool executeBuffer()
{
    if (bufferSize == 0) return false;

    if (buffer[0] == Command::SYNCGRID)
    {
        return recvSYNCGRID();
    }
    else if (buffer[0] == Command::SET_PAD_HIGHLIGHT)
    {
        padModeToggle = true;
        bufferDiscard(1);
        return true;
    }
    else if (buffer[0] == Command::SET_PAD_TOGGLE)
    {
        padModeToggle = false;
        bufferDiscard(1);
        return true;
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

void trellisSetTemp(uint8_t i, bool on)
{
    if (on) trellis.setLED(i);
    else    trellis.clrLED(i);
}

void trellisSet(uint8_t i, bool on)
{
    if (on) trellis.setLED(i);
    else    trellis.clrLED(i);

    GRIDDATA[i] = on;
}

void read_keypad()
{
    uint32_t time = millis();

    // can't do this more frequently than once per 30ms
    if (time - keypad_timer < 30) return;
    keypad_timer = time;

    //sendDEBUG("read keys");

    if (trellis.readSwitches()) 
    {
        for (uint8_t i = 0; i < numKeys; ++i) 
        { 
            if (padModeToggle)
            {
                if (trellis.justPressed(i))
                {
                    trellisSet(i, !GRIDDATA[i]);
                }
            }
            else
            {
                bool pressed = trellis.justPressed(i);
                bool released = trellis.justReleased(i);

                if (pressed)
                {
                    trellisSetTemp(i, !GRIDDATA[i]);
                    Serial.write(Command::PAD_DOWN);
                    Serial.write(i);
                }

                if (released)
                {
                    trellisSetTemp(i, GRIDDATA[i]);
                    Serial.write(Command::PAD_UP);
                    Serial.write(i);
                }
            }
        }

        if (padModeToggle) sendSYNCGRID();
        trellis.writeDisplay();
    }
}

static int keys[] = {0, 708, 718, 760, 773, 822, 837, 877, 894, 912, 930, 1000};
static int lastFrame;

static uint8_t panel_next = 0;
static uint8_t panel_prev = 0;

static uint32_t panel_timer = 0;

bool getPanelDown()
{
    uint32_t time = millis();

    // let's try to debounce...
    if (time - panel_timer < 30) return false;
    panel_timer = time;

    int test = analogRead(A0);
    uint8_t result = 0;

    for (int i = 0; i < 11; ++i)
    {
        int l = keys[i];
        int r = keys[i + 1];
         
        if (test >= l && test <= r)
        {
            int dl = test - l;
            int dr = r - test;

            if (dl <= 7 && dl < dr)
            {
                result = i;
            }
            else if (dr <= 7)
            {
                result = i + 1;
            }
        }
    }

    bool same = result == lastFrame;
    lastFrame = result;

    panel_next = result;
    return same;
}

void readPanel()
{
    if (getPanelDown())
    {
        if (panel_prev != panel_next)
        {
            if (panel_prev > 0) 
            {
                sendBUTTONUP(panel_prev);
            }

            if (panel_next > 0)
            {
                sendBUTTONDOWN(panel_next);
            }

            panel_prev = panel_next;
        }
    }

    /*
    if (next > 0 && next != panel_prev)
    {
        if (panel_prev > 0)
        {
            sendBUTTONUP(panel_prev);
        }

        sendBUTTONDOWN(next);
        panel_prev = next;
    }
    
    if (next != panel_prev && panel_prev > 0)
    {
        sendBUTTONUP(panel_prev);
        panel_prev = 0;
    }
    */
}

bool checkButton(uint8_t pin, bool &down, uint8_t id)
{
    bool held = digitalRead(pin) == LOW;

    if (held && !down)
    {
        down = true;
        sendBUTTONDOWN(id);
        return true;
    }
    else if (!held && down)
    {
        down = false;
        sendBUTTONUP(id);
        return true;
    }

    return false;
}

void loop() 
{
    read_keypad();
    read_encoder();
    readPanel();

    while (bufferSize < 16 
        && Serial.available() > 0)
    {
        buffer[bufferSize] = Serial.read();
        bufferSize += 1;
    }

    while (executeBuffer());
}
