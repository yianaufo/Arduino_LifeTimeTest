
#include <Servo.h> 
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define VER "1.0.0"
#define DELAY_INTERVAL_SEC   45
#define VALID_PAGE_FLIP_VAL  6
#define SENSOR_INPUT_PIN     A0
#define SERVO_L_PIN          10
#define SERVO_INIT_POSITION  15
#define SERVO_TARGET_POSITION  22
#define SERVO_RETRY_COUNT    4
#define SAMPLE_MODE_AVG      1
#define SAMPLE_MODE_MIN      2
#define SAMPLE_MODE_MAX      3
#define DEBUG false


volatile unsigned int count = 0; 
volatile boolean ready = false;
Servo servo_L; 
unsigned int totalSuccessPageFlip = 0;
unsigned int totalFailPageFlip = 0;
unsigned int totalRetry = 0;
LiquidCrystal_I2C lcd(0x27,16,2); // set the LCD address to 0x27

void TimerSetup(unsigned int sec) {
  TCCR1A = 0x00;                // Normal mode, just as a Timer
  TCCR1B &= ~_BV(CS12);         // no prescaling
  TCCR1B &= ~_BV(CS11);       
  TCCR1B |= _BV(CS10);     
  TIMSK1 |= _BV(TOIE1);         // enable timer overflow interrupt
  TCNT1 = 0;
  count = sec*244;
}

ISR (TIMER1_OVF_vect) {  
  count--;
  if (count == 0) {             // overflow frequency = 16 MHz/65536 = 244Hz
    ready = true;
    TimerSetup(DELAY_INTERVAL_SEC);   
  } 
}

int diff(unsigned int v1, unsigned int v2) {
    if (v1 > v2)  return v1 - v2;
    else return v2 - v1;
}

int ReadLeightSensor( unsigned int pin, 
                      unsigned int sample, 
                      unsigned int interval,
                      unsigned int mode, 
                      unsigned int ref) {
  int minVal = 1024, maxVal = 0, rcVal = 0, totalVal = 0, val = 0;
  for (int i=0; i<sample; i++) {
    val= analogRead(pin);
    if (DEBUG) Serial.println(val);  
    totalVal += val;
    if (val < minVal) minVal = val;
    if (val > maxVal) maxVal = val;
    delay(interval);
  }
  if (mode == SAMPLE_MODE_AVG) {
    rcVal = totalVal / sample;
  }else if (mode == SAMPLE_MODE_MIN | SAMPLE_MODE_MAX) {
    rcVal = max(diff(minVal, ref), diff(maxVal, ref));
  }else if (mode == SAMPLE_MODE_MIN) {
    rcVal = minVal;
  }else if (mode == SAMPLE_MODE_MAX) {
    rcVal = maxVal;
  }
  return rcVal;
}

void LightSeneorSetup() {
  // TODO: init setup, calibration the env light
}

void LcdDisplayLogo() {
  Wire.begin();
  lcd.init(); 
  lcd.backlight();
  lcd.setCursor(0, 0);  
  lcd.print("          Netronix, INC.");
  for (int i=0; i<10; i++) {
    delay(200);
    lcd.scrollDisplayLeft();
  }
  for (int i=0; i<3; i++) {  
    lcd.setCursor(0, 1);
    if (i%2)   lcd.print("          ----------------");
    else       lcd.print("          Life Time Test..");
    delay(200);
  }
  delay(1200);
  //show version
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Life Time Test..");
  lcd.setCursor(0, 1);
  lcd.print("VER: ");
  lcd.print(VER);
  delay(1200);
}

void LcdStandby() {
  lcd.clear();
  lcd.setCursor(0, 0);  
  lcd.print("Total Fail Retry");
}

void LcdUpdateCount(int pos, int len, int count) {
  lcd.setCursor(pos, 1); 
  //clear field
  for (int i=0; i<len; i++)  lcd.print(" ");
  lcd.setCursor(pos, 1); 
  lcd.print(count, DEC);
}

void ServoSetup(unsigned int pin, unsigned int initPosition) {
  servo_L.attach(pin);
  servo_L.write(initPosition);
  delay(1000);
}

void ServoGo(unsigned int pos) {
  //if (DEBUG) { Serial.print("Servo Go"); Serial.println(pos); }
  servo_L.write(pos);
}

void PageFlip() {
  ServoGo(SERVO_TARGET_POSITION);
  delay(200);
  ServoGo(SERVO_INIT_POSITION);
  delay(200);
}

void setup() {
  // put your setup code here, to run once:
  if (DEBUG) Serial.begin(9600);
  if (DEBUG) Serial.println("Setup!");
  LcdDisplayLogo();                                    //logo
  TimerSetup(DELAY_INTERVAL_SEC);                      //setup timer interrupt
  ServoSetup(SERVO_L_PIN, SERVO_INIT_POSITION);        //move motor to init position
  LightSeneorSetup();                                  //init light sensor
  LcdStandby();                                        //standby
}

void loop() {
  unsigned int prevVal, curVal, diffVal, retry;
  if (DEBUG) Serial.println("Loop!");
  // put your main code here, to run repeatedly:
  while (!ready) ;
  ready = false;
  if (DEBUG) Serial.println("Get Ready!");
  // wait until timeout
  
  retry = SERVO_RETRY_COUNT;
  do {
    // get value of current screen 
    prevVal = ReadLeightSensor(SENSOR_INPUT_PIN, 3, 200, SAMPLE_MODE_AVG, 0);
    // flip page
    PageFlip();
    // monitor the value during page flip
    diffVal = ReadLeightSensor(SENSOR_INPUT_PIN, 20, 100, 
                              SAMPLE_MODE_MAX|SAMPLE_MODE_MAX, prevVal);
    
    if (retry < SERVO_RETRY_COUNT) {
      totalRetry ++;
      LcdUpdateCount(11, 5, totalRetry);
      if (DEBUG) Serial.print("Retry_"); Serial.print(retry);
    }else {
      if (DEBUG) Serial.print("Try");
    }
    if (DEBUG) Serial.print(" diff "); Serial.println(diffVal);
    retry --;
  } while ((diffVal < VALID_PAGE_FLIP_VAL) && (retry));
  
  
  if (retry == 0) {
    // run out retry, failed
    totalFailPageFlip ++;
    LcdUpdateCount(6, 4, totalFailPageFlip);
    if (DEBUG) {
      Serial.print("FAILED IN PAGE FLIP ");
      Serial.println(totalFailPageFlip);
    }
  } else {
    // success
    totalSuccessPageFlip ++;
    LcdUpdateCount(0, 5, totalSuccessPageFlip);
    if (DEBUG) {
      Serial.print("SUCCESS WAIT NEXT FLIP ");
      Serial.println(totalSuccessPageFlip);
    } 
  } 
}


