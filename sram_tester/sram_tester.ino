/*

AS6C62256

Ard                          Ard
Pin   Func            Func   Pin
           -----v-----
22    A14  | 1    28 | Vcc
24    A12  | 2    27 | WE#    23
26     A7  | 3    26 | A13    25
28     A6  | 4    25 |  A8    27
30     A5  | 5    24 |  A9    29
32     A4  | 6    23 | A11    31
34     A3  | 7    22 | OE#    33
36     A2  | 8    21 | A10    35
38     A1  | 9    20 | CE#    37
40     A0  | 10   19 | DQ7    39
42    DQ0  | 11   18 | DQ6    41
44    DQ1  | 12   17 | DQ5    43
46    DQ2  | 13   16 | DQ4    45
      Vss  | 14   15 | DQ3    47
           -----------
         
 */

                    //  A0  A1  A2  A3  A4  A5  A6  A7  A8  A9 A10 A11 A12 A13 A14
byte addrPinMap[15] = { 40, 38, 36, 34, 32, 30, 28, 26, 27, 29, 35, 31, 24, 25, 22 };

                    // DQ0 DQ1 DQ2 DQ3 DQ4 DQ5 DQ6 DQ7
byte dqPinMap[8]    = { 42, 44, 46, 47, 45, 43, 41, 39 };

#define WE_PIN 23
#define OE_PIN 33
#define CE_PIN 37

enum DataBusDirection : uint8_t {
  BUS_READ,
  BUS_WRITE
};

void setAddr(uint16_t addr) {
  for(uint16_t i=0; i < 15; i++) {
    digitalWrite(addrPinMap[i], addr & 0x1);
    addr >>= 1;
  }
}

void setDataBusDirection(DataBusDirection dir) {
  for(uint16_t i=0; i < 8; i++) {
    pinMode(dqPinMap[i], dir == BUS_READ ? INPUT : OUTPUT);
  }
}

void setDQ(uint8_t data) {
  for(uint16_t i=0; i < 8; i++) {
    digitalWrite(dqPinMap[i], data & 0x1);
    data >>= 1;
  }
}

uint8_t readDQ() {
  uint8_t data = 0;
  for(int i=7; i >= 0; --i) {
    uint8_t d = digitalRead(dqPinMap[i]);
    data <<= 1;
    data |= d;
  }
  return data;
}

void deassertPin(uint8_t pin) {
  // Active low pins are deasserted when driven high
  digitalWrite(pin, 1);
}

void assertPin(uint8_t pin) {
  // Active low pins are asserted when driven low
  digitalWrite(pin, 0);
}

void sramWrite(uint16_t addr, uint8_t data, bool configureBus=false) {
  if (configureBus) {
    // Make sure SRAM isn't driving bus
    deassertPin(OE_PIN);
    
    // Set Arduino to drive bus
    setDataBusDirection(BUS_WRITE);
  }

  // Put address on bus
  setAddr(addr);

  // Enable SRAM
  assertPin(CE_PIN);

  // Put data on bus
  setDQ(data);

  // Strobe WE#
  assertPin(WE_PIN);
  deassertPin(WE_PIN);

  // Disable SRAM
  deassertPin(CE_PIN);
}

uint8_t sramRead(uint16_t addr, bool configureBus=false) {
  if (configureBus) {
    // Make sure SRAM isn't sampling bus
    deassertPin(WE_PIN);
    
    // Set Arduino to drive bus
    setDataBusDirection(BUS_READ);
  }

  // Put address on bus
  setAddr(addr);

  // Enable SRAM
  assertPin(CE_PIN);

  // Read cycle
  assertPin(OE_PIN);

  // Sample pins
  uint8_t dq = readDQ();

  // Disable SRAM
  deassertPin(OE_PIN);
  deassertPin(CE_PIN);
  
  return dq;
}

#define CRC8_POLYNOMIAL 0xA7

inline uint8_t advanceCRC(uint8_t d) {
  uint16_t d1 = d;
  d1 <<= 1;
  if (d1 > 255) d1 ^= CRC8_POLYNOMIAL;
  return (uint8_t) d1;
}

uint8_t fillPage(uint8_t page, uint8_t crc) {
  uint16_t page_address = ((uint16_t)page) << 8;
  for(uint16_t i = 0; i < 256; i++) {
    sramWrite(page_address + i, crc);
    crc = advanceCRC(crc);
  }
  return crc;
}

bool checkPage(uint8_t page, uint8_t crc) {
  uint16_t page_address = ((uint16_t)page) << 8;
  for(uint16_t i = 0; i < 256; i++) {
    uint8_t data = sramRead(page_address + i);
    if (data != crc) {
      Serial.print("Error at address 0x");
      Serial.print(page_address + i, HEX);
      Serial.print(" (saw:0x");
      Serial.print(data,HEX);
      Serial.print(", expected:0x");
      Serial.print(crc,HEX);
      Serial.print(") ");
      return false;
    }
    crc = advanceCRC(crc);
  }
  return true;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  Serial.println("SRAM Tester Start.");
  setDataBusDirection(BUS_READ);

  for(uint16_t i=0; i < 15; i++) {
    pinMode(addrPinMap[i], OUTPUT);
  }
  pinMode(WE_PIN, OUTPUT);
  deassertPin(WE_PIN);
  pinMode(OE_PIN, OUTPUT);
  deassertPin(OE_PIN);
  pinMode(CE_PIN, OUTPUT);
  deassertPin(CE_PIN);
}

void loop() {
  static uint8_t page = 0;
  static uint8_t crc = CRC8_POLYNOMIAL;

  // put your main code here, to run repeatedly:
  setDataBusDirection(BUS_WRITE);
  Serial.print("Filling page ");
  Serial.print(page);
  Serial.print("... ");
  uint8_t next_page_crc = fillPage(page, crc);

  setDataBusDirection(BUS_READ);
  delay(10);
  Serial.print("Checking page ");
  Serial.print(page);
  Serial.print("... ");
  bool succeeded = checkPage(page,crc);
  if ( succeeded ) Serial.print("OK");
  Serial.println();

  crc = next_page_crc;
  page = (page + 1) & 0x7f;
  delay(20);
}
