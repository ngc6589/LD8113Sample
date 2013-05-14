#include <SPI.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------
 -- Grobal Variables
 ------------------------------------------------------------ */
unsigned int msCount = 0;
boolean msCountOVF;
unsigned char vfdDigit = 0;
unsigned char vfdSeg[6];
unsigned char vfdDot[6];
char str01[10];
int hour = 11;
int minute = 56;
int second = 00;
int dot = 0;

String inputString = "";     // a string to hold incoming data
boolean stringComplete = false;  // whether the string is complete

/* ------------------------------------------------------------
 -- Interrupt hundler (every 0.5ms)
 ------------------------------------------------------------ */
ISR(TIMER2_COMPA_vect) {

  sendVfd();
  msCount++;
  if(msCount >= 2000) {  // 1seconds 
    msCount = 0;
    msCountOVF = true;
  }
  // Reset interrupt flag and counter.
  TIFR2 &= ~(1<<OCF2A);
}

/* ------------------------------------------------------------
 -- Initial Setup
 ------------------------------------------------------------ */
void setup() {

  pinMode(4, OUTPUT);  // SCLR
  pinMode(5, OUTPUT);  // RCK

  // initialize SPI
  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV2);
  SPI.setDataMode(SPI_MODE0);
  SPI.setBitOrder(LSBFIRST);

  // initialize serial:
  Serial.begin(9600);
  inputString.reserve(40);

  // Setup TIMER2
  // a. Disable the Timer/Counter2 interrupts by clearing OCIE2x and TOIE2.
  // b. Select clock source by setting AS2 as appropriate.
  // c. Write new values to TCNT2, OCR2x, and TCCR2x.
  // d. To switch to asynchronous operation: Wait for TCN2xUB, OCR2xUB, and TCR2xUB.
  // e. Clear the Timer/Counter2 Interrupt Flags.
  // f. Enable interrupts, if needed.

  // TIMSK2 2=OCIE2B 1=OCIE2A 0=TOIE2
  // Disable interrupt enable
  TIMSK2 &= ~(1<<OCIE2B);
  TIMSK2 &= ~(1<<OCIE2A);
  TIMSK2 &= ~(1<<TOIE2);
  // ASSR – Asynchronous Status Register
  // 6=EXCLK 5=AS2 4=TCN2UB 3=OCR2AUB 2=OCR2BUB 1=TCR2AUB 0=TCR2BUB  
  // AS2=0 CLKIO,  AS2=1 TOSC1
  ASSR &= ~(1<<AS2);
  // Waveform Generation Mode set to NORMAL mode WGM22,21,20=0
  TCCR2A &= ~(1<<WGM20);
  TCCR2A |= (1<<WGM21);
  TCCR2B &= ~(1<<WGM22);
  // TCCR2B – Timer/Counter Control Register B
  // 7=FOC2A 6=FOC2B 3=WGM22 2=CS22 1=CS21 0=CS20
  // CS22 CS21 CS20 Description
  // 0    0    0    No clock source (Timer/Counter stopped).
  // 0    0    1    clkT2S/(No prescaling)
  // 0    1    0    clkT2S/8 (From prescaler)
  // 0    1    1    clkT2S/32 (From prescaler)
  // 1    0    0    clkT2S/64 (From prescaler)
  // 1    0    1    clkT2S/128 (From prescaler)
  // 1    1    0    clkT2S/256 (From prescaler)
  // 1    1    1    clkT2S/1024 (From prescaler)
  TCCR2B |= (1<<CS22);
  TCCR2B &= ~(1<<CS21);
  TCCR2B &= ~(1<<CS20);
  TCNT2 = 0;
  OCR2A = 124; // 16MHz / 64 = 4us. 4us * 125 = 0.5ms
  // Clear interrupt flag
  // TIFR2 – Timer/Counter2 Interrupt Flag Register
  // 2=OCF2B 1=OCF2A 0=TOV2
  TIFR2 &= ~(1<<OCF2A);
  // TIMSK2 2=OCIE2B 1=OCIE2A 0=TOIE2
  // Enable interrupt
  TIMSK2 |= (1<<OCIE2A);
}

/* ------------------------------------------------------------
 -- Main loop
 ------------------------------------------------------------ */
void loop() {
  int i;
  char buf01[10];
  
  // every 1 seconds
  if(msCountOVF == true) {
    if(dot == 0) {
      dot = 1;
      vfdDot[5] = 1;
    } else {
      dot = 0;
      vfdDot[5] = 0;
    }
    sprintf(str01, "%02d%02d%02d", hour, minute, second);
    for(i = 0; i < 6; i++) {
      convVfdSeg(i);
    }
    second++;
    if(second == 60) {
      second = 0;
      minute++;
    }
    if(minute == 60) {
      minute = 0;
      hour++;
    }
    if(hour == 24) {
      hour = 0;
    }
    msCountOVF = false;
  }

  // serial input done
  // 9600 bps
  // help[RET] -> print help message
  // HH MM SS[RET] -> set time
  //
  if (stringComplete) {
    //Serial.println(inputString); 
    if(inputString.equals("help\r")) {
      Serial.println("help: this message");
      Serial.println("time set: hh mm ss[return] ");
    }
    if(inputString.length() == 9) {
      inputString.toCharArray(buf01, 9);
      buf01[2] = 0;
      buf01[5] = 0;
      buf01[8] = 0;
      hour = atoi(buf01);
      minute = atoi(&buf01[3]);
      second = atoi(&buf01[6]);
    }    
    inputString = "";
    stringComplete = false;
  }
}

/* ------------------------------------------------------------
 -- sendVfd
 ------------------------------------------------------------ */
void sendVfd() {
  unsigned char digit;

  // HC595 SCLR(negedge: shift register clear)
  digitalWrite(4,LOW);
  digitalWrite(4,HIGH);  

  // HC595 RCK (posedge: data latch)
  digitalWrite(5,LOW);
  digitalWrite(5,HIGH);

  // send to shift register
  digit = (1<<vfdDigit);
  if(vfdDot[vfdDigit] != 0) {
    digit |= 0x80;
  }
  SPI.transfer(digit);
  SPI.transfer(vfdSeg[vfdDigit]);  

  // HC595 RCK (posedge: data latch)
  digitalWrite(5,LOW);
  digitalWrite(5,HIGH);  

  vfdDigit++;
  if(vfdDigit > 5) {
    vfdDigit = 0;
  }
}

/* ------------------------------------------------------------
 -- sendVfd
 ------------------------------------------------------------ */
void convVfdSeg(int digit) {

  switch(str01[digit]) {
  case '0': 
    vfdSeg[digit] = 0xFC; 
    break;
  case '1': 
    vfdSeg[digit] = 0x01; 
    break;
  case '2': 
    vfdSeg[digit] = 0xda; 
    break;
  case '3': 
    vfdSeg[digit] = 0xf2; 
    break;
  case '4': 
    vfdSeg[digit] = 0x66; 
    break;
  case '5': 
    vfdSeg[digit] = 0xb6; 
    break;
  case '6': 
    vfdSeg[digit] = 0xbe; 
    break;
  case '7': 
    vfdSeg[digit] = 0xe0; 
    break;
  case '8': 
    vfdSeg[digit] = 0xfe; 
    break;
  case '9': 
    vfdSeg[digit] = 0xe6; 
    break;
  }
}

/*
  SerialEvent occurs whenever a new data comes in the
 hardware serial RX.  This routine is run between each
 time loop() runs, so using delay inside loop can delay
 response.  Multiple bytes of data may be available.
 */
void serialEvent() {
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read(); 
    // add it to the inputString:
    inputString += inChar;
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if (inChar == '\r') {
      stringComplete = true;
    } 
  }
}
