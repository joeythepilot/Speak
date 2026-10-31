#include <Arduino.h>
#include <BLEAdvertising.h>
#include <BLEDevice.h>

namespace {

constexpr char SPOT_NAME[] = "SPEAK-OUTSIDE-001";

// BLE advertising interval units are 0.625 ms. 0x0140 = 320 units = 200 ms.
constexpr uint16_t ADVERTISING_INTERVAL = 0x0140;

void printAddress() {
  const BLEAddress address = BLEDevice::getAddress();
  Serial.print("[spot] BLE address: ");
  Serial.println(address.toString().c_str());
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println("SPEAK! Prototype 001 - Back Door Spot");

  BLEDevice::init(SPOT_NAME);
  printAddress();

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

  Serial.print("[spot] advertising as ");
  Serial.println(SPOT_NAME);
  Serial.println("[spot] copy the BLE address above into collar/include/prototype_config.h");
}

void loop() {
  delay(1000);
}
