/**
 * WiFiManager advanced demo, contains advanced configurartion options
 * Implements TRIGGEN_PIN button press, press for ondemand configportal, hold for 3 seconds for reset settings.
 */

//#include "Api.h"
//#include "ApiController.h"
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <esp_wifi.h>
#include <ArduinoJson.h>
#include <WiFiClient.h>
#include <WebServer.h>



#define TRIGGER_PIN 0
#define DETONATOR_PIN 19
#define DETECTOR_PIN 12
#define SOUND_PIN 14
#define SOUND_ENABLE_PIN 27

 // wifimanager can run in a blocking mode or a non blocking mode
 // Be sure to know how to process loops with no delay() if using non blocking
bool wm_nonblocking = false; // change to true to use non blocking

WiFiManager wm; // global wm instance
WiFiManagerParameter custom_field; // global param ( for non blocking w params )
WebServer server(80);

// JSON data buffer
StaticJsonDocument<250> jsonDocument;
char buffer[250];

//float temperature = 25.5;
//float humidity  = 35.1;
//float pressure = 1013.25;

int Id = 1;
IPAddress IpAddress;// = "";
bool MotionDetected = false;
bool Armed = false;
String Message = "Ok";

bool soundenabled = true;


void setup() {
    WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP  
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    delay(3000);
    Serial.println("\n Starting");

    pinMode(TRIGGER_PIN, INPUT);
    pinMode(DETONATOR_PIN, OUTPUT);
    pinMode(SOUND_PIN, OUTPUT);
    pinMode(DETECTOR_PIN, INPUT);
    pinMode(SOUND_ENABLE_PIN, INPUT_PULLUP);
    digitalWrite(DETONATOR_PIN, LOW);
    soundenabled = digitalRead(SOUND_ENABLE_PIN);
    digitalWrite(SOUND_PIN, soundenabled);
    
    String test = wm.getWiFiSSID();
    if (test == "")
    {
        wm.resetSettings(); // wipe settings
    }
    else Serial.println(test + " MY test");
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
    res = wm.autoConnect("AutoConnectAP"); // password protected ap

    if (!res) {
        Serial.println("Failed to connect or hit timeout");
        wm.resetSettings(); // wipe settings
        ESP.restart();
    }
    else {
        //if you get here you have connected to the WiFi    
        Serial.println(wm.getWLStatusString());
        Serial.print("RRSI: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
        IpAddress = WiFi.localIP();
        Serial.println(IpAddress);
        setup_api();
        
             
    }
    InitSensor();
    
    
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

void loop() {
    if (wm_nonblocking) wm.process(); // avoid delays() in loop when non-blocking and other long running code  
    if (WiFi.status() == WL_CONNECTED) server.handleClient();
    MotionDetected = digitalRead(DETECTOR_PIN);
    if (soundenabled && MotionDetected)
        digitalWrite(SOUND_PIN, HIGH);
    else
        digitalWrite(SOUND_PIN, LOW);
    if (Armed && MotionDetected)
        digitalWrite(DETONATOR_PIN, HIGH);//Explode
    else digitalWrite(DETONATOR_PIN, LOW);
    checkButton();
    
    
    // put your main code here, to run repeatedly:
}

void InitSensor()
{
   // digitalWrite(soundpin, LOW);
    delay(1000);
    while (digitalRead(DETECTOR_PIN) == HIGH)
    {
        delay(1000);
        // Serial.println("initsensor");
    }
    MotionDetected = digitalRead(DETECTOR_PIN);
    attachInterrupt(DETECTOR_PIN, MotionDetection, RISING);
   // attachInterrupt(DETECTOR_PIN, NoMotionDetection, FALLING);
    Message = "Detector ready";
    Serial.println(Message);

    digitalWrite(SOUND_PIN, HIGH);
    delay(100);
    digitalWrite(SOUND_PIN, LOW);
    delay(100);
    digitalWrite(SOUND_PIN, HIGH);
    delay(100);
    digitalWrite(SOUND_PIN, LOW);

}
// Interrupt section
void MotionDetection()
{
    MotionDetected = digitalRead(DETECTOR_PIN);
    Serial.println("Detected");
    if (soundenabled)
        digitalWrite(SOUND_PIN, HIGH);
    else
        digitalWrite(SOUND_PIN, LOW);
    if (Armed && MotionDetected)
        digitalWrite(DETONATOR_PIN, HIGH);//Explode
    else 
        digitalWrite(DETONATOR_PIN, LOW);

}
//void NoMotionDetection()
//{
//    MotionDetected = digitalRead(DETECTOR_PIN);
//    Serial.println("No Detected");
//    
//
//}

// Api Section

void setup_api()
{
    
   
    server.on("/iedstatus", getIEDStatus);
    server.on("/ied", HTTP_POST, handlePost);

    // start server	 	 
    server.begin();
    Serial.println("WebServer paa port 80 startet!!");
   

}


void create_json(int id , IPAddress Ip, bool md, bool am,String ms) 
{
    jsonDocument.clear();
    jsonDocument["Id"] = id;
    jsonDocument["IpAddress"] = Ip;
    jsonDocument["MotionDetected"] = md;
    jsonDocument["Armed"] = am;
    jsonDocument["Message"] = ms;
    serializeJson(jsonDocument, buffer);
}
void add_json_object(char* tag, float value, char* unit) {
    JsonObject obj = jsonDocument.createNestedObject();
    obj["type"] = tag;
    obj["value"] = value;
    obj["unit"] = unit;
}

void getIEDStatus()
{
    Serial.println("Get IEDStatus");
    create_json(Id,IpAddress,MotionDetected,Armed,Message);
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
void handlePost() 
{
    if (server.hasArg("plain") == false) {
        //handle error here
    }
    String body = server.arg("plain");
    deserializeJson(jsonDocument, body);

    //// Get RGB components
    Armed = jsonDocument["Armed"];
    Serial.print(Armed);

    //pixels.fill(pixels.Color(red, green, blue));
    //pixels.show();
    // Respond to the client
    create_json(Id, IpAddress, MotionDetected, Armed, Message);
    server.send(200, "application/json", buffer);
    
}
