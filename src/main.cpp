#include <Arduino.h>
#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif
#include <Firebase_ESP_Client.h>
#include <HTTPClient.h>

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#define WIFI_SSID "Rabin"
#define WIFI_PASSWORD "12345678"

#define API_KEY "AIzaSyCITbn7Mh9QPGlKQEE16SSBHP8ad9bzOAQ"

#define DATABASE_URL "https://soil-d5d51-default-rtdb.firebaseio.com"

FirebaseData fbdo;
FirebaseData motorfbdo;

FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
int count = 0;
bool signupOK = false;
bool isMotorOn = false;
bool autoMotorOnOff = false;
int soilMoistureValue = 0;
int thresholdValue = 500;

void setMotor(bool isOn)
{
  isMotorOn = isOn;
  if (Firebase.ready() && signupOK)
  {
    if (Firebase.RTDB.setBool(&fbdo, "motor/isOn", isOn))
    {
      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo.dataPath());
      Serial.println("TYPE: " + fbdo.dataType());
    }
    else
    {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }
  }
}

void setAutoMotorOnOff(bool isOn)
{
  autoMotorOnOff = isOn;
  if (Firebase.ready() && signupOK)
  {
    if (Firebase.RTDB.setBool(&fbdo, "motor/autoOnOff", isOn))
    {
      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo.dataPath());
      Serial.println("TYPE: " + fbdo.dataType());
    }
    else
    {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }
  }
}

void setCurrentMoistureValue(int value)
{
  soilMoistureValue = value;
  if (Firebase.ready() && signupOK)
  {
    if (Firebase.RTDB.setInt(&fbdo, "soilMoisture/currentValue", value))
    {
      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo.dataPath());
      Serial.println("TYPE: " + fbdo.dataType());
    }
    else
    {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }
  }
}

String callAPI(const String &url)
{
  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;
    http.begin(url);

    int httpResponseCode = http.GET();

    if (httpResponseCode > 0)
    {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      String payload = http.getString();
      Serial.println(payload);
      return payload;
    }
    else
    {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
      return "Something went wrong";
    }

    http.end();
  }
  else
  {
    Serial.println("WiFi Disconnected");
    return "WiFi Disconnected";
  }
}

String getDate()
{
  return callAPI("http://worldtimeapi.org/api/timezone/Asia/Kathmandu");
}

void setMoistureDataWithDateAndTimeAsJson(int value)
{
  if (Firebase.ready() && signupOK)
  {
    FirebaseJson json;
    json.add("value", value);
    json.add("datetime", getDate());
    if (Firebase.RTDB.pushJSON(&fbdo, "soilMoisture/wholeData", &json))
    {
      Serial.println("PASSED");
      Serial.println("PATH: " + fbdo.dataPath());
      Serial.println("TYPE: " + fbdo.dataType());
    }
    else
    {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }
  }
}

void readData()
{
  if (Firebase.RTDB.getBool(&motorfbdo, "motor/autoOnOff"))
  {
    if (motorfbdo.boolData())
    {
      Serial.println(motorfbdo.boolData());
      autoMotorOnOff = motorfbdo.boolData();
    }
    else
    {
      autoMotorOnOff = false;
    }
  }

  if (Firebase.RTDB.getBool(&fbdo, "motor/isOn"))
  {
    if (fbdo.boolData())
    {
      Serial.println("true");
      isMotorOn = fbdo.boolData();
    }
    else
    {
      Serial.println("false");
      isMotorOn = false;
    }
  }
  else
  {
    isMotorOn = false;
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
  }
  long int randomValue = random(0, 1000);
  setCurrentMoistureValue(randomValue);
  setMoistureDataWithDateAndTimeAsJson(randomValue);
}

void connectToWiFi()
{
  Serial.begin(115200);
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

void setupFirebase()
{
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", ""))
  {
    Serial.println("ok");
    signupOK = true;
  }
  else
  {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}
void autoLogic()
{
  if (autoMotorOnOff)
  {
    if (soilMoistureValue > thresholdValue)
    {
      Serial.println("Soil is dry");
      setMotor(true);
    }
    else
    {
      Serial.println("Soil is wet");
      setMotor(false);
    }
  }
  else
  {
    Serial.println("Auto mode is off");
  }
}

void setup()
{
  connectToWiFi();
  setupFirebase();
  pinMode(LED_BUILTIN, OUTPUT);
  setAutoMotorOnOff(true);
  setMotor(false);
}

void loop()
{
  if (Firebase.ready())
  {
    if (signupOK)
    {
      if (millis() - sendDataPrevMillis > 1500 || sendDataPrevMillis == 0)
      {
        sendDataPrevMillis = millis();
        Serial.println("Time:" + millis());
        readData();
        autoLogic();

        if (isMotorOn)
        {
          digitalWrite(LED_BUILTIN, HIGH);
        }
        else
        {
          digitalWrite(LED_BUILTIN, LOW);
        }
      }
    }
    else
    {
      Serial.println("not signed up yet");
    }
  }
  else
  {
    Serial.println("Firebase not ready");
  }
}
