#include <assert.h>

#define CLK_PHASE_MICROS 40 // Target time for one full clock cycle

//#define DEBUG
#define REPORTING_RATE_LIMIT_MS 1

#define PERR_NO_STARTBIT 0x50
#define PERR_PARITY_MISMATCH 0x77
#define PERR_NO_STOPBIT 0x05
#define PERR_BUFFER_OVERRUN 0xFF
#define PERR_TIMEOUT 0xA1

#define KEYBOARD_CLK_READ_PIN 2  // Must be pin 2 or 3 on ATMega328P!
#define KEYBOARD_DAT_READ_PIN 7

#define HOST_CLK_READ_PIN 3 // Must be pin 2 or 3 on ATMega328P!
#define HOST_DAT_READ_PIN 4
#define HOST_CLK_DRIVE_PIN 5
#define HOST_DAT_DRIVE_PIN 6

#define PULLDOWN_HOST_CLK digitalWrite(HOST_CLK_DRIVE_PIN, 1)
#define RELEASE_HOST_CLK digitalWrite(HOST_CLK_DRIVE_PIN, 0)
#define PULLDOWN_HOST_DAT digitalWrite(HOST_DAT_DRIVE_PIN, 1)
#define RELEASE_HOST_DAT digitalWrite(HOST_DAT_DRIVE_PIN, 0)

#define DRIVE_HOST_CLK(b) ((b == 0) ? PULLDOWN_HOST_CLK : RELEASE_HOST_CLK)
#define DRIVE_HOST_DAT(b) ((b == 0) ? PULLDOWN_HOST_DAT : RELEASE_HOST_DAT)

#define KEYBOARD_DAT digitalRead(KEYBOARD_DAT_READ_PIN)

#define HOST_CLK digitalRead(HOST_CLK_READ_PIN)
#define HOST_DAT digitalRead(HOST_DAT_READ_PIN)

volatile byte keycode[32];
volatile byte head = 0;
volatile byte tail = 0;

volatile uint16_t protocolError = 0;
volatile byte curCode;
volatile byte inbits = 0;
volatile byte errbits = 0;

#define SCANCODE_TIMEOUT_MILLIS 150
// We should always enter here on the falling edge of KEYBOARD_CLK_PIN
void ps2int(void)
{
  static uint32_t lastMillis = 0;
  static byte parity = 0;
  
  uint32_t curMillis = millis();
  if (curMillis >= (lastMillis + SCANCODE_TIMEOUT_MILLIS))
  {
    // Haven't heard from you in a while, let's start again.
    curCode = 0;
    parity = 0;
    inbits = 0;
  }
  lastMillis = curMillis;
  
  byte curBit = KEYBOARD_DAT;
  switch(inbits)
  {
    case 0:
      // Start bit
      if (curBit == 0)
      {
        inbits++;
      } else
      {
        protocolError = PERR_NO_STARTBIT;
        errbits = inbits;
      }
      break;
      
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
      // Data bit, LSb first
      if (curBit) curCode |= 1 << (inbits-1);
      parity += curBit;
      inbits++;
      break;

    case 9:
      // parity bit
      parity += curBit;
      if ((parity & 0x1) != 1)
      {
        protocolError = PERR_PARITY_MISMATCH;
        errbits = inbits;
      }
      inbits++;
      break;
      
    case 10:
    {
      // stop bit
      if (curBit != 1)
      {
        protocolError = PERR_NO_STOPBIT;
        errbits = inbits;
      }
      else if ((parity & 0x1) == 1) // Parity good
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
          // Ring buffer overrun, drop the incoming code :(
          protocolError = PERR_BUFFER_OVERRUN;
          errbits = inbits;
        }
      }
      inbits = 0;
      parity = 0;
      curCode = 0;
    }
    break;
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(KEYBOARD_CLK_READ_PIN, INPUT_PULLUP);
  pinMode(KEYBOARD_DAT_READ_PIN, INPUT_PULLUP);
  pinMode(HOST_CLK_READ_PIN, INPUT_PULLUP);
  pinMode(HOST_DAT_READ_PIN, INPUT_PULLUP);
  pinMode(HOST_CLK_DRIVE_PIN, OUTPUT);
  pinMode(HOST_DAT_DRIVE_PIN, OUTPUT);
    
  delay(100);
#ifdef DEBUG
  Serial.begin(115200);
  Serial.println("PS2Keyboard Buffer Start");
#endif
  attachInterrupt(digitalPinToInterrupt(KEYBOARD_CLK_READ_PIN), ps2int, FALLING);
  RELEASE_HOST_CLK;
  RELEASE_HOST_DAT;
}

bool checkAbort()
{
  if (HOST_CLK == 0)
  {
    // Release control of DAT/CLK
    RELEASE_HOST_DAT;
    RELEASE_HOST_CLK;
    return true;
  }
  return false;
}

bool clockBit(byte b)
{
  // We come in here at the beginning of the high clock phase

  if (checkAbort()) return true;  // Check for abort before we do anything

  delayMicroseconds(CLK_PHASE_MICROS/4);
  DRIVE_HOST_DAT(b);
  delayMicroseconds(CLK_PHASE_MICROS/4);
  
  if (checkAbort()) return true;  // Check for abort 

  DRIVE_HOST_CLK(0);
  delayMicroseconds(CLK_PHASE_MICROS/2);
  DRIVE_HOST_CLK(1);
  
  if (checkAbort()) return true;
  return false;
}

bool clockOut(byte outByte) {
  byte parity = 1;
  byte nbits = 0;
  bool resetScan = false;

  if ( checkAbort() ) // Host is holding the clock
  {
    return true;
  }
  
  for (nbits = 0; (nbits < 11) && (resetScan == false); nbits++)
  {
    switch(nbits)
    {
      case 0:
        // Start bit
        resetScan = clockBit(0);
        break;
      case 1:
      case 2:
      case 3:
      case 4:
      case 5:
      case 6:
      case 7:
      case 8:
        // data bits
        resetScan = clockBit(outByte & 0x1);
        parity += outByte & 0x1;
        outByte >>= 1;
        break;
      case 9:
        // Parity bit
        resetScan = clockBit(parity & 0x1);
        break;
      case 10:
        // Stop bit
        resetScan = clockBit(1);
        break;
    }
  }
#ifdef DEBUG
  assert((HOST_CLK == 1) || resetScan);
#endif
  return resetScan;
}

void loop() {
  static uint16_t lastProtocolError = 0;
  static uint16_t lastProtocolErrorCount = 0;
  static uint32_t lastReportTime = 0;
  static byte lastHead = 0;
  // put your main code here, to run repeatedly:

  if (head != tail)
  {
    byte next = keycode[tail];
  
    if (!clockOut(next))
    {
      // advance tail
      tail = (tail + 1) & 0x1f;
  #ifdef DEBUG
      Serial.print("Sent : 0x");
      Serial.print(next,HEX);
      Serial.print(", Head : ");
      Serial.print(head);
      Serial.print(", Tail: ");
      Serial.print(tail);
      Serial.println();
  #endif
    }
  }
}
