




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

#define servo_L_B_PIN D7
#define servo_L_K_PIN D8

bool distance_m_on = false;
#define distance_m_trig_pin D5
#define distance_m_echoPin D6
long distance_m_duration;
int distance_m_distance;

//#define led_PIN D7

int servo_L_B_RAW = 1023 / 2;
int servo_L_B_angle = 90;

int servo_L_K_RAW = 1023 / 2;
int servo_L_K_angle = 90;

int PIR_MOTION_DETECTION_PIN = 5; // Input for HC-S501, GPIO15 / D5
int PIR_MOTION_DETECTION_VALUE = 0;
int PIR_MOTION_DETECTION_PIN_TO_VIRTUAL_PIN = 0; //0-4
bool PIR_MOTION_DETECTION_ON = true;

int DIGITAL_TO_VIRTUAL_PINS_VALUES[5] = {0, 0, 0, 0, 0};

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

String COMMAND_DISTANCE_METER = "distance meter trig";
String COMMAND_SHIFTOUT_SET_VALUE = "shiftout";
String COMMAND_CONFIG_RESET = "cfg rst now";

String latestMessage = "";

// Attach virtual serial terminal to Virtual Pin V1
WidgetTerminal terminal(V9);
//

////------------------------------------------------------------
//#include <Wire.h>
//#include <Adafruit_PWMServoDriver.h>
//
//// called this way, it uses the default address 0x40
//Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();
//// you can also call it with a different address you want
////Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x41);
//// you can also call it with a different address and I2C interface
////Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(&Wire, 0x40);
////------------------------------------------------------------
//// Depending on your servo make, the pulse width min and max may vary, you
//// want these to be as small/large as possible without hitting the hard stop
//// for max range. You'll have to tweak them as necessary to match the servos you
//// have!
//#define SERVOMIN  150 // this is the 'minimum' pulse length count (out of 4096)
//#define SERVOMAX  600 // this is the 'maximum' pulse length count (out of 4096)
//
//// our servo # counter
//uint8_t servonum = 0;

////------------------------------------------------------------
#include <Wire.h>
#include <servo_PCA9685.h>

#define MAX_SERVOS  16
/*
   ESP8266 I2C - pins
   SDA: 4 - D2
   SCL: 5 - D1
*/
servo_PCA9685 theServo_PCA9685  = servo_PCA9685();

int valz_PCA9685[16] = {
  0, 0, 0, 0
  , 0, 0, 0, 0
  , 0, 0, 0, 0
  , 0, 0, 0, 0
};
uint8_t servonum = 0;
////------------------------------------------------------------


#define MIN_PULSE_WIDTH       650
#define MAX_PULSE_WIDTH       2350
#define DEFAULT_PULSE_WIDTH   1500
#define FREQUENCY             50

// you can use this function if you'd like to set the pulse length in seconds
// e.g. setServoPulse(0, 0.001) is a ~1 millisecond pulse width. its not precise!
//void setServoPulse(uint8_t n, double pulse) {
//  double pulselength;
//
//  pulselength = 1000000;   // 1,000,000 us per second
//  pulselength /= 60;   // 60 Hz
//  Serial.print(pulselength); Serial.println(" us per period");
//  pulselength /= 4096;  // 12 bits of resolution
//  Serial.print(pulselength); Serial.println(" us per bit");
//  pulse *= 1000000;  // convert to us
//  pulse /= pulselength;
//  Serial.println(pulse);
//  pwm.setPWM(n, 0, pulse);
//}
//------------------------------------------------------------




#include <LedMatrix.h> //https://github.com/squix78/MAX7219LedMatrix

bool ledMatrix_on = false;
//LedMatrix ledMatrix;
//= LedMatrix(1, 2);

//------------------------------------------------------------
bool shiftRegister_ON = false;
int shiftRegister_pin_latch[2] =  {2, 5};
int shiftRegister_pin_clock[2] =  {3, 6};
int shiftRegister_pin_data[2] = {4, 7};
int shiftRegister_value[2] = {5, 5};
//------------------------------------------------------------


void terminal_show_instructions() {
  terminal.println("Available commands:");
  String indent = " ";
  terminal.println(indent + COMMAND_CONFIG_RESET + " - will do blynk token reset");
  terminal.println(indent + COMMAND_DISTANCE_METER + " - will on/off distancee tracking on D5(trig), D6(echo) V10-V11");
  terminal.println(indent + COMMAND_SHIFTOUT_SET_VALUE + " X V - will set shiftout register number X to value V");
  terminal.println("Extra pins:");
  terminal.println("servo_L_B_PIN D2 - VO write, v1 read");
  terminal.println("servo_L_K_PIN D3 - v3 write, v2 read");
}




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

void configBoolUpdate(String configRootKey, bool value) {
  if (SPIFFS.begin()) {
    Serial.println("configBoolUpdate: mounted file system");
    if (SPIFFS.exists(configFileURI)) {
      //file exists, reading and loading
      Serial.println("configBoolUpdate : reading config file");
      File configFile = SPIFFS.open(configFileURI, "r");
      if (configFile) {
        Serial.println("configBoolUpdate: opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          long boolWas = json[configRootKey];
          long boolNew = value ? 1 : 0;
          Serial.print("Will update(");
          Serial.print(configRootKey);
          Serial.print(") from : ");
          Serial.print(boolWas);
          Serial.print(" to: ");
          Serial.println(boolNew);
          json[configRootKey] = boolNew;

          File configFile = SPIFFS.open(configFileURI, "w");
          if (!configFile) {
            Serial.println("configBoolUpdate: failed to open config file for writing");
          }
          json.printTo(Serial);
          json.printTo(configFile);
          configFile.close();

        } else {
          Serial.println("configBoolUpdate : No config found");
          configLoaded = false;
        }
      } else {
        Serial.println("configBoolUpdate: failed to mount FS");
      }
    }
  }
}



bool stringStartsWithSequence(String source, String mayBeAPart) {
  return source.indexOf(mayBeAPart) == 0;
}

bool commandContainsResetSequence(String str) {
  return stringStartsWithSequence(str, COMMAND_CONFIG_RESET);
}
bool commandContainsDistanceMeterTriggerSequence(String str) {
  return stringStartsWithSequence(str, COMMAND_DISTANCE_METER);
}


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
  bool doReset = commandContainsResetSequence(String(param.asStr()));
  bool distanceMeterTrigger = commandContainsDistanceMeterTriggerSequence(String(param.asStr()));
  bool hasShiftoutUpdate = stringStartsWithSequence(String(param.asStr()), COMMAND_SHIFTOUT_SET_VALUE);
  if (doReset) {
    terminal.print("Will reset device");
  }

  if (hasShiftoutUpdate) {
    /*
      char* command = strtok(param.asStr(), " ");
      int argIDx = 0;
      while (command != 0) {
      terminal.println(" argumnent ");
      terminal.print(argIDx);
      terminal.print(command);
      termnial.println("");
      argIDx++;
      }
    */
  }
  if (distanceMeterTrigger) {
    terminal.print("Will trigger (on<->off) distance meter");
    distance_m_on = !distance_m_on;
    configBoolUpdate("distance_meter_on", distance_m_on);
  }

  if (stringStartsWithSequence(String(param.asStr()), "help")) {
    terminal_show_instructions();
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

BLYNK_READ(V11) {
  Blynk.virtualWrite(V11, distance_m_distance);
}
//-----------------------------------------
BLYNK_WRITE(V20) {
  valz_PCA9685[0] = param.asInt();
}
BLYNK_READ(V20) {
  Blynk.virtualWrite(V20, valz_PCA9685[0]);
}

BLYNK_WRITE(V21) {
  valz_PCA9685[1] = param.asInt();
}
BLYNK_READ(V21) {
  Blynk.virtualWrite(V21, valz_PCA9685[1]);
}

BLYNK_WRITE(V22) {
  valz_PCA9685[2] = param.asInt();
}
BLYNK_READ(V22) {
  Blynk.virtualWrite(V22, valz_PCA9685[2]);
}

BLYNK_WRITE(V23) {
  valz_PCA9685[3] = param.asInt();
}
BLYNK_READ(V23) {
  Blynk.virtualWrite(V23, valz_PCA9685[3]);
}

BLYNK_WRITE(V24) {
  valz_PCA9685[4] = param.asInt();
}
BLYNK_READ(V24) {
  Blynk.virtualWrite(V24, valz_PCA9685[4]);
}
//-----------------------------------------

void setup_json_cfg_wifi() {
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

          long distance_meter_on = json["distance_meter_on"];
          Serial.print("Config.distance_meter_on: ");
          Serial.println(distance_meter_on);
          distance_m_on = distance_meter_on == 1;

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

}

void setup_OTA() {
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
  //  TODO check this to have connection
  //  ArduinoOTA.setPassword((const char *)"123");
}


void setup()
{
  // Debug console
  Serial.begin(9600);

  setup_json_cfg_wifi();

  Serial.println("local ip");
  Serial.println(WiFi.localIP());

  Serial.println("Blink will start with params:");
  Serial.println(auth);

  Blynk.config(auth);
  Blynk.connect(60);
  //  Blynk.begin(auth, ssid, pass);
  // You can also specify server:
  //Blynk.begin(auth, ssid, pass, "blynk-cloud.com", 8442);
  //Blynk.begin(auth, ssid, pass, IPAddress(192,168,1,100), 8442);

  servo_L_B.attach(servo_L_B_PIN);
  servo_L_K.attach(servo_L_K_PIN);

  String ipAddress = WiFi.localIP().toString();
  terminal.println(F("Blynk v" BLYNK_VERSION ": Device started"));
  terminal.println("IP:" + ipAddress + "." );
  terminal.println("Auth:" + String(auth));
  terminal.println(F("-------------"));
  terminal.flush();

  setup_OTA();

  Serial.println("Ready");  //  "Готово"
  Serial.print("IP address: ");  //  "IP-адрес: "
  Serial.println("Auth:" + String(auth));
  Serial.println(WiFi.localIP());

  pinMode(distance_m_trig_pin, OUTPUT);
  pinMode(distance_m_echoPin, INPUT);
  terminal_show_instructions();

  pinMode(PIR_MOTION_DETECTION_PIN, INPUT);


  //----------------------------------------------------------
  //pwm.begin();
  //pwm.setPWMFreq(60);  // Analog servos run at ~60 Hz updates

  //pwm.begin();
  //pwm.setPWMFreq(FREQUENCY);
  //----------------------------------------------------------

  //----------------------------------------------------------
  theServo_PCA9685.begin();
  //----------------------------------------------------------


  //----------------------------------------------------------
  if (ledMatrix_on) {
    int ledMatrix_pin = 2;
    int ledMatrixCount = 1;
    /*
    ledMatrix = LedMatrix(ledMatrixCount, ledMatrix_pin);
    ledMatrix.init();
    ledMatrix.setIntensity(4); // range is 0-15
    ledMatrix.setText("MAX7219 Demo");
    */
  }
  //----------------------------------------------------------

}

int pulseWidth(int angle)
{
  int pulse_wide, analog_value;
  pulse_wide   = map(angle, 0, 180, MIN_PULSE_WIDTH, MAX_PULSE_WIDTH);
  analog_value = int(float(pulse_wide) / 1000000 * FREQUENCY * 4096);
  Serial.println(analog_value);
  return analog_value;
}



void mapValueAndWriteServo(int raw, int angle, Servo servo) {
  angle = map(raw, 0, 1023, 0, 180);  // Map the pot reading to an angle from 0 to 180
  servo.write(angle);
}

void loop_distance_measure() {

  if (distance_m_on) {
    // Clears the distance_m_trig_pin
    digitalWrite(distance_m_trig_pin, LOW);
    delayMicroseconds(2);

    // Sets the distance_m_trig_pin on HIGH state for 10 micro seconds
    digitalWrite(distance_m_trig_pin, HIGH);
    delayMicroseconds(10);
    digitalWrite(distance_m_trig_pin, LOW);

    // Reads the distance_m_echoPin, returns the sound wave travel time in microseconds
    distance_m_duration = pulseIn(distance_m_echoPin, HIGH);

    // Calculating the distance
    distance_m_distance = distance_m_duration * 0.034 / 2;

    int theTime = millis();

    // Prints the distance on the Serial Monitor
    Serial.print("Distance: ");
    Serial.print(distance_m_distance);
    Serial.print(" time: ");
    Serial.println(theTime);

    terminal.print("Distance (sm): ");
    terminal.print(distance_m_distance);
    terminal.print(" time: ");
    terminal.println(theTime);
    terminal.flush();

    delay(1000);
  }
}

void loop_serial_msg() {
  if (Serial.available() > 0) {
    // read the incoming byte:
    String txt = Serial.readString();
    Serial.print("COM msg: ");
    Serial.println(txt);
    bool doReset = commandContainsResetSequence(txt);
    if (doReset) {
      configDelete(configFileURI);
    }

  }
}

int digital_pin_value(int pinNumber) {
  return digitalRead(pinNumber);
}

void virtual_pin_value_set(int vPinIdx, int value) {
  DIGITAL_TO_VIRTUAL_PINS_VALUES[vPinIdx] = value;
}
void loop_pir_motion_detection() {
  if (PIR_MOTION_DETECTION_ON) {
    PIR_MOTION_DETECTION_VALUE = digital_pin_value(PIR_MOTION_DETECTION_PIN);

    virtual_pin_value_set(PIR_MOTION_DETECTION_PIN_TO_VIRTUAL_PIN, PIR_MOTION_DETECTION_VALUE);
  }
}

void loop_servo_PCA_9685() {
  for (uint16_t servoIdx = 0; servoIdx < 4; servoIdx++) {
    theServo_PCA9685.setPWM(servoIdx, 0, valz_PCA9685[servoIdx]);
    delay(50);
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

void sendBitsToShift(int idx, byte value) {
  digitalWrite(shiftRegister_pin_latch[idx], LOW);
  shiftOut(shiftRegister_pin_data[idx], shiftRegister_pin_clock[idx], MSBFIRST, value);
  digitalWrite(shiftRegister_pin_latch[idx], HIGH);
}
void loop_led_matrix() {
  if (ledMatrix_on) {
    /*
    ledMatrix.clear();
    ledMatrix.scrollTextLeft();
    ledMatrix.drawText();
    ledMatrix.commit(); // commit transfers the byte buffer to the displays
    delay(200);
    */
  }
}

void loop_shift_register() {
  if (shiftRegister_ON) {
    for (int i = 0; i < 2; i++) {
      sendBitsToShift(i, shiftRegister_value[i]);
    }
  }

}


void loop()
{
  ArduinoOTA.handle();
  Blynk.run();

  loop_servo_move();
  loop_distance_measure();
  loop_pir_motion_detection();
  loop_servo_PCA_9685();

  loop_shift_register();
  loop_led_matrix();

  loop_serial_msg();



}
