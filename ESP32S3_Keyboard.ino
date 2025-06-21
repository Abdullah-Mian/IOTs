#include "USB.h"
#include "USBHIDKeyboard.h"

USBHIDKeyboard Keyboard;

enum State {
  WAIT_START,
  PRESS_WIN_R,
  TYPE_NOTEPAD,
  PRESS_ENTER,
  WAIT_NOTEPAD,
  TYPE_RANDOM_WORDS
};

State currentState = WAIT_START;
unsigned long lastAction = 0;
bool sequenceStarted = false;

const char* randomWords[] = {
  "hello", "world", "quick", "brown", "fox", "jumps", "over", "lazy", "dog",
  "computer", "keyboard", "mouse", "screen", "window", "door", "house", "tree",
  "water", "fire", "earth", "wind", "magic", "power", "energy", "light"
};

void setup() {
  Keyboard.begin();
  USB.begin();
  randomSeed(analogRead(0));
  lastAction = millis();
}

void loop() {
  unsigned long currentTime = millis();
  
  switch(currentState) {
    case WAIT_START:
      if (!sequenceStarted && currentTime - lastAction > 3000) {
        currentState = PRESS_WIN_R;
        sequenceStarted = true;
        lastAction = currentTime;
      }
      break;
      
    case PRESS_WIN_R:
      if (currentTime - lastAction > 100) {
        Keyboard.press(KEY_LEFT_GUI);  // Windows key
        Keyboard.press('r');
        delay(50);
        Keyboard.releaseAll();
        currentState = TYPE_NOTEPAD;
        lastAction = currentTime;
      }
      break;
      
    case TYPE_NOTEPAD:
      if (currentTime - lastAction > 500) {
        Keyboard.print("notepad");
        currentState = PRESS_ENTER;
        lastAction = currentTime;
      }
      break;
      
    case PRESS_ENTER:
      if (currentTime - lastAction > 300) {
        Keyboard.press(KEY_RETURN);
        delay(50);
        Keyboard.release(KEY_RETURN);
        currentState = WAIT_NOTEPAD;
        lastAction = currentTime;
      }
      break;
      
    case WAIT_NOTEPAD:
      if (currentTime - lastAction > 2000) {  // Wait for Notepad to open
        currentState = TYPE_RANDOM_WORDS;
        lastAction = currentTime;
      }
      break;
      
    case TYPE_RANDOM_WORDS:
      if (currentTime - lastAction > 100) {  // Fast typing - every 100ms
        int wordIndex = random(0, sizeof(randomWords)/sizeof(randomWords[0]));
        Keyboard.print(randomWords[wordIndex]);
        Keyboard.print(" ");
        lastAction = currentTime;
      }
      break;
  }
}