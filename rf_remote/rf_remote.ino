
#include <AceButton.h>
using namespace ace_button;

#include <RH_ASK.h>
#ifdef RH_HAVE_HARDWARE_SPI
#include <SPI.h> // Not actually used but needed to compile
#endif

RH_ASK driver(1000);

// RF Key payload:
// [ byte | byte ]
// [KEYCODE | KEY_EVENT]
//
// KEY_EVENT maps to AceButton events, except for SPEED keycode where the state is just a speed from 0 to 255.
//
// Keycodes:
enum KeyCodes {
  KEY_LEFT,
  KEY_RIGHT,
  KEY_UP,
  KEY_DOWN,
  KEY_PLAYMUSIC,
  KEY_SPEED
};

const int LEFT_BUTTON_PIN = 8;
const int RIGHT_BUTTON_PIN = 6;
const int FORWARD_BUTTON_PIN = 4;
const int BACKWARD_BUTTON_PIN = 2;

AceButton button_left(LEFT_BUTTON_PIN, HIGH, KEY_LEFT);
AceButton button_right(RIGHT_BUTTON_PIN, HIGH, KEY_RIGHT);
AceButton button_forward(FORWARD_BUTTON_PIN, HIGH, KEY_UP);
AceButton button_backward(BACKWARD_BUTTON_PIN, HIGH, KEY_DOWN);

// Forward reference to prevent Arduino compiler becoming confused.
void handleEvent(AceButton*, uint8_t, uint8_t);

void setup()
{
#ifdef RH_HAVE_SERIAL
    Serial.begin(115200);   // Debugging only
#endif

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LEFT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(RIGHT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(FORWARD_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BACKWARD_BUTTON_PIN, INPUT_PULLUP);


  ButtonConfig* buttonConfig = ButtonConfig::getSystemButtonConfig();
  buttonConfig->setEventHandler(handleEvent);
  buttonConfig->setFeature(ButtonConfig::kFeatureClick);
  buttonConfig->setFeature(ButtonConfig::kFeatureDoubleClick);
  buttonConfig->setFeature(ButtonConfig::kFeatureLongPress);
  buttonConfig->setFeature(ButtonConfig::kFeatureSuppressAfterClick);
  buttonConfig->setFeature(ButtonConfig::kFeatureSuppressAfterLongPress);
  buttonConfig->setClickDelay(399);

  if (!driver.init())
#ifdef RH_HAVE_SERIAL
    Serial.println("init failed");
#else
    ;
#endif
}

const int RF_KEY_SEND_MAX_RATE = 100; // ms

const int SPEED_READ_THRESHOLD = 25;
int LastSpeed = 0;  // 0-255

unsigned long CurMillis = 0;
unsigned long LastSentKeyTime = 0;

bool MusicSecretKeyPressed = false;

void loop()
{
  CurMillis = millis();
  //readSpeed();
  button_left.check();
  button_right.check();
  button_forward.check();
  button_backward.check();

  if (LastSentKeyTime + 250 < CurMillis) {
    digitalWrite(LED_BUILTIN, LOW);
  }
}



void readSpeed() {
  int curSpeed = map(analogRead(A0),0 , 1023, 0, 255);

  if (abs(LastSpeed - curSpeed) < SPEED_READ_THRESHOLD) {
    return;
  }

  // To always be able to turn speed off, lets clamp to zero within the threshold value.
  if (curSpeed < SPEED_READ_THRESHOLD)
    curSpeed = 0;

  Serial.print("Curspeed: ");
  Serial.println(curSpeed);
  LastSpeed = curSpeed;
  sendKey(KEY_SPEED, curSpeed);
}

bool isMusicKey(int keyCode, int keyEvent) {
  if (keyCode != KEY_LEFT && keyCode != KEY_RIGHT) {
    return false;
  }

  bool curPressed = (keyEvent == AceButton::kEventPressed);

  Serial.print("Ispressed ");
  Serial.println(curPressed);
  // Play music only if both left and right are pressed
  if (MusicSecretKeyPressed && curPressed) {
    MusicSecretKeyPressed = false;
    return true;
  }

  MusicSecretKeyPressed = curPressed;
  return false;
}

void sendKey(int keyCode, int keyEvent) {
  // Packet send time is about 72ms with payload size of 2 bytes
  LastSentKeyTime = CurMillis;
  char buf[2];
  buf[0] = char(keyCode);
  buf[1] = char(keyEvent);
  buf[2] = '\0';

  driver.send((uint8_t *)buf, 2);
  driver.waitPacketSent();
}

void handleEvent(AceButton* button, uint8_t eventType, uint8_t buttonState) {

  // Print out a message for all events, for both buttons.
  Serial.print(F("handleEvent(): Pin: "));
  Serial.print(button->getPin());
  Serial.print(F("; eventType: "));
  Serial.print(eventType);
  Serial.print(F("; buttonState: "));
  Serial.println(buttonState);

  digitalWrite(LED_BUILTIN, HIGH);

  if (isMusicKey(button->getId(), eventType)) {
    Serial.println("Send PLAYMUSIC");
    sendKey(KEY_PLAYMUSIC, 0);
    return;
  }
  sendKey(button->getId(), eventType);
}
