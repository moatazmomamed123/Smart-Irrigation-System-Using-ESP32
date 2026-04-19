#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <DHT.h>

// 1. إعدادات الواي فاي
#define WIFI_SSID "test"
#define WIFI_PASSWORD "123456789"

// 2. إعدادات Firebase (تم التصحيح بناءً على الصورة)
#define DATABASE_URL "https://smartplant-9df1d-default-rtdb.europe-west1.firebasedatabase.app"
#define DATABASE_SECRET "cITEBGl5Xy1gxZJpfDgYrJzR8ds52j1bdtNhlt5z"

// 3. تعريف الدبابيس
#define DHTPIN 4
#define DHTTYPE DHT11
#define SOIL_PIN 34
#define LDR_PIN 35
#define PUMP_PIN 26

DHT dht(DHTPIN, DHTTYPE);
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
bool autoMode = false; 

void setup() {
  Serial.begin(115200);
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW); 
  dht.begin();
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");

  config.database_url = DATABASE_URL;
  config.signer.tokens.legacy_token = DATABASE_SECRET;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("System Ready.");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) return;

  // قراءة الحساسات
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  int soilValue = analogRead(SOIL_PIN);
  int ldrValue = analogRead(LDR_PIN);
  int soilPercent = map(soilValue, 4095, 1500, 0, 100);
  soilPercent = constrain(soilPercent, 0, 100);
  int lightPercent = map(ldrValue, 0, 4095, 100, 0); // تحويل الإضاءة لنسبة

  // 1. جلب الإعدادات من Firebase
  // جلب وضع الري التلقائي (auto_irrigation)
  if (Firebase.RTDB.getBool(&fbdo, "/controls/auto_irrigation")) {
    autoMode = fbdo.boolData();
  }

  // 2. منطق التحكم في المضخة
  if (autoMode) {
    // وضع تلقائي: تعتمد على الحساس (أقل من 30%)
    if (soilPercent < 30) digitalWrite(PUMP_PIN, HIGH);
    else digitalWrite(PUMP_PIN, LOW);
  } else {
    // وضع يدوي: تعتمد على زر الـ Pump في Firebase
    if (Firebase.RTDB.getBool(&fbdo, "/controls/pump")) {
      if (fbdo.boolData()) digitalWrite(PUMP_PIN, HIGH);
      else digitalWrite(PUMP_PIN, LOW);
    }
  }

  // 3. رفع البيانات إلى Firebase كل 5 ثوانٍ
  if (millis() - sendDataPrevMillis > 5000) {
    sendDataPrevMillis = millis();

    FirebaseJson updateData;
    // تحديث البيانات في مجلد sensor_data ليتوافق مع تطبيق الفلتر
    updateData.set("sensor_data/temp", t);
    updateData.set("sensor_data/moisture", soilPercent);
    updateData.set("sensor_data/light", lightPercent);
    updateData.set("sensor_data/pump_state", digitalRead(PUMP_PIN) == HIGH);

    if (Firebase.RTDB.updateNode(&fbdo, "/", &updateData)) {
      Serial.println("Dashboard Updated!");
    } else {
      Serial.println(fbdo.errorReason());
    }
  }
}