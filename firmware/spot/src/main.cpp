#include <Arduino.h>
#include <BLEAdvertising.h>
#include <BLEDevice.h>

namespace {

constexpr char SPOT_NAME[] = "SPEAK-OUTSIDE-001";

// BLE advertising interval units are 0.625 ms. 0x0140 = 320 units = 200 ms.
constexpr uint16_t ADVERTISING_INTERVAL = 0x0140;
constexpr uint32_t STATUS_INTERVAL_MS = 3000;

uint32_t lastStatusMs = 0;

void printStatus() {
  BLEAddress address = BLEDevice::getAddress();
  Serial.print("[spot] BLE address: ");
  Serial.println(address.toString().c_str());
  Serial.print("[spot] advertising as ");
  Serial.println(SPOT_NAME);
}

}  // namespace

void setup() {
  Serial.begin(115200);

  // Give native USB CDC a chance to enumerate, but don't block forever
  // because the Spot must continue advertising even with no computer attached.
  const uint32_t waitStartedMs = millis();
  while (!Serial && millis() - waitStartedMs < 5000) {
    delay(20);
  }

  Serial.println();
  Serial.println("SPEAK! Prototype 001 - Back Door Spot");

  BLEDevice::init(SPOT_NAME);

  BLEAdvertisementData advertisementData;
  advertisementData.setName(SPOT_NAME);

  // Small vendor payload: ASCII "SPK" + prototype version + Spot number.
  std::string manufacturerData;
  manufacturerData.push_back('S');
  manufacturerData.push_back('P');
  manufacturerData.push_back('K');
  manufacturerData.push_back(0x01);  // protocol version
  manufacturerData.push_back(0x01);  // Spot 001
  advertisementData.setManufacturerData(manufacturerData);

  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->setAdvertisementData(advertisementData);
  advertising->setScanResponse(false);
  advertising->setMinInterval(ADVERTISING_INTERVAL);
  advertising->setMaxInterval(ADVERTISING_INTERVAL);

  BLEDevice::startAdvertising();

  printStatus();
  Serial.println("[spot] copy the BLE address above into collar/include/prototype_config.h");
  lastStatusMs = millis();
}

void loop() {
  const uint32_t now = millis();
  if (now - lastStatusMs >= STATUS_INTERVAL_MS) {
    printStatus();
    lastStatusMs = now;
  }
  delay(20);
}
