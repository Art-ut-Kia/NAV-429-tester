//
// Arinc Shield Test.ino
//
// author: JPP
//
// Version V1.01 (15/03/2017) : ACLK divider is a parameter, to cope with both units:
//   - 12MHz (prototypes) or
//   - 16MHz (serial production)
//
// Version V1.02 (18/01/2018) : added a test step for RS422 loopback check
//
// Version V1.03 (14/05/2019) : - added "volatile" attribute to RINT1Trigd & RINT2Trigd variables
//                                to fix some occurrences of failed interrupts tests
//                              - changed sequencing principle in loop() function for periodic ARINC emission
//                                to enhance 1ms period accuracy. This was verified with an AriScope from Naveol:
//             http://naveol.com/index.php?menu=product&p=4
//
// This program is distributed in the hope that it will be useful, but without any
// warranty; without even the implied warranty of merchantability or fitness for a
// particular purpose. See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with this
// program. If not, see <http://www.gnu.org/licenses/>.
//

#include <SPI.h>

// pin numbers
#define SS     10  // SPI Slave Select
#define OE      9  // Output Enable of voltage translator (TXB0104)
#define MR      8  // HI-3593 Master Reset
#define TxEmpty 7  // HI-3593 Transmitter Empty
#define DIn0    5  // Discrete input #0 (0V / open)
#define DIn1    6  // Discrete input #1 (0V / open)
#define DOut    4  // Discrete output (0V / Open)
#define AIn0    A5 // Analog input #0 (0V .. 5V)
#define AIn1    A4 // Analog input #1 (0V .. 5V)
#define AIn2    A3 // Analog input #2 (0V .. 5V)
#define AIn3    A2 // Analog input #3 (0V .. 5V)
#define R1Int   3  // HI-3593 Receiver #1 interrupt
#define R2Int   2  // HI-3593 Receiver #2 interrupt
#define RTOut   1  // RS422 Tx Out
#define RRIn    0  // RS422 Rx In

// clock divider (page 9, table 2)
   // for prototypes (Arduino USB 12MHz oscillator is used)
//#define ClkDiv 0x0C
   // for serial production (dedicated 16MHz oscillator on the shield)
#define ClkDiv 0x10

// next timer rendez-vous
unsigned long int prevTime; // JPP V1.03

SPISettings HI3593SpiSettings(
  // for arduino, SPI clock will be @16MHz/2 = 8MHz (the nearest from 10MHz)
  10000000, // page 1, features: "10MHz SPI"
  MSBFIRST, // page 14, fig 5
  SPI_MODE0 // page 14, fig 5
);

// interrupt service routine for digital input #3 (RINT1)
volatile bool RINT1Trigd; // JPP V1.03
void isr1() {
  RINT1Trigd = true;
}

// interrupt service routine for digital input #2 (RINT2)
volatile bool RINT2Trigd; // JPP V1.03
void isr2() {
  RINT2Trigd = true;
}

void setup() {
  // open serial port for debug
  Serial.begin(9600);
  Serial.println(F("\t>> Arinc 429 shield test program V1.03 <<\n")); // JPP V1.03

  // in/out pins configuration
  pinMode(SS,     OUTPUT);
  pinMode(OE,     OUTPUT);
  pinMode(MR,     OUTPUT);
  pinMode(TxEmpty, INPUT);
  pinMode(DIn1,    INPUT);
  pinMode(DIn0,    INPUT);
  pinMode(DOut,   OUTPUT);
  pinMode(R1Int,   INPUT);
  pinMode(R2Int,   INPUT);
  pinMode(RTOut,  OUTPUT);
  pinMode(RRIn,    INPUT);

  // default states on outputs
  digitalWrite(SS,    HIGH);
  digitalWrite(OE,    HIGH);
  digitalWrite(RTOut, HIGH);

  // reset HI-3593
  digitalWrite(MR, HIGH); delay(1); digitalWrite(MR,  LOW);

  // initialize SPI
  SPI.begin();
  SPI.beginTransaction(HI3593SpiSettings);

  // -------------------------------------------------------------------------
  // Test MR (master reset) signal to HI-3593
  // -------------------------------------------------------------------------
  // write a register (the ACLK division register)
  digitalWrite(SS, LOW); SPI.transfer(0x38); SPI.transfer(ClkDiv); digitalWrite(SS, HIGH);
  // read back the register
  digitalWrite(SS, LOW); SPI.transfer(0xd4); unsigned char readback = SPI.transfer(0); digitalWrite(SS, HIGH);
  bool shieldPres = readback==ClkDiv;
  // reset HI-3593
  digitalWrite(MR, HIGH); delay(1); digitalWrite(MR,  LOW);
  // read back again the register (default value of ACLK division register after reset is 0x00)
  digitalWrite(SS, LOW); SPI.transfer(0xd4); readback = SPI.transfer(0); digitalWrite(SS, HIGH);
  if (readback != ClkDiv && shieldPres) Serial.println(F("Test of Master Reset is:\t\tPASSED"));
  else Serial.println(F("Master Reset didn't have the expected effect :("));
  Serial.println();

  // -------------------------------------------------------------------------
  // Proper initialization after master reset
  // -------------------------------------------------------------------------
  // set the HI-3593 ACLK division register to accomodate the right clock frequency
  digitalWrite(SS, LOW);
  SPI.transfer(0x38); // Set ACLK division register (HI-3593 data sheet, page 6)
  SPI.transfer(ClkDiv); // page 9, table 2
  digitalWrite(SS, HIGH);

  // set the HI-3593 TX control register  
  digitalWrite(SS, LOW);
  SPI.transfer(0x08); // write transmit control register, page 4)
  SPI.transfer(0x20); // bit 5 (TMODE): send ARINC words without enable command (page 5)
  digitalWrite(SS, HIGH);

  // -------------------------------------------------------------------------
  // Test SPI communication with HI-3593 (reads back the TX control register)
  // -------------------------------------------------------------------------
  // read transmit control register
  digitalWrite(SS, LOW); SPI.transfer(0x84); unsigned char tcr = SPI.transfer(0); digitalWrite(SS, HIGH);
  if (tcr==0x20) Serial.println(F("Test of SPI from/to HI-3593 is:\t\tPASSED"));
  else {
    Serial.print(F("Incorrect read back of a HI-3593 register. Should read 0x20, obtained: 0x"));
    Serial.println(tcr, HEX);
  }
  Serial.println();

  // -------------------------------------------------------------------------
  // Test ARINC transmit and receive (requires to loopback TX to both RX)
  // -------------------------------------------------------------------------
  // emission of an ARINC word
  union {long int w; unsigned char b[4];} ArincWord;
  ArincWord.w = 0x40302010;
  digitalWrite(SS, LOW); SPI.transfer(0x0C); for (int i=0; i<4; i++) SPI.transfer(ArincWord.b[i]); digitalWrite(SS, HIGH);
  // waits fot complete transmission
  delay(1);
  // reads RX channel #1
  digitalWrite(SS, LOW); SPI.transfer(0xA0); for (int i=0; i<4; i++) ArincWord.b[i] = SPI.transfer(0); digitalWrite(SS, HIGH);
  if (ArincWord.w == 0x40302010) Serial.println(F("Test of ARINC TX loop back to RX1 is:\tPASSED"));
  else {
    Serial.print(F("Incorrect loop back of A429 TX on RX1. Should read 0x40302010, obtained: 0x"));
    Serial.println(ArincWord.w, HEX);
  }
  // reads RX channel #2
  ArincWord.w = 0;
  digitalWrite(SS, LOW); SPI.transfer(0xC0); for (int i=0; i<4; i++) ArincWord.b[i] = SPI.transfer(0); digitalWrite(SS, HIGH);
  if (ArincWord.w == 0x40302010) Serial.println(F("Test of ARINC TX loop back to RX2 is:\tPASSED"));
  else {
    Serial.print(F("Incorrect loop back of A429 TX on RX2. Should read 0x40302010, obtained: 0x"));
    Serial.println(ArincWord.w, HEX);
  }
  Serial.println();

  // -------------------------------------------------------------------------
  // Test ARINC receive interrupts RINT1 & RINT2 (loopback TX to both RX)
  // -------------------------------------------------------------------------
  // configure interrupts
  attachInterrupt(digitalPinToInterrupt(R1Int), isr1, RISING); // JPP V1.03
  attachInterrupt(digitalPinToInterrupt(R2Int), isr2, RISING); // JPP V1.03
  RINT1Trigd = false; RINT2Trigd = false;
  // emission of an ARINC word
  ArincWord.w = 0x40302010;
  digitalWrite(SS, LOW); SPI.transfer(0x0C); for (int i=0; i<4; i++) SPI.transfer(ArincWord.b[i]); digitalWrite(SS, HIGH);
  // waits for complete transmission
  delay(1);
  if (RINT1Trigd) Serial.println(F("Test of RINT1 interrupt is:\t\tPASSED"));
  else Serial.println(F("RINT1 didn't trigger :("));
  if (RINT2Trigd) Serial.println(F("Test of RINT2 interrupt is:\t\tPASSED"));
  else Serial.println(F("RINT2 didn't trigger :("));
  detachInterrupt(digitalPinToInterrupt(R1Int)); // JPP V1.03
  detachInterrupt(digitalPinToInterrupt(R2Int)); // JPP V1.03
  Serial.println();

  // -------------------------------------------------------------------------
  // Test ARINC "transmit empty" (TEMPTY) signal
  // -------------------------------------------------------------------------
  // emission of several ARINC words
  ArincWord.w = 0x40302010;
  for (int j=0; j<12; j++)
    {digitalWrite(SS, LOW); SPI.transfer(0x0C); for (int i=0; i<4; i++) SPI.transfer(ArincWord.b[i]); digitalWrite(SS, HIGH);}
  int c=0;
  int InitialTxEmpty = digitalRead(TxEmpty);
  while (!digitalRead(TxEmpty) && c<1000) c++;
  int FinalTxEmpty = digitalRead(TxEmpty);
  if (InitialTxEmpty==0 && FinalTxEmpty==1) Serial.println(F("Test of TEMPTY is:\t\t\tPASSED"));
  else Serial.println(F("TEMPTY didn't evolve as exptected (check ACLK devider) :("));
  Serial.println();

  // ---------------------------------------------------------------------------
  // Test DOUT, DIN1 & DIN2 0v/open discrete ; and AIN0 .. AIN3 analog signals
  // DOUT is to be looped back to DIN1, DIN2 and AIN0 .. AIN3
  // ---------------------------------------------------------------------------
  digitalWrite(DOut, LOW);
  char id0 = digitalRead(DIn0); char id1 = digitalRead(DIn1);
  int  ia0 = analogRead(AIn0);  int  ia1 = analogRead(AIn1);
  int  ia2 = analogRead(AIn2);  int  ia3 = analogRead(AIn3);
  digitalWrite(DOut, HIGH);
  char fd0 = digitalRead(DIn0); char fd1 = digitalRead(DIn1);
  int  fa0 = analogRead(AIn0);  int  fa1 = analogRead(AIn1);
  int  fa2 = analogRead(AIn2);  int  fa3 = analogRead(AIn3);
  if (id0==1 && fd0==0)   Serial.println(F("Test of loop back of DOUT to DIN0 is:\tPASSED"));
  else                    Serial.println(F("Incorrect loop back of DOUT to DIN0 :("));
  if (id1==1 && fd1==0)   Serial.println(F("Test of loop back of DOUT to DIN1 is:\tPASSED"));
  else                    Serial.println(F("Incorrect loop back of DOUT to DIN1 :("));
  if (ia0>923 && fa0<100) Serial.println(F("Test of loop back of DOUT to AIN0 is:\tPASSED"));
  else                    Serial.println(F("Incorrect loop back of DOUT to AIN0 :("));
  if (ia1>923 && fa1<100) Serial.println(F("Test of loop back of DOUT to AIN1 is:\tPASSED"));
  else                    Serial.println(F("Incorrect loop back of DOUT to AIN1 :("));
  if (ia2>923 && fa2<100) Serial.println(F("Test of loop back of DOUT to AIN2 is:\tPASSED"));
  else                    Serial.println(F("Incorrect loop back of DOUT to AIN2 :("));
  if (ia3>923 && fa3<100) Serial.println(F("Test of loop back of DOUT to AIN3 is:\tPASSED"));
  else                    Serial.println(F("Incorrect loop back of DOUT to AIN3 :("));
  Serial.println();
  
  // JPP V1.02 (begin ...)
  // ---------------------------------------------------------------------------
  // Test of RS422 TX(H/L) and RS422 RX(H/L) signals
  // RS422 TX(H/L) is to be looped back to RS422 RX(H/L)
  // ---------------------------------------------------------------------------
  Serial.print(F("Place the RS loop back jumpers (test will be performed in 5 seconds) ..."));
  delay(5000);
  bool rsOk = true;
  Serial.write(0xaa); delay(2); if (Serial.read() != 0xaa) rsOk = false;
  Serial.write(0x55); delay(2); if (Serial.read() != 0x55) rsOk = false;
  Serial.println();
  if (rsOk) Serial.println(F("Test of loop back of RS TX to RX is:\tPASSED"));
  else      Serial.println(F("Incorrect loop back of RS TX to RX :("));
  // JPP V1.02 (... end)
  
  Serial.println(F("\t\tThat's all folks !"));
  
  prevTime = micros(); // JPP V1.03
}

void loop() {
  // repeatedly emit an ARINC word (this permits to analyze the signal with an oscilloscope)
  union {long int w; unsigned char b[4];} ArincWord;
  ArincWord.w = 0x40302010;
  digitalWrite(SS, LOW); SPI.transfer(0x0C); for (int i=0; i<4; i++) SPI.transfer(ArincWord.b[i]); digitalWrite(SS, HIGH);
  // waits 1ms to ensure FIFO is flushed
  unsigned long int nextTime = prevTime + 1000;       // JPP V1.03
  while((signed long int)(micros() - nextTime) <= 0); // JPP V1.03
  prevTime = nextTime;                                // JPP V1.03
}
