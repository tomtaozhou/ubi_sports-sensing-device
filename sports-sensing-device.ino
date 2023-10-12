#include <M5StickC.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <base64.h>

const int httpsPort = 443;

const char* host = "example.com";
const char* wpUsername = "your_wordpress_username"; 
const char* wpAppPassword = "your_generated_app_password";

WiFiManager wifiManager;
WiFiClientSecure client;

float bodyWeight = 70.0;
int stepCount = 0;
int sitUpCount = 0;
float caloriesBurned = 0;
float previousAy = 0;
float previousAz = 0;
bool dataChanged = false;

unsigned long lastCheckTime = 0;
const int CHECK_INTERVAL = 10000;
String lastPostedStatus;

const int MAX_RETRIES = 3;

void showMessage(String msg) {
  M5.Lcd.setRotation(1);
  M5.Lcd.setCursor(0, 10, 2);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.println(msg);
}

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());
  showMessage("Please connect to this AP\nto config WiFi\nSSID: " + myWiFiManager->getConfigPortalSSID());
}

void setupWiFi() {  
  wifiManager.setAPCallback(configModeCallback);
  bool doManualConfig = false;
  showMessage("Push POWER button to enter Wifi config.");
  for(int i=0 ; i<200 ; i++) {
    M5.update();
    if (M5.BtnB.wasPressed()) {
      doManualConfig = true;
      break;
    }
    delay(10);
  }

  if (doManualConfig) {
    Serial.println("wifiManager.startConfigPortal()");
    if (!wifiManager.startConfigPortal()) {
      Serial.println("startConfigPortal() connect failed!");
      showMessage("Wi-Fi failed.");
      return;
    }
  } else {
    showMessage("Wi-Fi connecting...");
    Serial.println("wifiManager.autoConnect()");
    if (!wifiManager.autoConnect()) {
      Serial.println("autoConnect() connect failed!");
      showMessage("Wi-Fi failed.");
      return;
    }
  }

  String IPaddress = WiFi.localIP().toString();
  showMessage("Wi-Fi connected.\nIP Address: " + IPaddress);
}

void postToWordPress(String status) {
  if (!client.connect(host, httpsPort)) {
    Serial.println("Connection to WordPress failed!");
    return;
  }

  String postData = "title=M5StickC Activity&content=" + status + "&status=publish";
  client.println("POST /wp-json/wp/v2/posts HTTP/1.1");
  client.println("Host: " + String(host));
  client.println("User-Agent: M5StickC");
  String basicAuthCredentials = wpUsername + String(":") + wpAppPassword;
  client.println("Authorization: Basic " + base64::encode(basicAuthCredentials));
  client.println("Content-Type: application/x-www-form-urlencoded");
  client.print("Content-Length: ");
  client.println(postData.length());
  client.println();
  client.println(postData);

  // Read the full response from WordPress for debugging
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
        break;
    }
    Serial.println(line);
  }
}

void setup() {
  M5.begin();
  M5.Lcd.begin();
  M5.Lcd.setRotation(1);
  M5.IMU.Init();

  Serial.begin(9600);
  client.setInsecure();

  setupWiFi();
}

void loop() {
  float ax, ay, az, gx, gy, gz;
  M5.IMU.getAccelData(&ax, &ay, &az);
  M5.IMU.getGyroData(&gx, &gy, &gz);

  if (ay > 0.5 && previousAy < 0.5) {
    stepCount++;
    caloriesBurned += (0.035 * bodyWeight / 30);
    dataChanged = true;
  }

  if (az > 0.5 && previousAz < 0.5 && gy > 0.5) {
    sitUpCount++;
    caloriesBurned += 0.8;
    dataChanged = true;
  }

  previousAy = ay;
  previousAz = az;

  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.println("Steps: " + String(stepCount));
  M5.Lcd.println("Sit-ups: " + String(sitUpCount));
  M5.Lcd.println("Calories: " + String(caloriesBurned, 2));

  if (millis() - lastCheckTime >= CHECK_INTERVAL) {
    lastCheckTime = millis();

    String currentStatus = "Steps: " + String(stepCount) + ", Sit-ups: " + String(sitUpCount) + ", Calories: " + String(caloriesBurned, 2);

    if (currentStatus != lastPostedStatus) {
      postToWordPress(currentStatus);
      lastPostedStatus = currentStatus;
    }
  }

  delay(1000);
}
