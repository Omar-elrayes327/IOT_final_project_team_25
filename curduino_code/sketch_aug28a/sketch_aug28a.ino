#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <ArduinoJson.h>
#include <ESPSupabase.h>

// ===================== PINS =====================
#define LDR_PIN    23
#define LED_PIN    27
#define DHT_PIN     4
#define SERVO_PIN  14
#define GAS_PIN    35

// ===================== SENSORS =====================
DHT dht(DHT_PIN, DHT22);
Servo doorServo;
int doorState = 0;  // 0=Closed, 1=Open

// ===================== DISPLAY =====================
LiquidCrystal_I2C lcd(0x27, 16, 2); // ESP32 compatible library

// ===================== NETWORK =====================
const char* ssid = "omar";
const char* password = "123456789";

// ===================== Supabase =====================
const char* supabaseUrl = "https://qroeztvhjoydenryhxfl.supabase.co";
const char* supabaseKey = "sb_secret_is_dY-YDUEZK-1x2Aj3cGg_GsPM3U-p";
Supabase supabase;

// ===================== CLOUD / MQTT =====================
const char* mqttServer = "broker.hivemq.com";
const int mqttPort = 1883;
WiFiClient espClient;
PubSubClient client(espClient);

// ===================== CONTROL STATES =====================
bool ledState = false;
bool ledManual = false; 
unsigned long lastManualTime = 0;

// ===================== KEYPAD =====================
const byte ROWS = 4; 
const byte COLS = 4; 
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {32, 33, 25, 26}; 
byte colPins[COLS] = {12, 13, 15, 19}; 
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

String passwordKey = "1306";  //الرقم السري
String inputKey = "";
bool isEnteringPassword = false;  

// ===================== SUPABASE INTERVAL =====================
unsigned long lastSupabase = 0;
const unsigned long supabaseInterval = 15000; // 15 seconds

// ===================== MQTT CALLBACK =====================
void callback(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, payload, length);

  if (!err) {
    if (String(topic) == "esp32/led/control") {
      if (doc.containsKey("led")) {
        int v = doc["led"];
        ledManual = true;
        lastManualTime = millis();
        ledState = (v != 0);
        digitalWrite(LED_PIN, ledState ? HIGH : LOW);
      } else if (doc.containsKey("state")) {
        String s = doc["state"].as<String>();
        ledManual = true;
        lastManualTime = millis();
        if (s == "ON") { digitalWrite(LED_PIN, HIGH); ledState = true; }
        else if (s == "OFF") { digitalWrite(LED_PIN, LOW); ledState = false; }
      }
    }
    if (String(topic) == "esp32/door/control") {
      if (doc.containsKey("door")) {
        String cmd = doc["door"].as<String>();
        if (cmd == "OPEN" || cmd == "open") { doorServo.write(90); doorState = 1; }
        else if (cmd == "CLOSE" || cmd == "close") { doorServo.write(0); doorState = 0; }
      }
    }
    return;
  }

  String message = "";
  for (unsigned int i = 0; i < length; i++) message += (char)payload[i];
  message.trim();

  if (String(topic) == "esp32/led/control") {
    ledManual = true;
    lastManualTime = millis();
    if (message == "LED_ON" || message == "ON") { digitalWrite(LED_PIN, HIGH); ledState = true; }
    else if (message == "LED_OFF" || message == "OFF") { digitalWrite(LED_PIN, LOW); ledState = false; }
  }
  if (String(topic) == "esp32/door/control") {
    if (message == "OPEN") { doorServo.write(90); doorState = 1; }
    else if (message == "CLOSE") { doorServo.write(0); doorState = 0; }
  }
}

// ===================== MQTT RECONNECT =====================
void reconnect() {
  while (!client.connected()) {
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      client.subscribe("esp32/led/control");
      client.subscribe("esp32/door/control");
    } else {
      delay(5000);
    }
  }
}

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(LDR_PIN, INPUT);
  pinMode(GAS_PIN, INPUT);

  dht.begin();
  delay(2000);
  doorServo.attach(SERVO_PIN);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("ESP32 System");
  lcd.setCursor(0,1);
  lcd.print("Starting...");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);

  // Initialize Supabase
  supabase.begin(supabaseUrl, supabaseKey);
}

// ===================== LOOP =====================
int lastTemp = 0;
int lastHum = 0;

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  // Keypad door control
  char key = keypad.getKey();
  if (key) {
    isEnteringPassword = true;  
    if (key == '#') {
      if (inputKey == passwordKey) { 
        doorServo.write(90); doorState = 1; 
        lcd.clear(); lcd.print("Door OPEN"); 
      } else { 
        doorServo.write(0); doorState = 0; 
        lcd.clear(); lcd.print("Wrong Password"); 
      }
      inputKey = ""; 
      isEnteringPassword = false;  
    } else if (key == '*') { 
      inputKey = ""; 
      lcd.clear(); lcd.print("Cleared"); 
      isEnteringPassword = false; 
    } else { 
      inputKey += key; 
      lcd.clear(); 
      lcd.print("Input: "); 
      lcd.print(inputKey); 
    }
  }
  if (isEnteringPassword) { delay(200); return; }

  // Read sensors
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  int tInt = isnan(t) ? lastTemp : round(t);
  int hInt = isnan(h) ? lastHum : round(h);
  lastTemp = tInt; lastHum = hInt;

  int ldrValue = digitalRead(LDR_PIN);
  int gasValue = analogRead(GAS_PIN);

  // Reset manual LED after 5s
  if (ledManual && millis() - lastManualTime > 5000) ledManual = false;

  // LED control
  if (!ledManual) {
    if (ldrValue == HIGH) { digitalWrite(LED_PIN, HIGH); ledState = true; }
    else { digitalWrite(LED_PIN, LOW); ledState = false; }
  }

  // Gas alert
  bool gasAlert = gasValue > 2000;

  // Serial output
  Serial.print("Temp: "); Serial.print(tInt);
  Serial.print(" Hum: "); Serial.print(hInt);
  Serial.print(" LED: "); Serial.print(ledState ? "ON" : "OFF");
  Serial.print(" Door: "); Serial.print(doorState ? "OPEN" : "CLOSE");
  Serial.print(" LDR: "); Serial.print(ldrValue ? "DARK" : "LIGHT");
  Serial.print(" Gas: "); Serial.println(gasValue);

  // Publish MQTT
  StaticJsonDocument<256> out;
  out["T"] = tInt;
  out["H"] = hInt;
  out["LDR"] = ldrValue ? "DARK" : "LIGHT";
  out["LED"] = ledState ? 1 : 0;
  out["DOOR"] = doorState;
  out["Gas"] = gasValue;
  char buf[256];
  size_t n = serializeJson(out, buf);
  client.publish("esp32/sensors/data", buf, n);

  // Insert to Supabase every 15 sec
  if (millis() - lastSupabase > supabaseInterval) {
    lastSupabase = millis();
    String tableNames[4] = {"dht22","gas-sensor","led","photoresistor-sensor"};
    String jsonDataArray[4] = {
      "{\"temperature\": \"" + String(tInt) + "\", \"humidity\": \"" + String(hInt) + "\"}",
      "{\"ppm\": \"" + String(gasValue) + "\"}",
      "{\"statue\": \"" + String(ledState ? 1 : 0) + "\"}",
      "{\"Initial_light_value\": \"" + String(ldrValue) + "\"}"
    };

    for (int i = 0; i < 4; i++) {
      int response = supabase.insert(tableNames[i], jsonDataArray[i], false);
      if (response == 200) {
        Serial.print(tableNames[i]);
        Serial.println(" data inserted successfully!");
      } 
    }
  }

  // LCD
  // LCD
 lcd.clear();
 if (gasAlert) {
   lcd.setCursor(0,0); lcd.print("!! GAS ALERT !!");
   lcd.setCursor(0,1); lcd.print("Gas: "); lcd.print(gasValue);
 } else {
   
   lcd.setCursor(0,0);
   lcd.print("T:"); lcd.print(tInt);
   lcd.print(" H:"); lcd.print(hInt);
   lcd.print(" G:"); lcd.print(gasValue); 

   
   lcd.setCursor(0,1);
   lcd.print("LED:"); lcd.print(ledState ? "ON " : "OFF");
   lcd.print(" D:"); lcd.print(doorState ? "OPEN" : "CLOSE");
 }

  delay(2000);
}
