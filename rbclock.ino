#include <Arduino.h>
#include <TM1637Display.h>
#include <NMEAGPS.h>
#include <GPSport.h>

const int CLK1 = 16;
const int DIO1 = 15;
const int CLK2 = 18;
const int DIO2 = 17;
const int BUTTONPIN = 5;
const int PPS = 6;
const int SETLED = 2;
const int T1 = 11;   // Timer 1 fuer 1kHz-Signal

volatile unsigned int i, s, m, h, mon, dom, y, to;
volatile unsigned int leapday = 0 ;
unsigned long hms = 100000 ; 
int gps_s, gps_m, gps_h, gps_mon, gps_dom, gps_y;

int month_length[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

boolean setme = true;
String nmeabuffer;

TM1637Display display1(CLK1, DIO1);
TM1637Display display2(CLK2, DIO2);

NMEAGPS  gps; // This parses the GPS characters
gps_fix  fix; // This holds on to the latest values

void setup() {
  noInterrupts();
  pinMode(T1, INPUT);
  pinMode(BUTTONPIN, INPUT_PULLUP);
  pinMode(SETLED, OUTPUT); // setme LED
  digitalWrite(1, HIGH);
  delay(1000);
  digitalWrite(T1, HIGH);

  display1.setBrightness(0x0f);
  display2.setBrightness(0x0f);
  display1.clear();
  display2.clear();

  // Timer zum Zählen der 1kHz-Pulse konfigurieren

  TCCR1A = 0;
  TCCR1B = 71;
  TCCR1B |= (1 << WGM12); // CTC mode
  TCNT1 = 1;
  TIMSK1 = 2;
  OCR1A = 999;
  interrupts();

  Serial.begin(38400);
  while (!Serial && millis () < 3000UL)
    ;
  gpsPort.begin(9600);

  attachInterrupt(digitalPinToInterrupt(BUTTONPIN), set_me, CHANGE );
  Serial.println("hello in setup()");

}

ISR(TIMER1_COMPA_vect) {
  Serial.println("INTERRUPT#######################");
  i = 0; // Hack!
  if (s < 59) { // FIXME Schaltsekunde
    s++ ;
  }
  else {
    s = 0;
    if (m < 59) {
      m++ ;
    }
    else {
      m = 0;
      if (h < 23) {
        h++ ;
      }
      else {
        h = 0;
        if (dom < (month_length[mon - 1] + leapday ) ) {
          dom++;
        }
        else {
          dom = 1 ;
          if (mon < 12) {
            mon++ ;
          }
          else {
            mon = 1 ;
            y++ ;
          }
        }
      }
    }
  }
}

void set_me() {
  setme = true;
}

void align_pps() {
  i = 0;
  Serial.println("align pps");
}
void set_clock(){
  Serial.println("in set_clock");
  TIMSK1 = 0; //disable Timer interrupt
  attachInterrupt(digitalPinToInterrupt(PPS), align_pps, RISING);
  
  delay(2000);
  TIMSK1 = 2; //disable Timer interrupt 
  detachInterrupt(digitalPinToInterrupt(PPS));
  while (!gps.available( gpsPort )) {
  Serial.println("gps not available");
  delay(10);
  }
  fix = gps.read();
  while (!fix.valid.altitude) {
    Serial.println("no overdetermined fix");
    delay(10);
    fix = gps.read();
  }
  if (fix.valid.time) { // fix.valid.altitude = overdetermined fix mit 4 oder mehr sats

    gps_s =  fix.dateTime.seconds;
    gps_m =  fix.dateTime.minutes;
    gps_h =  fix.dateTime.hours;
    gps_mon = fix.dateTime.month;
    gps_dom = fix.dateTime.date;
    gps_y = fix.dateTime.year + 2000; // FIXME: library nimmt nur die letzten 2 Stellen des Jahrs

    h = gps_h;
    m = gps_m;
    s = gps_s;
    mon = gps_mon;
    dom = gps_dom;
    y = gps_y;
    }
}


int getCheckSum(String s) {
  // NMEA-Checksum Matthias Busse 18.05.2014 Version 1.1

  unsigned int j, XOR, c;

  for (XOR = 0, j = 0; j < s.length(); j++) {
    c = (unsigned char)s.charAt(j);
    if (c == '*') break;
    if ((c != '$') && (c != '!')) XOR ^= c;
  }
  return XOR;
}

void loop() {
  display1.showNumberDec(100 * h + m, true);
  display2.showNumberDec(s, false);

  i = TCNT1;
  delay(100);

  nmeabuffer = ""; 
  nmeabuffer = nmeabuffer + "$ZAZDA," + (10000L*h + 100*m + s) + "." + i + "," + dom + "," + mon + "," + y + ",-02,00*";

  digitalWrite(SETLED, setme);

  if ( (mon == 2 ) && (( y % 4 == 0 ) && (y % 100 != 0) || (y % 400 == 0)) ) {
    leapday = 1;
  }
  else {
    leapday = 0 ;
  }

  if (setme) {
    set_clock();
    setme = false ;
    digitalWrite(1, LOW);
  } else {
    Serial.print(nmeabuffer);
    Serial.println( getCheckSum(nmeabuffer), HEX);
  }
}
