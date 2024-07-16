#include <Arduino.h>

// digital read pin 15
#define PIN 32

void setup() {
  Serial.begin(115200);
  pinMode(PIN, INPUT);
  Serial.println("Hello World");
  pinMode(LED_BUILTIN, OUTPUT);
}

uint16_t status = 0;
bool isPressed = false;
bool isReleased = true;
int numTouch = 0;
unsigned long timeTouch = 0;
unsigned long timeout = 0;

void loop() {
  status = analogRead(PIN);
  if (status >= 2048 && isReleased) {
    isPressed = true;
    isReleased = false;
    numTouch++;
    Serial.print("Touch: ");
    Serial.println(numTouch);
    timeTouch = millis();
  } else if (status < 2048 && isPressed) {
    isPressed = false;
    isReleased = true;
  }

  if(status >= 2048){
    digitalWrite(LED_BUILTIN, HIGH);
  }else{
    digitalWrite(LED_BUILTIN, LOW);
  }

  // if 2 second time between two touches then reset the counter
  if (millis() - timeTouch > 2000 && numTouch != 0) {
    Serial.println("Reset conter");
    numTouch = 0;
  }
  // teleplot the value
  // Serial.print(">Status:");
  // Serial.println(status);
  delay(10);
}

