//
// ClockJitterMeas.ino : measurement of the jitter on a clock.
// Pulses are applied to the digital input #2.
// On each rising edge, the arduino 1MHz timer content is sent over the COM interface.
// The format on the COM interface is HEX ascii terminated by an EOL (end of line).
// Selftest feature: uncomment "#define selfTest" to get pulses on output #13
//

//#define selfTest

const byte pulseInput = 2; // the input on which the pulse is applied
const byte pulseOutput = 13; // the output that generates a pulse (for selftest: connect D13 to D2)

volatile unsigned long int prevTime;

// interrupt service routine
void isr() { Serial.println((unsigned long int)micros(),HEX); }

void setup() {
  // init serial port
  Serial.begin(230400);

  // configures pulse input
  pinMode(pulseInput, INPUT);
  attachInterrupt(digitalPinToInterrupt(pulseInput), isr, RISING);

  // configures pulse output (selftest)
  pinMode(pulseOutput, OUTPUT);
  digitalWrite(pulseOutput, LOW); // default state

  prevTime = micros();
}

void loop() {
# ifdef selfTest
  digitalWrite(pulseOutput, HIGH);
  while ((signed long int)(micros()-prevTime)<937); // 9/9600 * 1e6 = 937.5µs
  digitalWrite(pulseOutput, LOW);
  while ((signed long int)(micros()-prevTime)<10000);
  prevTime+=10000;
# endif
}
