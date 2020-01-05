/**
 *  Project:  Arinc Shield (Naveol Nav429) Tester
 *  Shield vendor: "http://naveol.com/index.php?menu=product&p=3"
 *  File:     main.cpp
 *  Author:   Jean-Paul PETILLON
 *
 *  note : The MBED library is "mbed" (not mbed-os). Latest tests of this SW
 *         were performed with version 164, dated 2018/11/08, of this library.
 *
 *  Version: 1.04 dated 2020/01/05 (affected lines are marked "JPP V1.04")
 *  - fixed teh R422 loopback test : in contrast with arduino, serial link
 *    on D0/D1 pins (the RS422 interface of NAV429) is distinct from the one
 *    connected to the PC via ST-LINK.
 *  - added a "clear screen" command at program start
 *
 *  Version: 1.03 dated 2018/02/04 (affected lines are marked "JPP V1.03")
 *  - added a reset command (this is usefull on nucleo boards when reset button
 *    is not accessible since these boards cannot be reset with UART protocol
 *    signals as arduino can). Achieved by sending the 'r' char from the PC.
 */

#include "mbed.h"

// in/out digital pins
DigitalOut SS    (D10);    // SPI Slave Select
DigitalOut OE     (D9);    // Output Enable of voltage translator (TXB0104)
DigitalOut MR     (D8);    // HI-3593 Master Reset
DigitalIn  TxEmpty(D7);    // HI-3593 Transmitter Empty
DigitalIn  DIn1   (D5);    // Discrete input #0 (0V / open)
DigitalIn  DIn0   (D6);    // Discrete input #1 (0V / open)
DigitalOut DOut   (D4);    // Discrete output (0V / Open)
DigitalOut RTOut  (D1);    // RS422 Tx Out
DigitalIn  RRIn   (D0);    // RS422 Rx In

// interrupt inputs
InterruptIn  R1Int (D3);   // HI-3593 Receiver #1 interrupt
InterruptIn  R2Int (D2);   // HI-3593 Receiver #2 interrupt
  
// analog input pins
AnalogIn   AIn0   (A5);    // Analog input #0 (0V .. 5V)
AnalogIn   AIn1   (A4);    // Analog input #1 (0V .. 5V)
AnalogIn   AIn2   (A3);    // Analog input #2 (0V .. 5V)
AnalogIn   AIn3   (A2);    // Analog input #3 (0V .. 5V)

// serial link for debug trace to PC (default: 9600, 8, 1, no parity) 
Serial pc(SERIAL_TX, SERIAL_RX);

// serial link that emulates arduino UART   // JPP V1.04
Serial serial(PG_14, PG_9);                 // JPP V1.04

// SPI from/to HI-3593
// note: requires to overwrite "d11_configuration" in file mbed_app.json
// alternatively, SPI_MOSI may be replaced by PB_5 in following line code
SPI spi(SPI_MOSI, SPI_MISO, SPI_SCK);

// interrupt service routine for digital input #3 (RINT1)
bool RINT1Trigd;
void isr1() {
  RINT1Trigd = true;
}

// interrupt service routine for digital input #2 (RINT2)
bool RINT2Trigd;
void isr2() {
  RINT2Trigd = true;
}
 
int main() {
    // welcome message
    pc.printf("\033[2J"); // clear screen // JPP V1.04
    pc.printf("\t>> NUCLEO-F746ZG / Arinc 429 shield test program <<\r\n\r\n");

    // default states on outputs
    SS = 1;
    OE = 1;
    RTOut = 1;

    // reset HI-3593
    MR = 1; wait_ms(1); MR = 0;

    // initialize SPI (hope it is MSB first as required page 14, fig 5)
    spi.format(8, 0);    // 8-bit data, MODE 0 (page 14, fig 5)
    spi.frequency(10e6); // page 1, features: "10MHz SPI"

    // -------------------------------------------------------------------------
    // Test MR (master reset) signal to HI-3593
    // -------------------------------------------------------------------------
    // write a register
    SS = 0; spi.write(0x38); spi.write(0x10); SS = 1;
    // read back the register
    SS = 0; spi.write(0xd4); unsigned char adr = spi.write(0); SS = 1;
    bool shieldPresent = adr==0x10;
    // reset HI-3593
    MR = 1; wait_ms(1); MR = 0;
    // read back again the register
    SS = 0; spi.write(0xd4); adr = spi.write(0); SS = 1;
    if (adr != 0x10 && shieldPresent) {
        pc.printf("Test of Master Reset is:\t\tPASSED\r\n");
    } else {
        pc.printf("Master Reset didn't have the expected effect :(\r\n");
    }
    pc.printf("\r\n");

    // -------------------------------------------------------------------------
    // Minimum HI-3593 settings for regular operation
    // -------------------------------------------------------------------------
    // Set the HI-3593 ACLK division register to accomodate 16MHz external clock
    SS = 0;
    spi.write(0x38); // Set ACLK division register (HI-3593 data sheet, page 6)
    spi.write(0x10); // page 9, table 2: 16MHz clock
    SS = 1;
    // set the HI-3593 TX control register  
    SS = 0;
    spi.write(0x08); // write transmit control register, page 4)
    spi.write(0x20); // bit 5 (TMODE): send ARINC words without enable command (page 5)
    SS = 1;

    // -------------------------------------------------------------------------
    // Test SPI communication with HI-3593 (reads back the TX control register)
    // -------------------------------------------------------------------------
    // read back the transmit control register
    SS = 0; spi.write(0x84); unsigned char tcr = spi.write(0); SS = 1;
    if (tcr==0x20) pc.printf("Test of SPI from/to HI-3593 is:\t\tPASSED\r\n");
    else {
      pc.printf("Incorrect read back of a HI-3593 register. Should read 0x20, obtained: 0x");
      pc.printf("%x\r\n", tcr);
    }
    pc.printf("\r\n");

    // -------------------------------------------------------------------------
    // Test ARINC transmit and receive (requires to loopback TX to both RX)
    // -------------------------------------------------------------------------
    // emission of an ARINC word
    union {long int w; unsigned char b[4];} ArincWord;
    ArincWord.w = 0x40302010;
    SS = 0; spi.write(0x0C); for (int i=0; i<4; i++) spi.write(ArincWord.b[i]); SS = 1;
    // waits fot complete transmission
    wait_ms(1);
    // reads RX channel #1
    SS = 0; spi.write(0xA0); for (int i=0; i<4; i++) ArincWord.b[i] = spi.write(0); SS = 1;
    if (ArincWord.w == 0x40302010) pc.printf("Test of ARINC TX loop back to RX1 is:\tPASSED\r\n");
    else {
        pc.printf("Incorrect loop back of A429 TX on RX1. Should read 0x40302010, obtained: 0x");
        pc.printf("%x\r\n", ArincWord.w);
    }
    // reads RX channel #2
    ArincWord.w = 0;
    SS = 0; spi.write(0xC0); for (int i=0; i<4; i++) ArincWord.b[i] = spi.write(0); SS = 1;
    if (ArincWord.w == 0x40302010) pc.printf("Test of ARINC TX loop back to RX2 is:\tPASSED\r\n");
    else {
        pc.printf("Incorrect loop back of A429 TX on RX2. Should read 0x40302010, obtained: 0x");
        pc.printf("%x\r\n", ArincWord.w);
    }
    pc.printf("\r\n");

    // -------------------------------------------------------------------------
    // Test ARINC receive interrupts RINT1 & RINT2 (loopback TX to both RX)
    // -------------------------------------------------------------------------
    // attach interrupts to their relevant service routines
    R1Int.fall(&isr1);
    R2Int.fall(&isr2);
    // reset the "interrupt triggered" flags
    RINT1Trigd = false; RINT2Trigd = false;
    // emission of an ARINC word
    ArincWord.w = 0x40302010;
    SS = 0; spi.write(0x0C); for (int i=0; i<4; i++) spi.write(ArincWord.b[i]); SS = 1;
    // waits for complete transmission
    wait_ms(1);
    if (RINT1Trigd) pc.printf("Test of RINT1 interrupt is:\t\tPASSED\r\n");
    else pc.printf("RINT1 didn't trigger :(\r\n");
    if (RINT2Trigd) pc.printf("Test of RINT2 interrupt is:\t\tPASSED\r\n");
    else pc.printf("RINT2 didn't trigger :(\r\n");
    pc.printf("\r\n");
    // detach interrupts from their ISRs
    R1Int.fall(NULL);
    R2Int.fall(NULL);

    // -------------------------------------------------------------------------
    // Test ARINC "transmit empty" signal
    // -------------------------------------------------------------------------
    // emission of some ARINC word
    ArincWord.w = 0x40302010;
    for (int j=0; j<12; j++)
      {SS = 0; spi.write(0x0C); for (int i=0; i<4; i++) spi.write(ArincWord.b[i]); SS = 1;}
    int InitialTxEmpty = TxEmpty;
    int c=0;
    // minimum number of iterations to flush FIFO is 65000 on NUCLEO at 216MHz
    while ((TxEmpty==0) && (c<130000)) c++;
    int FinalTxEmpty = TxEmpty;
    if (InitialTxEmpty==0 && FinalTxEmpty==1) pc.printf("Test of TEMPTY is:\t\t\tPASSED\r\n");
    else pc.printf("TEMPTY didn't evolve as exptected :(\r\n");
    pc.printf("\r\n");

    // ---------------------------------------------------------------------------
    // Test DOUT, DIN1 & DIN2 0v/open discrete ; and AIN0 .. AIN3 analog signals
    // DOUT is to be looped back to DIN1, DIN2 and AIN0 .. AIN3
    // ---------------------------------------------------------------------------

    DOut = 0;
    wait_ms(1);
    char id0 = DIn0; char id1 = DIn1;
    int  ia0 = AIn0.read_u16(); int  ia1 = AIn1.read_u16();
    int  ia2 = AIn2.read_u16(); int  ia3 = AIn3.read_u16();

    DOut = 1;
    wait_ms(1);
    char fd0 = DIn0; char fd1 = DIn1;
    int  fa0 = AIn0.read_u16(); int  fa1 = AIn1.read_u16();
    int  fa2 = AIn2.read_u16(); int  fa3 = AIn3.read_u16();

    if (id0==1 && fd0==0) pc.printf("Test of loop back of DOUT to DIN0 is:\tPASSED\r\n");
    else pc.printf("Incorrect loop back of DOUT to DIN0 :(\r\n");
    if (id1==1 && fd1==0) pc.printf("Test of loop back of DOUT to DIN1 is:\tPASSED\r\n");
    else pc.printf("Incorrect loop back of DOUT to DIN1 :(\r\n");
    if (ia0>40000 && fa0<1600) pc.printf("Test of loop back of DOUT to AIN0 is:\tPASSED\r\n");
    else pc.printf("Incorrect loop back of DOUT to AIN0 :(\r\n");
    if (ia1>40000 && fa1<1600) pc.printf("Test of loop back of DOUT to AIN1 is:\tPASSED\r\n");
    else pc.printf("Incorrect loop back of DOUT to AIN1 :(\r\n");
    if (ia2>40000 && fa2<1600) pc.printf("Test of loop back of DOUT to AIN2 is:\tPASSED\r\n");
    else pc.printf("Incorrect loop back of DOUT to AIN2 :(\r\n");
    if (ia3>40000 && fa3<1600) pc.printf("Test of loop back of DOUT to AIN3 is:\tPASSED\r\n");
    else pc.printf("Incorrect loop back of DOUT to AIN3 :(\r\n");

    // ---------------------------------------------------------------------------
    // Test of RS422 TX(H/L) and RS422 RX(H/L) signals
    // RS422 TX(H/L) is to be looped back to RS422 RX(H/L)
    // ---------------------------------------------------------------------------
    bool rsOk = true;
    char r;
    serial.putc(0xaa);                     // JPP V1.04
    wait_ms(2);
    if (serial.readable()) {               // JPP V1.04
        r = serial.getc();                 // JPP V1.04
        if (r != 0xaa) rsOk = false;
    } else rsOk = false;
    serial.putc(0x55);                     // JPP V1.04
    wait_ms(2);
    if (serial.readable()) {               // JPP V1.04
        r = serial.getc();                 // JPP V1.04
        if (r != 0x55) rsOk = false;
    } else rsOk = false;
    if (rsOk) pc.printf("Test of loop back of RS422 TX to RX is:\tPASSED\r\n");
    else      pc.printf("Incorrect loop back of RS422 TX to RX :(\r\n");

    pc.printf("\r\n\t\tThat's all folks !\r\n");

    // infinite loop
    while (1) {     
        // transmit cyclically an ARINC word
        SS = 0; spi.write(0x0C); for (int i=0; i<4; i++) spi.write(ArincWord.b[i]); SS = 1;
        wait_us(360); // an ARINC word is 360µs => should not overflow thanks to SPI transmission duration
        
        // JPP V1.03 (begin ...)
        // check if a reset command is received
        if (pc.readable() && (pc.getc() == 'r')) NVIC_SystemReset();
        // JPP V1.03 (... end)
    }
}
