
#define USE_MUSIC
#define USE_SERVO

#include <Arduino.h>

#ifdef USE_MUSIC
#include <ezBuzzer.h>
#endif
//#include <Servo.h>
#include <ServoTimer2.h>
#include <AceButton.h>
using namespace ace_button;

#include <RH_ASK.h>
#ifdef RH_HAVE_HARDWARE_SPI
#include <SPI.h> // Not actually used but needed to compile
#endif


const int MOTOR_FW_PIN = 2;
const int MOTOR_BW_PIN = 4;
const int MOTOR_SPEED_PIN = 6; // Pin 9 is not working (interfere with RF)
const int BUZZER_PIN = 9;
const int LED_PIN = 5;           // the PWM pin the LED is attached to
const int SERVO_PIN = 3; // Was 9 for Servo lib (timer 1)
const int RF_RECEIVE_PIN = 11;  // pin for the RF receiver

const int MOTOR_MAX_SPEED = 255;
const int MOTOR_MIN_SPEED = 140; // Motor doesnt budge until this value

const int ROT_CENTER = 90;
const int ROT_STEP = 15;
const int ROT_MAX_LIMIT = 180;
const int ROT_MIN_LIMIT = 0;
const int KEY_RELEASED_TIMEOUT = 500; // ms

const int MIN_BRIGHTNESS = 60;
const int LED_FADE_TIME = 30;

enum KeyCodes {
  KEY_LEFT,
  KEY_RIGHT,
  KEY_UP,
  KEY_DOWN,
  KEY_PLAYMUSIC,
  KEY_SPEED
};

enum EngineStates {
  OFF,
  FORWARD,
  BACKWARD
};

EngineStates EngineState = OFF;
const int ENGINE_SPEED_STEP = 25;
int EngineSpeed = 0;

enum MusicStates {
  STOPPED,
  STOPPING,
  PLAYING
};

MusicStates MusicState = STOPPED;

#ifdef USE_MUSIC
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


ezBuzzer buzzer(BUZZER_PIN);
#endif


#ifdef USE_SERVO
ServoTimer2 myservo;
#endif
RH_ASK driver(1000);

void setup()
{
  pinMode(LED_PIN, OUTPUT);
  pinMode(MOTOR_FW_PIN, OUTPUT);
  pinMode(MOTOR_BW_PIN, OUTPUT);
  pinMode(MOTOR_SPEED_PIN, OUTPUT);
  digitalWrite(MOTOR_FW_PIN, LOW);
  digitalWrite(MOTOR_BW_PIN, LOW);
  analogWrite(MOTOR_SPEED_PIN, 0);

  Serial.begin(115200);
#ifdef USE_SERVO
  setServo(ROT_CENTER);
#endif
  if (!driver.init()) {
    Serial.println("init failed");
  }
}

unsigned long CurTime = 0;

int brightness = MIN_BRIGHTNESS;    // how bright the LED is
int fadeAmount = 5;    // how many points to fade the LED by
unsigned long LedFadeStart = 0;

const int SERVO_TIMEOUT = 350; // Put servo to sleep after a while to keep it from "ticking"
unsigned long SetServoTime = -1;
int rot_max = 180;
int rot_min = 0;

int clamp(int value) {
  value = min(value, rot_max);
  return max(value, rot_min);
}

void setEngineState(enum EngineStates state, int enSpeed) {
  enSpeed = constrain(enSpeed, MOTOR_MIN_SPEED, MOTOR_MAX_SPEED);
  switch (state) {
    case FORWARD:
      Serial.print("FORWARD: ");
      Serial.println(enSpeed);
      digitalWrite(MOTOR_BW_PIN, LOW);
      digitalWrite(MOTOR_FW_PIN, HIGH);
      break;
    case BACKWARD:
      Serial.print("BACKWARD: ");
      Serial.println(enSpeed);
      digitalWrite(MOTOR_FW_PIN, LOW);
      digitalWrite(MOTOR_BW_PIN, HIGH);
      break;
    case OFF: // fallthrough
    default:
      enSpeed = 0;
      Serial.println("OFF");
      digitalWrite(MOTOR_FW_PIN, LOW);
      digitalWrite(MOTOR_BW_PIN, LOW);
  }

  analogWrite(MOTOR_SPEED_PIN, enSpeed);
  EngineState = state;
  EngineSpeed = enSpeed;
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

void initTimer2() {
  TIMSK2 = 0;  // disable interrupts
  TCCR2A = 0;  // normal counting mode
  TCCR2B = _BV(CS21); // set prescaler of 8
  TCNT2 = 0;     // clear the timer2 count
  TIFR2 = _BV(TOV2);  // clear pending interrupts;
  TIMSK2 =  _BV(TOIE2) ; // enable the overflow interrupt
}

void handleMusic() {
#ifdef USE_MUSIC
  buzzer.loop(); // MUST call the buzzer.loop() function in loop()

  if (MusicState == STOPPING && buzzer.getState() == BUZZER_IDLE) {
    buzzer.stop();
    MusicState = STOPPED;
    // Reinitialize timer2 for Servo-lib after ezBuzzer used it to play music.
    initTimer2();
    return;
  }

  if (MusicState == PLAYING && buzzer.getState() == BUZZER_IDLE) {
    // Servo is using same timer (2) as buzzer, so lets disable it while music is playing.
    myservo.detach();
    setEngineState(OFF, 0); // Always turn off motor during music playback
    int length = sizeof(noteDurations) / sizeof(int);
    buzzer.playMelody(melody, noteDurations, length); // playing
    MusicState = STOPPING;
  }
#endif
}

void handleEngineKey(enum EngineStates state, int keyEvent) {
  switch(keyEvent) {
    case AceButton::kEventPressed:
      return; // ignore
    case AceButton::kEventClicked:
      if (EngineState == state || EngineState == OFF) {
        // Increase speed by ENGINE_SPEED_STEP if state is same direction.
        setEngineState(state, EngineSpeed == 0 ? MOTOR_MIN_SPEED : EngineSpeed + ENGINE_SPEED_STEP);
        return;
      }
      // If previous state was opposite direction, just turn off motor.
      break;
    case AceButton::kEventLongPressed:
      setEngineState(state, MOTOR_MIN_SPEED);
      break;
    case AceButton::kEventLongReleased: // fallthrough
    default:
      break;
  }
  setEngineState(OFF, 0);
}

void handleServoKey(int key, int keyEvent) {
  int rotation = key == KEY_LEFT ? rot_max : rot_min;

  if (keyEvent == AceButton::kEventPressed
      || keyEvent == AceButton::kEventLongPressed) {
    setServo(rotation);
  }
  else if (keyEvent == AceButton::kEventReleased
          || keyEvent == AceButton::kEventLongReleased) {
    setServo(ROT_CENTER);
  }
}

void handleKey(int keyCode, int keyEvent) {
  bool keyValid = true;

  switch (keyCode) {
    case KEY_RIGHT: // Fallthrough
    case KEY_LEFT: 
      handleServoKey(keyCode, keyEvent);
      break;
    case KEY_UP:
      handleEngineKey(FORWARD, keyEvent);
      break;
    case KEY_DOWN:
      handleEngineKey(BACKWARD, keyEvent);
      break;
    case KEY_SPEED:
      // Only forward for now
      setEngineState(keyEvent > 0 ? FORWARD : OFF, keyEvent);
      break;
    case KEY_PLAYMUSIC:
      MusicState = PLAYING;
      break;
    default:
      keyValid = false;
      break;
  }
}

void handleInput() {
  uint8_t buf[2];
  uint8_t buflen = sizeof(buf);

  if (!driver.recv(buf, &buflen)) { // Non-blocking
    return;
  }

  // Message with a good checksum received, dump it.
  //driver.printBuffer("Got:", buf, buflen);

  int keyCode = buf[0];
  int keyEvent = buf[1];
  Serial.print("Got key: ");
  Serial.print(keyCode);
  Serial.print(" event: ");
  Serial.println(keyEvent);

  handleKey(keyCode, keyEvent);
}

void setServo(int degrees) {
#ifdef USE_SERVO
  myservo.attach(SERVO_PIN);

  degrees = clamp(degrees);
  analogWrite(LED_PIN, degrees);
  // servo2 uses MAX_PULSE_WIDTH and MIN_PULSE_WIDTH for max/min degrees.
  myservo.write(map(degrees, 0, 180, MIN_PULSE_WIDTH, MAX_PULSE_WIDTH));
  SetServoTime = CurTime;

#endif
}

void servoSleep() {
  if ((CurTime - SERVO_TIMEOUT) < SetServoTime) {
    return;
  }
  myservo.detach();
  SetServoTime = 0;
}

void loop()
{
  CurTime = millis();
  fadeLed();
  handleMusic();
  handleInput();
  servoSleep();
}
