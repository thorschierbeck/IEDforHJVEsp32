/**
 * WiFiManager advanced demo, contains advanced configurartion options
 * Implements TRIGGEN_PIN button press, press for ondemand configportal, hold for 3 seconds for reset settings.
 */

//#include "Api.h"
//#include "ApiController.h"
#include <TinyGPS++.h>
#include <TinyGPSPlus.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <esp_wifi.h>
#include <ArduinoJson.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>


#define TRIGGER_PIN 0
#define DETONATOR_PIN 19
#define DETECTOR_PIN 12
#define SOUND_PIN 14
//#define SOUND_ENABLE_PIN 27
#define BATTERY_PIN 32
#define TEST_PIN 25
#define RXD2 16
#define TXD2 17

TinyGPSPlus gps;
 // wifimanager can run in a blocking mode or a non blocking mode
 // Be sure to know how to process loops with no delay() if using non blocking
bool wm_nonblocking = false; // change to true to use non blocking

WiFiManager wm; // global wm instance
WiFiManagerParameter custom_field; // global param ( for non blocking w params )
WebServer server(80);

// JSON data buffer
StaticJsonDocument<250> jsonDocument;
char buffer[250];

long timeperiode = 0;
long periode = 30000;

int Id = 1;
IPAddress IpAddress;// = "";
bool MotionDetected = false;
bool Armed = false;
String Message = "Ok";
double ProcentBat = 0;
int RRSI = -100;
double Longitude = 0;
double Latitude = 0;
double Altitude = 0;
int Satellites = 0;
bool SoundEnabled = false;
String Name;

//bool soundenabled = true;
bool runones = false;


void setup() {
    WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP  
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    delay(3000);
    Serial.println("\n Starting");

    pinMode(TRIGGER_PIN, INPUT_PULLUP);
    pinMode(DETONATOR_PIN, OUTPUT);
    pinMode(SOUND_PIN, OUTPUT);
    pinMode(DETECTOR_PIN, INPUT);
   // pinMode(SOUND_ENABLE_PIN, INPUT_PULLUP);
    digitalWrite(DETONATOR_PIN, LOW);
   
    digitalWrite(SOUND_PIN, SoundEnabled);
    
    
    String test = wm.getWiFiSSID();
    if (test == "")
    {
        wm.resetSettings(); // wipe settings
    }
    //else Serial.println(test + " MY test");
    // 

    if (wm_nonblocking) wm.setConfigPortalBlocking(false);

    // add a custom input field
    int customFieldLength = 40;


    // new (&custom_field) WiFiManagerParameter("customfieldid", "Custom Field Label", "Custom Field Value", customFieldLength,"placeholder=\"Custom Field Placeholder\"");

    // test custom html input type(checkbox)
    // new (&custom_field) WiFiManagerParameter("customfieldid", "Custom Field Label", "Custom Field Value", customFieldLength,"placeholder=\"Custom Field Placeholder\" type=\"checkbox\""); // custom html type

    // test custom html(radio)
    const char* custom_radio_str = "<br/><label for='customfieldid'>Custom Field Label</label><input type='radio' name='customfieldid' value='1' checked> One<br><input type='radio' name='customfieldid' value='2'> Two<br><input type='radio' name='customfieldid' value='3'> Three";
    new (&custom_field) WiFiManagerParameter(custom_radio_str); // custom html input

    wm.addParameter(&custom_field);
    wm.setSaveParamsCallback(saveParamCallback);

    // custom menu via array or vector
    // 
    // menu tokens, "wifi","wifinoscan","info","param","close","sep","erase","restart","exit" (sep is seperator) (if param is in menu, params will not show up in wifi page!)
    // const char* menu[] = {"wifi","info","param","sep","restart","exit"}; 
    // wm.setMenu(menu,6);
    std::vector<const char*> menu = { "wifi","info","param","sep","restart","exit" };
    wm.setMenu(menu);

    // set dark theme
    wm.setClass("invert");


    //set static ip
    // wm.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0)); // set static ip,gw,sn
    // wm.setShowStaticFields(true); // force show static ip fields
    // wm.setShowDnsFields(true);    // force show dns field always

    // wm.setConnectTimeout(20); // how long to try to connect for before continuing
    wm.setConfigPortalTimeout(120); // auto close configportal after n seconds
    // wm.setCaptivePortalEnable(false); // disable captive portal redirection
    // wm.setAPClientCheck(true); // avoid timeout if client connected to softap

    // wifi scan settings
    // wm.setRemoveDuplicateAPs(false); // do not remove duplicate ap names (true)
    // wm.setMinimumSignalQuality(20);  // set min RSSI (percentage) to show in scans, null = 8%
    // wm.setShowInfoErase(false);      // do not show erase button on info page
    // wm.setScanDispPerc(true);       // show RSSI as percentage not graph icons

    // wm.setBreakAfterConfig(true);   // always exit configportal even if wifi save fails

    bool res;
    // res = wm.autoConnect(); // auto generated AP name from chipid
    // res = wm.autoConnect("AutoConnectAP"); // anonymous ap
    res = wm.autoConnect("AutoConnectAP", "HjvOeb1234"); // password protected ap

    if (!res) {
        Serial.println("Failed to connect or hit timeout");
       // wm.resetSettings(); // wipe settings
        ESP.restart();
    }
    else {
        //if you get here you have connected to the WiFi    
        Serial.println(wm.getWLStatusString());
        RRSI = WiFi.RSSI();
        Serial.print("RRSI: ");
        Serial.print(RRSI);
        Serial.println(" dBm");
        IpAddress = WiFi.localIP();
        Serial.println(IpAddress);
        setup_OTA();
        setup_api();
        
             
    }
    InitSensor();
    Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
    
}

void setup_OTA()
{
    // Port defaults to 3232
   // ArduinoOTA.setPort(3232);

   // Hostname defaults to esp3232-[MAC]
   // ArduinoOTA.setHostname("myesp32");

   // No authentication by default
   // ArduinoOTA.setPassword("admin");

   // Password can be set with it's md5 value as well
   // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
   // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");
    ArduinoOTA
        .onStart([]() {
        String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
    else // U_SPIFFS
        type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
            })
        .onEnd([]() {
                Serial.println("\nEnd");
            })
                .onProgress([](unsigned int progress, unsigned int total) {
                Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
                    })
                .onError([](ota_error_t error) {
                        Serial.printf("Error[%u]: ", error);
                    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
                    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
                    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
                    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
                    else if (error == OTA_END_ERROR) Serial.println("End Failed");
                    });

                    ArduinoOTA.begin();

                    Serial.println("Ready");
                    Serial.print("IP address: ");
                    Serial.println(WiFi.localIP());
}


void loop() {
    if (wm_nonblocking) wm.process(); // avoid delays() in loop when non-blocking and other long running code  
    if (WiFi.status() == WL_CONNECTED)
    {
        ArduinoOTA.handle();
        server.handleClient();
    }
    if (MotionDetected == true && digitalRead(DETECTOR_PIN) == LOW) 
    {
        runones = false;
        MotionDetected = digitalRead(DETECTOR_PIN);
        Message = "Detector Ready";
    }


    if (SoundEnabled && MotionDetected)
        digitalWrite(SOUND_PIN, HIGH);
    else
        digitalWrite(SOUND_PIN, LOW);
    
    if (Armed && MotionDetected)
        digitalWrite(DETONATOR_PIN, HIGH);//Explode
    else 
        digitalWrite(DETONATOR_PIN, LOW);
    
    checkButton();
   
    if (!runones)
    {
        ReadBattery();
        RRSI = WiFi.RSSI();
        PostToDataToServer();
    }

    if (millis() >= timeperiode + periode)
    {
        
        timeperiode = millis() + periode;
        runones = false;

    }
    while (Serial2.available() > 0)
    {
        if (gps.encode(Serial2.read()))
        {
            displayInfo();
        }
    }
   /* while (Serial2.available() > 0)
        if (gps.encode(Serial2.read()))
            displayInfo();*/
    
    // put your main code here, to run repeatedly:
}

void InitSensor()
{
    dacWrite(TEST_PIN, 225);
    Message = "Detector Warming up";
    Serial.println(Message);
 
    delay(1000);
    while (digitalRead(DETECTOR_PIN) == HIGH)
    {
        delay(1000);
        
    }
    MotionDetected = digitalRead(DETECTOR_PIN);
   
   
    Message = "Detector ready";
    Serial.println(Message);

    digitalWrite(SOUND_PIN, HIGH);
    delay(100);
    digitalWrite(SOUND_PIN, LOW);
    delay(100);
    digitalWrite(SOUND_PIN, HIGH);
    delay(100);
    digitalWrite(SOUND_PIN, LOW);
    attachInterrupt(DETECTOR_PIN, MotionDetection, RISING);
}

void PostToDataToServer()
{
  
    Serial.println("Posting JSON data to server...");
    // Block until we are able to connect to the WiFi access point
    if (WiFi.status() == WL_CONNECTED) {

        HTTPClient http;

        http.begin("http://ied.schier-lunds.dk/api/IED");
      //  http.begin("http://192.168.1.27/api/IED");
        http.addHeader("Content-Type", "application/json");

        StaticJsonDocument<200> doc;
        // Add values in the document
        //
        doc["id"] = Id;
        doc["ipAddress"] = IpAddress;
        doc["motionDetected"] = MotionDetected;
        doc["armed"] = Armed;
        doc["soundEnabled"] = SoundEnabled;
        doc["message"] = Message;
        doc["batteryProcent"] = ProcentBat;
        doc["rrsi"] = RRSI;
        doc["latitude"] = Latitude;
        doc["longitude"] = Longitude;
        doc["altitude"] = Altitude;
        doc["satellites"] = Satellites;
        doc["name"] = Name;


        // Add an array.
        //
       /* JsonArray data = doc.createNestedArray("data");
        data.add(48.756080);
        data.add(2.302038);*/

        String requestBody;
        serializeJson(doc, requestBody);

        int httpResponseCode = http.POST(requestBody);

        if (httpResponseCode > 0) {

            String response = http.getString();

            Serial.println(httpResponseCode);
            Serial.println(response);

        }
        else {
            
            
            Serial.printf("Error occurred while sending HTTP POST: %s\n", http.errorToString(httpResponseCode).c_str());

        }
        runones = true;
    }
}

void checkButton() {
    // check for button press
    if (digitalRead(TRIGGER_PIN) == LOW) {
        // poor mans debounce/press-hold, code not ideal for production
        delay(50);
        if (digitalRead(TRIGGER_PIN) == LOW) {
            Serial.println("Button Pressed");
            // still holding button for 3000 ms, reset settings, code not ideaa for production
            delay(3000); // reset delay hold
            if (digitalRead(TRIGGER_PIN) == LOW) {
                Serial.println("Button Held");
                Serial.println("Erasing Config, restarting");
                wm.resetSettings();
                ESP.restart();
            }

            // start portal w delay
            Serial.println("Starting config portal");
            wm.setConfigPortalTimeout(120);

            if (!wm.startConfigPortal("AutoConnectAP")) {
                Serial.println("failed to connect or hit timeout");
                delay(3000);
                // ESP.restart();
            }
            else {
                //if you get here you have connected to the WiFi
                Serial.println("connected...yeey :)");
            }
        }
    }
}

String getParam(String name) {
    //read parameter from server, for customhmtl input
    String value;
    if (wm.server->hasArg(name)) {
        value = wm.server->arg(name);
    }
    return value;
}

void saveParamCallback() {
    Serial.println("[CALLBACK] saveParamCallback fired");
    Serial.println("PARAM customfieldid = " + getParam("customfieldid"));
}

void ReadBattery()
{
   ProcentBat = map(analogRead(BATTERY_PIN), 2850, 3480, 0, 100);
  
}

// Interrupt section
void MotionDetection()
{
    MotionDetected = digitalRead(DETECTOR_PIN); //pin goes high
    Serial.println("Detected");
    if (SoundEnabled)
        digitalWrite(SOUND_PIN, HIGH);
    else
        digitalWrite(SOUND_PIN, LOW);
    if (Armed && MotionDetected)
        digitalWrite(DETONATOR_PIN, HIGH);//Explode
    else 
        digitalWrite(DETONATOR_PIN, LOW);
    runones = false;
    Message = "Movement Detected";
}



//API section

void setup_api()
{
    
   
    server.on("/iedstatus", getIEDStatus);
    server.on("/ied", HTTP_POST, handlePost);

    // start server	 	 
    server.begin();
    Serial.println("WebServer paa port 80 startet!!");
   

}

void getIEDStatus()
{
    Serial.println("Get IEDStatus");
    create_json(Id,IpAddress,MotionDetected,Armed,Message, SoundEnabled, Name );
    server.send(200, "application/json", buffer);
    /*server.send(200, "application/json", "OK");*/
}

//void getEnv()
//{
//    Serial.println("Get env");
//    jsonDocument.clear();
//    add_json_object("temperature", temperature, "°C");
//    add_json_object("humidity", humidity, "%");
//    add_json_object("pressure", pressure, "mBar");
//    serializeJson(jsonDocument, buffer);
//    server.send(200, "application/json", buffer);
//   // server.send(200, "application/json", "OK");
//}

//API Post to the Esp32

void handlePost() 
{
    if (server.hasArg("plain") == false) {
        //handle error here
    }
    String body = server.arg("plain");
    deserializeJson(jsonDocument, body);

    //// Get RGB components
    Armed = jsonDocument["Armed"];
    SoundEnabled = jsonDocument["SoundEnabled"];
    const char* test = jsonDocument["Name"];
    Name = test;
    //Serial.println("Armeret has changed");

    //pixels.fill(pixels.Color(red, green, blue));
    //pixels.show();
    // Respond to the client
    create_json(Id, IpAddress, MotionDetected, Armed, Message, SoundEnabled, Name);
    server.send(200, "application/json", "{}");
    runones = false;
}

// Json handlers

void create_json(int id, IPAddress Ip, bool md, bool am, String ms, bool se, String n)
{
    jsonDocument.clear();
    jsonDocument["Id"] = id;
    jsonDocument["IpAddress"] = Ip;
    jsonDocument["MotionDetected"] = md;
    jsonDocument["Armed"] = am;
    jsonDocument["Message"] = ms;
    jsonDocument["SoundEnabled"] = se;
    jsonDocument["Name"] = n;
    serializeJson(jsonDocument, buffer);
}

void add_json_object(char* tag, float value, char* unit) {
    JsonObject obj = jsonDocument.createNestedObject();
    obj["type"] = tag;
    obj["value"] = value;
    obj["unit"] = unit;
}

void displayInfo()
{
    if (gps.location.isValid())
    {

        Latitude = gps.location.lat();
        Longitude = gps.location.lng();
        Altitude = gps.altitude.meters();
        Satellites = gps.satellites.value();
       /* Serial.print("Latitude: ");
        Serial.println(gps.location.lat(), 6);
        Serial.print("Longitude:gps ");
        Serial.println(gps.location.lng(), 6);
        Serial.print("Altitude: ");
        Serial.println(gps.altitude.meters());*/
    }
    else
    {
        Longitude = 0;
        Latitude = 0;
        Altitude = 0;

       // Serial.println("Location: Not Available");
    }

   /* Serial.print("Date: ");
    if (gps.date.isValid())
    {
        Serial.print(gps.date.month());
        Serial.print("/");
        Serial.print(gps.date.day());
        Serial.print("/");
        Serial.println(gps.date.year());
    }
    else
    {
        Serial.println("Not Available");
    }

    Serial.print("Time: ");
    if (gps.time.isValid())
    {
        if (gps.time.hour() < 10) Serial.print(F("0"));
        Serial.print(gps.time.hour());
        Serial.print(":");
        if (gps.time.minute() < 10) Serial.print(F("0"));
        Serial.print(gps.time.minute());
        Serial.print(":");
        if (gps.time.second() < 10) Serial.print(F("0"));
        Serial.print(gps.time.second());
        Serial.print(".");
        if (gps.time.centisecond() < 10) Serial.print(F("0"));
        Serial.println(gps.time.centisecond());
    }
    else
    {
        Serial.println("Not Available");
    }
*/
   /* Serial.println();
    Serial.println();
    delay(1000);*/
}