#include "dis65c02.h"

#define RESB_PIN 22
#define CLK_PIN 23
#define BE_PIN 24
#define RWB_PIN 25

#define VPB_PIN 26
#define IRQB_PIN 27
#define NMIB_PIN 28
#define SYNC_PIN 29

#define A0_PIN 30
#define D0_PIN 46

#define DIR_INPUT INPUT
#define DIR_OUTPUT OUTPUT

struct PinState {
  byte pin;
  bool dir;
  byte curState;
  byte activeState;
  char *cmd;
  char *text;
};

PinState pinState[] = {
  {RESB_PIN, DIR_OUTPUT,   0,   0,    "RESB", "Reset"},
  {CLK_PIN,  DIR_OUTPUT,   0,   2,    "CLK",  "Clock"},
  {BE_PIN,   DIR_OUTPUT,   1,   0,    "BE",   "Bus Enable"},
  {RWB_PIN,  DIR_INPUT,    0,   0,    NULL,   "Write"},
  
  {VPB_PIN,  DIR_INPUT,    0,   0,    NULL,   "VectorPull"},
  {IRQB_PIN, DIR_OUTPUT,   1,   0,    "IRQ",  "IRQ"},
  {NMIB_PIN, DIR_OUTPUT,   1,   0,    "NMI",  "NMI"},
  {SYNC_PIN, DIR_INPUT,    0,   1,    "SYNC", "Sync"}
};

const PinState* psRESB = &pinState[0];
const PinState* psRWB = &pinState[3];
const PinState* psVPB = &pinState[4];
const PinState* psSYNC = &pinState[7];

#define SERIAL_TERMINATOR '\n'
#define COMMAND_SEPARATOR ';'
#define COMMAND_VALUE_SEPARATOR '='

char serBuf[128];
uint16_t serBufIdx = 0;

char outText[256];

// the setup function runs once when you press reset or power the board
void setup() {
  Serial.begin(115200);
  for (uint16_t i = 0; i < 16; i++) {
    pinMode(A0_PIN + i, INPUT);
  }

  for (uint16_t i = 0; i < 8; i++) {
    pinMode(D0_PIN + i, INPUT);
  }

  for (uint16_t i = 0; i < 8; i++)
  {
    PinState& pin = pinState[i];
    pinMode(pin.pin, pin.dir);
    if (pin.dir == DIR_OUTPUT) {
      digitalWrite(pin.pin, pin.curState & 0x1);
    }
  }

  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.println("\n\nAnalyze65 start");
}

short mclk = 0;
byte clk_phase = 0;

unsigned short address = 0;
byte data = 0;

bool isPinActive(PinState* pin)
{
  pin->curState = digitalRead(pin->pin);
  return pin->curState == pin->activeState;
}

void readBus()
{
  address = 0;
  data = 0;
  
  for (uint16_t i = 0; i < 16; i++)
  {
    address |= digitalRead(A0_PIN+i) << i;
  }
  for (uint16_t i = 0; i < 8; i++)
  {
    data |= digitalRead(D0_PIN+i) << i;
  }

  for (unsigned short i = 0; i < 8; i++)
  {
    PinState& pin = pinState[i];
    byte curPinState = digitalRead(pin.pin);
    pin.curState = curPinState;
  }
}

void showDrivenState()
{
  // Display which pins are being driven to their active state
  for (unsigned short i = 0; i < 8; i++)
  {
    PinState& pin = pinState[i];
    if (pin.dir == OUTPUT)
    {
      bool isPinActive = pin.curState == pin.activeState;
      if (isPinActive)
      {
        sprintf(outText," *%s", pin.text);
        Serial.print(outText);
      }
    }
  }
}

void showBus(bool forceAll = false)
{
  readBus();
  sprintf(outText, ". $(%04x) : %02x", address, data);
  Serial.print(outText);

  for (unsigned short i = 0; i < 8; i++)
  {
    PinState& pin = pinState[i];
    if (pin.dir == INPUT)
    {
      bool isPinActive = pin.curState == pin.activeState;
      if (forceAll || isPinActive)
      {
        char indicator = forceAll ? (isPinActive ? ' ' : '-') : ' ';
        sprintf(outText,", %c%s", indicator, pin.text);
        Serial.print(outText);
      }
    }
  }
  
  if (forceAll)
  {
    sprintf(outText,", %cClock", clk_phase ? '+' : '-');
    Serial.print(outText);
  }

  Serial.print("   ");
  showDrivenState();

  Serial.println();
}

dismnm* curOpcode = nullptr;
byte opcodeArgBytesRemaining = 0;
unsigned short opcodeArgValue = 0;
byte busLogLevel = 0;
char *spaces = "                                                  ";

// the loop function runs over and over again forever
void loop() {
  if (mclk > 0)
  {
    clk_phase = (clk_phase & 1)^1; // Toggle bit 0, clear all others
    digitalWrite(CLK_PIN, clk_phase & 1);

    if (clk_phase == 0 && isPinActive(psSYNC) && !isPinActive(psRESB))
    {
      // Instruction fetch start
      readBus();
      Serial.print(spaces);
      sprintf(outText,"* Sync $%04x", address);
      Serial.println(outText);
    }

    bool isWritePhase = (psRWB->curState == 0) && (clk_phase == 1);
    bool isReadPhase = (psRWB->curState == 1) && (clk_phase == 0);

    if  (busLogLevel > 0)
    {
      // Force detailed bus logging every clock phase
      showBus(true);
    }
    // The data for a write cycle only shows up during high clk phase,
    // all others the data we want is available before the posedge of clk.
    else if (isWritePhase || isReadPhase)
    {
      showBus();
    }

    if (isReadPhase)
    {
      if (psSYNC->curState == 1)
      {
        // Opcode start
        curOpcode = &a65C02_ops[data];
        opcodeArgBytesRemaining = a65C02_ops[data].arg_size;
        opcodeArgValue = 0;
      }
      else if (curOpcode != nullptr)
      {
        // Opcode argument
        opcodeArgValue = (opcodeArgValue << 8) | data;
        opcodeArgBytesRemaining--;
      }
    }
        
    if (clk_phase == 1 && curOpcode != nullptr && opcodeArgBytesRemaining == 0)
    {
      // Opcode complete, print disassembled instruction
      if (curOpcode->arg_size == 2)
      {
        opcodeArgValue = (opcodeArgValue >> 8) | (opcodeArgValue << 8);
      }
      
      sprintf(outText, aAddrModeFmt[curOpcode->addrMode], zsMNM[curOpcode->mnemonic], opcodeArgValue);
      Serial.print(spaces);
      Serial.print("  -> ");
      Serial.print(outText);
      Serial.println();

      curOpcode = nullptr;
    }

    mclk--;
  }
  
  while (Serial.available() > 0)
  {
    char serNext = Serial.read();
    if ( serNext != SERIAL_TERMINATOR && serBufIdx < sizeof(serBuf)-1)
    {
      serBuf[serBufIdx++] = serNext;
    }
    else
    {
      serBuf[serBufIdx] = '\0';
      processCommandBuffer(serBuf, serBufIdx);
      serBufIdx = 0;
    }
  }
  delay(1);
}

void processCommand(char *cmd, char *szValue)
{
  for (uint16_t i = 0; i < 8; i++)
  {
    PinState& pin = pinState[i];
    if (pin.dir == DIR_OUTPUT && !strcasecmp(pin.cmd,cmd))
    {
      byte v = pin.activeState;
      if (szValue)
      {
        v = atoi(szValue);
      }
      v &= 0x1;
      digitalWrite(pin.pin, v);
      pin.curState = v;
      sprintf(outText, "$ %s set to %d", pin.text, v);
      Serial.println(outText);
      return;
    }
  }

  if (!strcasecmp("step", cmd))
  {
    uint16_t count;
    if (!szValue)
    {
      count = 2;
    }
    else
    {
      count = 2 * atoi(szValue);
    }
    mclk = count-clk_phase;
    return;
  }

  if (!strcasecmp("tick", cmd))
  {
    mclk = 1;
    return;
  }

  if (!strcasecmp("showbus", cmd))
  {
    showBus(true);
    return;
  }

  if (!strcasecmp("bus.log", cmd))
  {
    int level = 0;
    if (szValue)
    {
      level = atoi(szValue);
    }
    sprintf(outText, "$ bus logging level set to %s", level == 0 ? "default" : "LOG ALL THE THINGS!");
    Serial.println(outText);
    busLogLevel = level;
    return;
  }

  Serial.print("! Unrecognized command: ");
  Serial.println(cmd);
}

void processCommandBuffer(char *buf, int sz)
{
  uint16_t cmdStart = 0;
  uint16_t valueStart = 0;
  uint16_t cmdEnd = 0;
  Serial.print("Analyze65> ");
  Serial.println(buf);
  while ( cmdEnd <= sz )
  {
    if( buf[cmdEnd] == COMMAND_VALUE_SEPARATOR )
    {
      buf[cmdEnd] = '\0';
      valueStart = cmdEnd+1;
    }
    else if ( buf[cmdEnd] == COMMAND_SEPARATOR || buf[cmdEnd] == '\0' )
    {
      buf[cmdEnd] = '\0';
      if ( buf[cmdStart] != '\0' )
      {
        char *value = valueStart == cmdStart ? NULL : &buf[valueStart];
        processCommand(&buf[cmdStart], value);
        
        cmdStart = cmdEnd+1;
        valueStart = cmdEnd+1;
      }
    }
    cmdEnd++;
  }
}
