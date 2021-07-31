
#include <ezBuzzer.h>
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
const int MOTOR_ON_PIN = 12;
const int BUZZER_PIN = 3;
const int LED_PIN = 5;           // the PWM pin the LED is attached to
const int SERVO_PIN = 6; // Was 9 for Servo lib (timer 1)
const int RF_RECEIVE_PIN = 11;  // pin for the RF receiver

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

enum MusicStates {
  STOPPED,
  STOPPING,
  PLAYING
};

MusicStates MusicState = STOPPED;

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
ServoTimer2 myservo;
RH_ASK driver(1000);

void setup()
{
  pinMode(LED_PIN, OUTPUT);
  pinMode(MOTOR_FW_PIN, OUTPUT);
  pinMode(MOTOR_BW_PIN, OUTPUT);
  pinMode(MOTOR_ON_PIN, OUTPUT);
  digitalWrite(MOTOR_FW_PIN, LOW);
  digitalWrite(MOTOR_BW_PIN, LOW);
  digitalWrite(MOTOR_ON_PIN, LOW);

  Serial.begin(9600);
 
  myservo.attach(SERVO_PIN);  // attaches the servo on pin 9 to the servo object
  setServo(ROT_CENTER);

  if (!driver.init()) {
    Serial.println("init failed");
  }
}

unsigned long CurTime = 0;

int brightness = MIN_BRIGHTNESS;    // how bright the LED is
int fadeAmount = 5;    // how many points to fade the LED by
unsigned long LedFadeStart = 0;


int rot = ROT_CENTER;
int rot_max = 180;
int rot_min = 0;
int PendingKey = -1;
unsigned long KeyPressedTimeStamp = 0; // if this is set to something else than -1 rotation will be reset after (millis() - KeyPressedTimeStamp) > KEY_RELEASED_TIMEOUT

int clamp(int value) {
  value = min(value, rot_max);
  return max(value, rot_min);  
}

void setEngineState(enum EngineStates state) {
  // If already in state, toggle off.

  switch (state) {
    case FORWARD:
      Serial.println("FORWARD");
      digitalWrite(MOTOR_BW_PIN, LOW);
      digitalWrite(MOTOR_FW_PIN, HIGH);
      digitalWrite(MOTOR_ON_PIN, HIGH);
      break;
    case BACKWARD:
      Serial.println("BACKWARD");
      digitalWrite(MOTOR_FW_PIN, LOW);
      digitalWrite(MOTOR_BW_PIN, HIGH);
      digitalWrite(MOTOR_ON_PIN, HIGH);
      break;
    case OFF: // fallthrough
    default:
      Serial.println("OFF");
      digitalWrite(MOTOR_FW_PIN, LOW);
      digitalWrite(MOTOR_BW_PIN, LOW);      
      digitalWrite(MOTOR_ON_PIN, LOW);
  }

  EngineState = state;
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

  if (MusicState == STOPPING && buzzer.getState() == BUZZER_IDLE) {
    buzzer.stop();
    MusicState = STOPPED;
    myservo.attach(SERVO_PIN);
    return;
  }

  if (MusicState == PLAYING && buzzer.getState() == BUZZER_IDLE) {
    // Servo is using same timer (2) as buzzer, so lets disable it while music is playing.
    myservo.detach();
    setEngineState(OFF); // Always turn off motor during music playback
    int length = sizeof(noteDurations) / sizeof(int);
    buzzer.playMelody(melody, noteDurations, length); // playing
    MusicState = STOPPING;
  }
}

void handleKey(int keyCode, int keyEvent) {
  bool keyValid = true;

  // Save pressed timestamp to handle missing released
  if (keyEvent == AceButton::kEventPressed) {
    KeyPressedTimeStamp = CurTime;
    PendingKey = keyCode;
  }

  if (keyEvent == AceButton::kEventReleased) {
    KeyPressedTimeStamp = 0;
    PendingKey = -1;
  }

  switch (keyCode) {
    case KEY_RIGHT:
      rot = keyEvent == AceButton::kEventPressed ? rot_max : ROT_CENTER;
      break;
    case KEY_LEFT:
      rot = keyEvent == AceButton::kEventPressed ? rot_min : ROT_CENTER;
      break;
    case KEY_SPEED:
      // Only forward for now
      setEngineState(keyEvent > 0 ? FORWARD : OFF);
      PendingKey = -1; // No key release for SPEED keycode.
      break;
    case KEY_PLAYMUSIC:
      MusicState = PLAYING;
      PendingKey = -1;
      break;
    default:
      keyValid = false;
      break;
  }
  rot = clamp(rot);
  setServo(rot);
}


void handleInput() {
  // Send key released if keyreleased is not received within KEY_RELEASED_TIMEOUT
  if (PendingKey != -1 && (CurTime - KeyPressedTimeStamp) > KEY_RELEASED_TIMEOUT) {
    KeyPressedTimeStamp = 0;
    KeyPressedTimeStamp = 0;
    handleKey(PendingKey, AceButton::kEventReleased);
  }
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
  // servo2 uses MAX_PULSE_WIDTH and MIN_PULSE_WIDTH for max/min degrees.
  myservo.write(map(degrees, 0, 180, MIN_PULSE_WIDTH, MAX_PULSE_WIDTH));
}


void loop()
{
  CurTime = millis();
  fadeLed();
  handleMusic();
  handleInput();
}
