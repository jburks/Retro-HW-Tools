#include "PS2Keyboard.h"

#define KEYBOARD_CLK_PIN 3 // Must be pin 2 or 3 on ATMega328P!
#define KEYBOARD_DAT_PIN 5
#define HOST_CLK_PIN 2
#define HOST_DAT_PIN 4

#define CLK_PHASE_MICROS 80 // Target time for one full clock cycle

int ledStat = 0;
byte keycode[32];
volatile byte head = 0;
volatile byte tail = 0;

volatile uint32_t protocolErrors = 0;

void ps2int(void)
{
  static uint16_t curCode;
  static byte nbits = 0;
  static byte parity = 0;
  byte curBit = digitalRead(KEYBOARD_DAT_PIN);
  if (nbits == 0)
  {
    // Start bit
    if (curBit != 0) protocolErrors++;
    nbits++;
  }
  else if (nbits >= 1 && nbits <= 8)
  {
    curCode |= curBit << 8;
    curCode >>= 1;
    parity += curBit;
    nbits++;
  }
  else if (nbits == 9)
  {
    // parity bit
    parity += curBit;
    if ((parity & 0x1) != 1) protocolErrors++;
    nbits++;
  }
  else
  {
    // stop bit
    if (curBit != 1) protocolErrors++;
    if ((parity & 0x1) != 1)
    {
      // Parity error; should send a "resend" cmd
      // We'll just toggle the builtin LED
      ledStat = (ledStat & 1) ^ 1;
      digitalWrite(LED_BUILTIN, ledStat);
    }
    else
    {
      byte headNext = (head+1) & 0x1f;
      if (headNext != tail)
      {
        // Ring buffer not full
        keycode[head] = (byte)(curCode);
        head = headNext;
      }
      else
      {
        protocolErrors = (protocolErrors & 0xff) | (head << 8) | (((uint32_t)tail) << 16);
      }
    }
    nbits = 0;
    parity = 0;
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(KEYBOARD_CLK_PIN, INPUT_PULLUP);
  pinMode(KEYBOARD_DAT_PIN, INPUT_PULLUP);
  pinMode(HOST_CLK_PIN, INPUT_PULLUP);
  pinMode(HOST_DAT_PIN, OUTPUT);
  
  delay(250);
#ifdef DEBUG
  Serial.begin(115200);
  Serial.println("PS2Keyboard Buffer Start");
#endif
  attachInterrupt(digitalPinToInterrupt(KEYBOARD_CLK_PIN), ps2int, FALLING);
  digitalWrite(HOST_DAT_PIN, 1);
}

void clockBit(byte b)
{
  // We come in here half way through the high clock phase
  digitalWrite(HOST_DAT_PIN, b);
  delayMicroseconds(CLK_PHASE_MICROS/4);
  digitalWrite(HOST_CLK_PIN, 0);
  delayMicroseconds(CLK_PHASE_MICROS/2);
  digitalWrite(HOST_CLK_PIN, 1);
  delayMicroseconds(CLK_PHASE_MICROS/4);
}

void clockOut() {
  static byte nbits = 0;
  static byte parity = 0;
  static byte outByte = 0;
  byte clockState = digitalRead(HOST_CLK_PIN);

  if ( clockState == 0 ) // Host is holding the clock
  {
    nbits = 0;
    parity = 1;
  }
  else
  {
    pinMode(HOST_CLK_PIN, OUTPUT);
    if (nbits == 0)
    {
      // Start bit
      outByte = keycode[tail];
      clockBit(0);
      nbits++;
    }
    else if (nbits >= 1 && nbits <= 8)
    {
      // data bits
      clockBit(outByte & 0x1);
      parity += outByte & 0x1;
      outByte >>= 1;
      nbits++;
    }
    else if (nbits == 9)
    {
      // Parity bit
      clockBit(parity & 0x1);
      nbits++;
    }
    else if (nbits == 10)
    {
      // Stop bit
      clockBit(1);

      digitalWrite(HOST_CLK_PIN, 1);
      digitalWrite(HOST_DAT_PIN, 1);
      // all done, we should check that host didn't stop us at the last moment
      tail = (tail + 1) & 0x1f;
      nbits = 0;
      parity = 0;
    }
    pinMode(HOST_CLK_PIN, INPUT_PULLUP);
  }
}

void loop() {
  static int lastProtocolErrorCount = 0;
  static byte myTail = 0;
  // put your main code here, to run repeatedly:
  while (head != tail)
  {
    byte in = keycode[tail];
    clockOut();
  }
#ifdef DEBUG
  if (lastProtocolErrorCount != protocolErrors)
  {
    Serial.print("Protocol error count: ");
    Serial.println(protocolErrors,HEX);
    lastProtocolErrorCount = protocolErrors;
  }
#endif
}
