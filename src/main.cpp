#include <Arduino.h>
#include <BleKeyboard.h>

BleKeyboard Keyboard;

void setup() {

  Serial.begin(115200);
  Serial.println("Started");

  Keyboard.setDeviceName("BlackWidow Lite");
  Keyboard.setDeviceManufacturer("Razer USA, Ltd");
  Keyboard.setProductId(0x0235); // logitech (0xc535) razer (0x0235)
  Keyboard.setVendorId(0x1532); // logitech (0x046d) razer (0x1532)
  Keyboard.begin(); // 0x0A02 = Screen

  delay(5000);
  Keyboard.println("keyboard ready");
  delay(150);
  Keyboard.write(KEY_RETURN);
  delay(150);
  Keyboard.write(KEY_RETURN);
  delay(150);
  Keyboard.write(KEY_RETURN);
  delay(150);
  Keyboard.print("done...");
  delay(150);

  Keyboard.print("|...");
  delay(150);

  Keyboard.press(KEY_BACKSPACE);
  Keyboard.releaseAll();
  delay(150);

  Keyboard.press(KEY_LEFT_ALT);
  Keyboard.press(KEY_TAB);
  Keyboard.releaseAll();
  delay(150);
}

void loop() {
  Serial.println(".");
  
  delay(1000);
}