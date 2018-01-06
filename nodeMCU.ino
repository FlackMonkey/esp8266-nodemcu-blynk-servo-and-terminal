
/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial
#include <FS.h>
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

#include <WiFiManager.h>
#include <ArduinoJson.h>


#include <Servo.h>   // Include the library
#include <ArduinoOTA.h>

Servo servo_L_B;
Servo servo_L_K;

WidgetLED led1(V4);

#define servo_L_B_PIN D2
#define servo_L_K_PIN D3
//#define led_PIN D7

int servo_L_B_RAW = 1023 / 2;
int servo_L_B_angle = 90;

int servo_L_K_RAW = 1023 / 2;
int servo_L_K_angle = 90;

int led_state = LOW;

// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
char auth[34] = "";

//TODO move to flash.
String configFileURI = "/cfg.json";

//flag for saving data
bool shouldSaveConfig = false;
//
bool configLoaded = false;
//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void configDelete(String fileName) {
  if (SPIFFS.exists(fileName)) {
    Serial.println("Will reset Config");
    SPIFFS.remove(fileName);
    Serial.println("Reset Config: done. Rebooting...");
    delay(3000);
    ESP.reset();
  } else {
    Serial.println("Reset Config: fail. No file found. Rebooting...");
    delay(3000);
    ESP.reset();
  }

}

bool stringContainsResetSequence(String str){
  return str.indexOf("cfg rst now") != -1;
}

String latestMessage = "";

// Attach virtual serial terminal to Virtual Pin V1
WidgetTerminal terminal(V9);

// You can send commands from Terminal to your hardware. Just use
// the same Virtual Pin as your Terminal Widget
BLYNK_WRITE(V9)
{
  // Send it back
  terminal.print("You said:");
  terminal.write(param.getBuffer(), param.getLength());
  terminal.println();

  Serial.print("Terminal message: |||");
  Serial.print(param.asStr());
  Serial.println("|||");
  bool doReset = stringContainsResetSequence(String(param.asStr()));

  if (doReset) {
    terminal.print("Will reset device");
  }
  // Ensure everything is sent
  terminal.flush();

  if (doReset) {
    configDelete(configFileURI);
  }
}






/* Reads slider in the Blynk app and writes the value to "servo_L_B_RAW" variable */
BLYNK_WRITE(V0)
{
  servo_L_B_RAW = param.asInt();
}
BLYNK_WRITE(V3)
{
  servo_L_K_RAW = param.asInt();
}

/* Display servo position on Blynk app */
BLYNK_READ(V1)
{
  Blynk.virtualWrite(V1, servo_L_B_angle);
}
BLYNK_READ(V2)
{
  Blynk.virtualWrite(V2, servo_L_K_angle);
}
//-----------------------------------------
BLYNK_WRITE(V4)
{
  led_state = param.asInt();
}
BLYNK_READ(V5)
{
  Blynk.virtualWrite(V5, led_state);
}
//-----------------------------------------

void setup()
{
  // Debug console
  Serial.begin(9600);
  Serial.println("mounting FS..."); 

  //
  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists(configFileURI)) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open(configFileURI, "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          //strcpy(mqtt_server, json["mqtt_server"]);
          //strcpy(mqtt_port, json["mqtt_port"]);

          bool useCharArray = false;

          if (useCharArray) {
            String tkn = json["blynk_token"];
            tkn.toCharArray(auth, tkn.length());

//            String ssid_ = json["ssid"];
//            ssid_.toCharArray(ssid, ssid_.length());
//
//            String pass_ = json["ssid_pass"];
//            pass_.toCharArray(pass, pass_.length());
          } else {
            strcpy(auth, json["blynk_token"]);
//            strcpy(ssid, json["ssid"]);
//            strcpy(pass, json["ssid_pass"]);
          }
          configLoaded = true;


        if (String(auth).length() < 32) {
          Serial.println("Fornat FS. Reboot...");
           SPIFFS.format();
           delay(500);
           ESP.reset();
          }
        Serial.println("JSON -> global vars:");  
        Serial.println(auth);
//        Serial.println(ssid);
//        Serial.println(pass);
        Serial.println("-------");

      } else {
        Serial.println("failed to load json config");
        configLoaded = false;
      }
    }
  } else {
    Serial.println("No config found");
    configLoaded = false;
  }
} else {
  Serial.println("failed to mount FS");
}
//end read
// The extra parameters to be configured (can be either global or just in the setup)
// After connecting, parameter.getValue() will get you the configured value
// id/name placeholder/prompt default length
//WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
//WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
WiFiManagerParameter custom_blynk_token("blynk", "blynk token", auth, 34);

//WiFiManager
//Local intialization. Once its business is done, there is no need to keep it around
WiFiManager wifiManager;

//set config save notify callback
wifiManager.setSaveConfigCallback(saveConfigCallback);

//set static ip
//wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

//add all your parameters here
//wifiManager.addParameter(&custom_mqtt_server);
wifiManager.addParameter(&custom_blynk_token);

//reset settings - for testing
//wifiManager.resetSettings();

//set minimu quality of signal so it ignores AP's under that quality
//defaults to 8%
//wifiManager.setMinimumSignalQuality();

//sets timeout until configuration portal gets turned off
//useful to make it all retry or go to sleep
//in seconds
//wifiManager.setTimeout(120);

//fetches ssid and pass and tries to connect
//if it does not connect it starts an access point with the specified name
//here  "AutoConnectAP"
//and goes into a blocking loop awaiting configuration

if (configLoaded) {
  wifiManager.autoConnect("ESP8266COnfigFail");
  if (!wifiManager.autoConnect("AutoConnectAP", "password")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }
} else {
  wifiManager.startConfigPortal("OnDemandAP");
}
//if you get here you have connected to the WiFi
Serial.println("connected...yeey :)");

//read updated parameters
//strcpy(mqtt_server, custom_mqtt_server.getValue());
//strcpy(mqtt_port, custom_mqtt_port.getValue());
strcpy(auth, custom_blynk_token.getValue());

//save the custom parameters to FS
if (shouldSaveConfig) {
  Serial.println("saving config");
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  //json["mqtt_server"] = mqtt_server;
  //json["mqtt_port"] = mqtt_port;
  json["blynk_token"] = auth;
  //
//  String newWIfi = WiFi.SSID().c_str();
//  String newPass = WiFi.psk().c_str();
//
//  newWIfi.toCharArray(ssid, newWIfi.length());
//  newPass.toCharArray(pass, newPass.length());
//
//  Serial.print("New ssid: ");
//  Serial.println(ssid);
//  Serial.print("New pass: ");
//  Serial.println(pass);
//  json["ssid"] = newWIfi;
//  json["ssid_pass"] = newPass;
  //

  File configFile = SPIFFS.open(configFileURI, "w");
  if (!configFile) {
    Serial.println("failed to open config file for writing");
  }

  json.printTo(Serial);
  json.printTo(configFile);
  configFile.close();
  //end save
}

Serial.println("local ip");
Serial.println(WiFi.localIP());

Serial.println("Blink will start with params:");
Serial.println(auth);
//Serial.println(ssid);
//Serial.println(pass);

Blynk.config(auth);
Blynk.connect(60);
//  Blynk.begin(auth, ssid, pass);
// You can also specify server:
//Blynk.begin(auth, ssid, pass, "blynk-cloud.com", 8442);
//Blynk.begin(auth, ssid, pass, IPAddress(192,168,1,100), 8442);

servo_L_B.attach(servo_L_B_PIN);
servo_L_K.attach(servo_L_K_PIN);

//  pinMode(led_PIN, OUTPUT);

String ipAddress = WiFi.localIP().toString();

terminal.println(F("Blynk v" BLYNK_VERSION ": Device started"));
terminal.println("IP:" + ipAddress + "." );
terminal.println("Auth:" + String(auth));
terminal.println(F("-------------"));
terminal.flush();

//  ArduinoOTA.setPassword((const char *)"123");

ArduinoOTA.onStart([]() {
  Serial.println("Start");  //  "Начало OTA-апдейта"
  terminal.println("OTA START");
  terminal.flush();

});
ArduinoOTA.onEnd([]() {
  Serial.println("\nEnd");  //  "Завершение OTA-апдейта"
  terminal.println("\nEnd");
  terminal.flush();
});
ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
  Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  String progressPercent =  String((progress / (total / 100)));
  terminal.println("Progress: " + progressPercent + "%/" + total);
  terminal.flush();
});
ArduinoOTA.onError([](ota_error_t error) {
  Serial.printf("Error[%u]: ", error);
  if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
  //  "Ошибка при аутентификации"
  else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
  //  "Ошибка при начале OTA-апдейта"
  else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
  //  "Ошибка при подключении"
  else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
  //  "Ошибка при получении данных"
  else if (error == OTA_END_ERROR) Serial.println("End Failed");
  //  "Ошибка при завершении OTA-апдейта"
});
ArduinoOTA.begin();
Serial.println("Ready");  //  "Готово"
Serial.print("IP address: ");  //  "IP-адрес: "
Serial.println("Auth:" + String(auth));
Serial.println(WiFi.localIP());
}

void mapValueAndWriteServo(int raw, int angle, Servo servo) {
  angle = map(raw, 0, 1023, 0, 180);  // Map the pot reading to an angle from 0 to 180
  servo.write(angle);
}

void loop()
{
  ArduinoOTA.handle();
  Blynk.run();


  loop_servo_move();

  if (Serial.available() > 0) {
    // read the incoming byte:
    String txt = Serial.readString();
    Serial.print("COM msg: ");
    Serial.println(txt);
    bool doReset = stringContainsResetSequence(txt);
    if (doReset){
      configDelete(configFileURI);
      }
    
  }


}

void loop_servo_move() {
  //  mapValueAndWriteServo(servo_L_B_RAW,servo_L_B_angle,servo_L_B);
  //  mapValueAndWriteServo(servo_L_K_RAW,servo_L_K_angle,servo_L_K);
  //
  servo_L_B_angle = map(servo_L_B_RAW, 0, 1023, 0, 180);  // Map the pot reading to an angle from 0 to 180
  servo_L_B.write(servo_L_B_angle);
  //
  servo_L_K_angle = map(servo_L_K_RAW, 0, 1023, 0, 180);  // Map the pot reading to an angle from 0 to 180
  servo_L_K.write(servo_L_K_angle);
  //  digitalWrite(led_PIN, led_state);
  delay(50);
}
