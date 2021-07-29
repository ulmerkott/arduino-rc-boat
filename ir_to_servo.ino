#include <ezBuzzer.h>
#include <Servo.h>

// #define IR_USE_AVR_TIMER1 // This can change IRremote from using timer2 (which ezBuzzer uses) to timer1 (which servo is using)
#include <IRremote.h>


const int MOTOR_FW_PIN = 2;
const int BUZZER_PIN = 3;
const int LED_PIN = 5;           // the PWM pin the LED is attached to
const int IR_RECEIVE_PIN = 7;  // pin for the IR sensor

const int ROT_CENTER = 90;
const int ROT_STEP = 15;
const int ROT_MAX_LIMIT = 180;
const int ROT_MIN_LIMIT = 0;
const int SERVO_RESET_TIMEOUT = 200; // ms

const int MIN_BRIGHTNESS = 60;

const int LED_FADE_TIME = 30;

const int KEY_MINUS = 0x7;
const int KEY_PLUS = 0x15;
const int KEY_CH_MINUS = 0x45;
const int KEY_CH = 0x46;
const int KEY_CH_PLUS = 0x47;
const int KEY_NEXT = 0x44;
const int KEY_PREV = 0x40;
const int KEY_HPLUS = 0xD;
const int KEY_HMINUS = 0x19;
const int KEY_PLAYPAUSE = 0x43;

// Till havs
int melody[] = {
  NOTE_B4, 
  NOTE_GS5, 0, NOTE_FS5, NOTE_GS5, NOTE_FS5, NOTE_E5, NOTE_CS5, NOTE_E5,
  NOTE_B4, 0, NOTE_B4,
  NOTE_FS5, 0, NOTE_E5, NOTE_FS5, NOTE_E5, NOTE_DS5, NOTE_E5, NOTE_FS5,
  NOTE_A4, NOTE_GS4, NOTE_GS4, 0, 0
};

// note durations: 4 = quarter note, 8 = eighth note, etc, also called tempo:
int noteDurations[] = {
  2,
  2, 8, 8, 8, 4, 4, 4, 4,
  1, 2, 2,
  2, 8, 8, 8, 4, 4, 4, 4,
  3, 8, 1,
  1, 1
};


Servo myservo;
ezBuzzer buzzer(BUZZER_PIN);

void setup()
{
  pinMode(LED_PIN, OUTPUT);
  pinMode(MOTOR_FW_PIN, OUTPUT);
  digitalWrite(MOTOR_FW_PIN, LOW);

  Serial.begin(9600);
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK); // Start the receiver, enable feedback LED, take LED feedback pin from the internal boards definition

  myservo.attach(9);  // attaches the servo on pin 9 to the servo object
  myservo.write(ROT_CENTER);
}

int CurTime = 0;

int brightness = MIN_BRIGHTNESS;    // how bright the LED is
int fadeAmount = 5;    // how many points to fade the LED by
int LedFadeStart = 0;


int rot = ROT_CENTER;
int rot_max = 180;
int rot_min = 0;
int resetTimeStamp = -1; // if this is set to something else than -1 rotation will be reset after (millis() - resetTimeStamp) > SERVO_RESET_TIMEOUT
bool PlayMusic = false;

int clamp(int value) {
  value = min(value, rot_max);
  return max(value, rot_min);  
}

void fadeLed() {
 
  if ((CurTime - LED_FADE_TIME) < LedFadeStart) {
    return;
  }
  
  analogWrite(LED_PIN, brightness);
  // change the brightness for next time through the loop:
  brightness = brightness + fadeAmount;

  // reverse the direction of the fading at the ends of the fade:
  if (brightness <= MIN_BRIGHTNESS || brightness >= 255) {
    fadeAmount = -fadeAmount;
  }
  
  // wait for 30 milliseconds to see the dimming effect
  LedFadeStart = CurTime;
}

void handleMusic() {
  buzzer.loop(); // MUST call the buzzer.loop() function in loop()

  if (!PlayMusic && buzzer.getState() != BUZZER_IDLE) {
    buzzer.stop();
  }

  if (PlayMusic && buzzer.getState() == BUZZER_IDLE) {
    int length = sizeof(noteDurations) / sizeof(int);
    buzzer.playMelody(melody, noteDurations, length); // playing
  }
}


void loop()
{
  CurTime = millis();
  fadeLed();
  handleMusic();
  
  bool keyValid = false;
  // pause IR while music is playing since buzzer is using same timer (2) as IRremote lib
  if (!PlayMusic && IrReceiver.decode()) { 
    //Serial.println(IrReceiver.decodedIRData.decodedRawData, DEC);
    IrReceiver.printIRResultShort(&Serial);
    keyValid = true;
    bool isRepeat = (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT);
    switch (IrReceiver.decodedIRData.command ) {
      case KEY_CH_MINUS:
        rot += ROT_STEP;
        rot = clamp(rot);
        break;
      case KEY_CH_PLUS:
        rot -= ROT_STEP;
        rot = clamp(rot);
        break;
      case KEY_CH:
        rot = 90;
        break;
      case KEY_NEXT:
        rot = rot_max;
        resetTimeStamp = CurTime; // returns bad value sometimes
        break;
      case KEY_PREV:
        rot = rot_min;
        resetTimeStamp = CurTime; // returns bad value sometimes
        break;
      case KEY_PLUS:
        digitalWrite(2, HIGH);
        break;
      case KEY_MINUS:
        digitalWrite(2, LOW);
        break;
      case KEY_HMINUS: // Decrease min/max with 15degres
        rot_min += 15;
        rot_min = min(rot_min, ROT_CENTER);
        rot_max -= 15;
        rot_max = max(rot_max, ROT_CENTER);
        break;
      case KEY_HPLUS: // Increase min/max with 15degres
        rot_min -= 15;
        rot_min = max(rot_min, ROT_MIN_LIMIT);
        rot_max += 15;
        rot_max = min(rot_max, ROT_MAX_LIMIT);
        break;
      case KEY_PLAYPAUSE:
        if (isRepeat)
          break;
        PlayMusic = !PlayMusic;
        break;
      default:
        keyValid = false;
        break;
    }
    rot = clamp(rot);
    myservo.write(rot);
    IrReceiver.resume();
  }
  //  Serial.println("Event");
  //
  //  Serial.println(resetTimeStamp, DEC);
  //  Serial.println(millis(), DEC);
  
  // Reset servo to middle position after if NEXT/PREV buttons are used
  if (!keyValid) {
    // Bug causes millis() to return wrong value...
    if (resetTimeStamp != -1) {
      if ((millis() - resetTimeStamp) > SERVO_RESET_TIMEOUT || resetTimeStamp < -1 /*bug?*/) {
        resetTimeStamp = -1;
        rot = ROT_CENTER;
        myservo.write(ROT_CENTER);
      }
    }
  // Code used when above bug is present
  //    if (resetTimeStamp > -1) {
  //      if (resetTimeStamp * EVENT_LOOP_WAIT > SERVO_RESET_TIMEOUT) {
  //        resetTimeStamp = -1;
  //        rot = ROT_CENTER;
  //        myservo.write(ROT_CENTER);
  //      }
  //      else {
  //        resetTimeStamp++;
  //      }
  //    }
  }
}
