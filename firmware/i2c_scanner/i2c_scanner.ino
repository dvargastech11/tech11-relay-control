/*
  I2C Address Scanner
  ---------------------
  Flash this temporarily (separate from the main firmware) to identify
  the real I2C address of each MCP23017 board as you adjust its DIP
  switches. Connect ONE board at a time to avoid ambiguity.

  Wiring: SDA -> GPIO21, SCL -> GPIO22, VCC -> 3.3V, GND -> GND

  Open Serial Monitor at 115200 baud after uploading.
*/

#include <Wire.h>

void setup() {
  Wire.begin(); // SDA=GPIO21, SCL=GPIO22 (ESP32 defaults)
  Serial.begin(115200);
  delay(500);
  Serial.println("\nI2C Scanner starting...");
}

void loop() {
  Serial.println("Scanning...");
  int found = 0;

  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();

    if (error == 0) {
      Serial.printf("Device found at address 0x%02X\n", address);
      found++;
    }
  }

  if (found == 0) {
    Serial.println("No I2C devices found - check wiring.");
  } else {
    Serial.printf("Scan complete. %d device(s) found.\n", found);
  }

  delay(3000);
}
