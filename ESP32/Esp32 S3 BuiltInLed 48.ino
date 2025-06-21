#define PIN_RGB_LED 48
#define RGB_BUILTIN (SOC_GPIO_PIN_COUNT + PIN_RGB_LED)
#define RGB_BRIGHTNESS 64  // You can adjust brightness (max 255)

void setup() {
  // No initialization needed for RGB LED
}

void loop() {
  digitalWrite(RGB_BUILTIN, HIGH);  // Turn the RGB LED white
  delay(1000);
  digitalWrite(RGB_BUILTIN, LOW);   // Turn the RGB LED off
  delay(1000);
  
  // RGB Color sequence
  rgbLedWrite(RGB_BUILTIN, RGB_BRIGHTNESS, 0, 0);  // Red
  delay(1000);
  rgbLedWrite(RGB_BUILTIN, 0, RGB_BRIGHTNESS, 0);  // Green
  delay(1000);
  rgbLedWrite(RGB_BUILTIN, 0, 0, RGB_BRIGHTNESS);  // Blue
  delay(1000);
  rgbLedWrite(RGB_BUILTIN, 0, 0, 0);              // Off
  delay(1000);
}