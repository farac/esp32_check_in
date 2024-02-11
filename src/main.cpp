#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <NimBLEDevice.h>

// Provide the token generation process info.
#include "addons/TokenHelper.h"
// Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

#define LED 2
// Wifi
#define WIFI_SSID "1"
#define WIFI_PASSWORD "1"
// Firebase
#define FIREBASE_WEB_API_KEY "1"
#define DATABASE_URL "1"
// BLE
#define BLE_SERVICE_UUID "1"
#define BLE_CHARACTERISTIC_UUID "1"

FirebaseData firebaseData;

FirebaseAuth firebaseAuth;
FirebaseConfig firebaseConfig;

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

NimBLECharacteristic *pCharacteristic;
bool dataChanged = false;
uint16_t studentId;

void connectWifi();
// check if student with passed id is in table of students on fb
bool checkStudentExists(uint16_t);
bool recordCheckIn(uint16_t);

class IdWrittenToCallbacks : public NimBLECharacteristicCallbacks
{
  void onWrite(NimBLECharacteristic *pCharacteristic)
  {
    Serial.println("Characteristic changed by client:\nIs now:");
    uint16_t recData = *pCharacteristic->getValue().data();
    Serial.println(recData);

    studentId = recData;
    dataChanged = true;
  }
};

void setup()
{
  Serial.begin(115200);

  connectWifi();

  // BLE related
  NimBLEDevice::init("ESP32_Check_In");
  NimBLEServer *pServer = BLEDevice::createServer();
  NimBLEService *pService = pServer->createService(BLE_SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
      BLE_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
  pCharacteristic->setCallbacks(new IdWrittenToCallbacks());
  pService->start();

  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
  if (NimBLEDevice::startAdvertising())
  {
    Serial.println("BLE advertising started");
  }

  // firebase related
  firebaseConfig.api_key = FIREBASE_WEB_API_KEY;
  firebaseConfig.database_url = DATABASE_URL;
  if (Firebase.signUp(&firebaseConfig, &firebaseAuth, "", ""))
  {
    Serial.println("Firebase login ok.");
    signupOK = true;
  }
  else
  {
    Serial.printf("Firebase login not ok: %s\n", firebaseConfig.signer.signupError.message.c_str());
  }

  firebaseConfig.token_status_callback = tokenStatusCallback;

  Firebase.begin(&firebaseConfig, &firebaseAuth);
  Firebase.reconnectWiFi(true);
}

void loop()
{
  if (Firebase.ready() && signupOK && dataChanged && (millis() - sendDataPrevMillis > 1500 || sendDataPrevMillis == 0))
  {
    dataChanged = false;
    if (!checkStudentExists(studentId))
    {
      Serial.println("Student not found in database!");
      Serial.println("Setting charateristic to 0");
      pCharacteristic->setValue(0);
    }
    else
    {
      Serial.println("Student found in database!");
      Serial.println("Sending to db:");
      recordCheckIn(studentId);
      Serial.println("Setting charateristic to 1");
      pCharacteristic->setValue(1);
    }
  }
}

void connectWifi()
{
  // connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();
}

bool recordCheckIn(uint16_t id)
{
  FirebaseJson json;
  json.add("student_id", id);
  bool push_1 = Firebase.RTDB.pushJSON(&firebaseData, "/timbravanja/", &json);
  String path = "/timbravanja/";
  path.concat(firebaseData.pushName());
  path.concat("/time");
  bool push_2 = Firebase.RTDB.setTimestamp(&firebaseData, path);
  return (push_1 && push_2);
}

bool checkStudentExists(uint16_t id)
{
  Serial.println(id);
  std::string testPath = "/students/";
  testPath.append(std::to_string(id));

  Serial.println("Checking for student at path:");
  Serial.println(testPath.c_str());

  return Firebase.RTDB.get(&firebaseData, testPath);
}
