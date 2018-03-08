#include "DHT.h"
#include <LiquidCrystal_I2C.h>
#include <everytime.h>
#include<ESP8266WiFi.h>
#include<ESP8266WebServer.h>
#include<ESP8266HTTPClient.h>
#include<ArduinoJson.h>
#include <MillisTimer.h>

#define DHTPIN D3  
#define DHTTYPE DHT22 

ESP8266WebServer server;
StaticJsonBuffer<2000> jsonBuffer;

const char* ssid = "ssid";
const char* password = "password";
const String gatewayHost="host_addres";


boolean authenticated = false;
const char* access_token;
String authorization="";
char MAC_char[18];
String  registerRequestBody;

MillisTimer registerTimer = MillisTimer(1000);
MillisTimer readStatesTimer = MillisTimer(1000);
MillisTimer readDHTTimer = MillisTimer(1000);

DHT dht(DHTPIN, DHTTYPE);
int localHum = 0;
int localTemp = 0;
LiquidCrystal_I2C lcd(0x3F, 16, 2);


void sendDHTData(){
  
  String serviceDataUri = gatewayHost + "/api/device-data/v1/dht22-data";
  HTTPClient http;
  Serial.println("");
  Serial.println("Send DHT DATA...");
  Serial.println(serviceDataUri);

  http.begin(serviceDataUri);
  http.addHeader("Accept", "application/json");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", authorization);
  

  String requestBody="";

  JsonObject& root = jsonBuffer.createObject();
  root["deviceId"] = MAC_char;
  root["temp"] = localTemp;
  root["humidity"]=localHum;
  root.printTo(requestBody);
  Serial.println("Request : ");
  Serial.println(requestBody);
  
  int httpCode = http.POST(requestBody);
  String payload = http.getString();

  Serial.println("");
  Serial.println("RESPONSE: ");
  Serial.println(payload);
  jsonBuffer.clear();
  http.end();
}

void getDHT()
{
  float tempIni = localTemp;
  float humIni = localHum;
  localTemp = dht.readTemperature();
  localHum = dht.readHumidity();
  
  if (localHum==NULL || localTemp==NULL || localTemp >100 || localTemp < -100 || localHum >100 || localHum< 0 )   // Check if any reads failed and exit early (to try again).
  {
    Serial.println("Failed to read from DHT sensor!");
    localTemp = tempIni;
    localHum = humIni;
    return;
  }

 if(tempIni!=localTemp || humIni!=localHum )
  {
  
    
    lcd.clear();
    lcd.setCursor(2, 1);
    lcd.print("Temp :");
    lcd.setCursor(14, 1);
    lcd.print(localTemp);
    lcd.print(" C");
  
    lcd.setCursor(2, 2);
    lcd.print("Humidity :");
    lcd.setCursor(14, 2);
    lcd.print(localHum);
    lcd.print(" %");

    sendDHTData();

    
  }

}

void info() {
  String htmlInfoMsg = "<h1> MINI WEB SERVER INFO  </h1>";

  authenticated ? htmlInfoMsg = htmlInfoMsg + "<h2>Succesfull authenticated</h2>" :  htmlInfoMsg =  htmlInfoMsg +  "<h2>Not authenticated</h2>";
  registerTimer.isRunning() ? htmlInfoMsg = htmlInfoMsg + "<h2>Period register enabled</h2>" :  htmlInfoMsg =  htmlInfoMsg +  "<h2>Period register disabled</h2>";

  server.send(200, "text/html", htmlInfoMsg);
  htmlInfoMsg = "";
}

void authenticate() {
   String  authUri = gatewayHost+"/auth/auth/oauth/token";
  
  HTTPClient http;
  Serial.println("");
  Serial.println("Authenticate ...");
  Serial.println(authUri);
  http.begin(authUri);
  http.addHeader("Authorization", "Basic c3ByaW5nY2xvdWR0ZW1wOnNjdF9zZWNyZXQ=");
  http.addHeader("Accept", "application/json");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  int httpCode = http.POST("grant_type=password&username=device&password=test&scope=deviceclient");
  String payload = http.getString();
  http.end();
  Serial.println("");
  Serial.print("RESPONSE: ");
  Serial.print(payload);

  JsonObject& root = jsonBuffer.parseObject(payload);
  if (!root.success()) {
    Serial.println("Authentication problem.Canot get token object from authentication resposne.");
    return;
  }

  access_token = root["access_token"];
  Serial.println("");
  Serial.print("access_token: ");
  Serial.print(access_token);

  authorization="Bearer";
  authorization+=access_token;

  authenticated = true;
  jsonBuffer.clear();
}




void registerDeviceActivity() {

  String registerUri = gatewayHost + "/api/device-reg/v1/devices/register-activity/"+ MAC_char;
  HTTPClient http;
  Serial.println("");
  Serial.println("RegisterDeviceActivity ...");
  Serial.println(registerUri);

  http.begin(registerUri);
  http.addHeader("Accept", "application/json");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization",  authorization);

  int httpCode = http.PUT(registerRequestBody);
  String payload = http.getString();

  Serial.println("");
  Serial.print("RESPONSE: ");
  Serial.print(payload);
  http.end();

}

void registerDevice() {

  String registerUri = gatewayHost+ "/api/device-reg/v1/devices/register";
  HTTPClient http;
  Serial.println("");
  Serial.println("RegisterDevice ...");
  Serial.println(registerUri);

  http.begin(registerUri);
  http.addHeader("Accept", "application/json");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization",  authorization);

  int httpCode = http.POST(registerRequestBody);
  String payload = http.getString();

  Serial.println("");
  Serial.print("RESPONSE: ");
  Serial.print(payload);
  http.end();

}

void registerDeviceExpiredHanlder(MillisTimer &mt) {
  registerDeviceActivity();
}




String getDeviceOutputStates() {
  String deviceStatesUri = gatewayHost + "/api/device-state/v1/device-state/" + MAC_char;

  HTTPClient http;
  Serial.println("");
  Serial.println("Get device states...");
  Serial.println(deviceStatesUri);

  http.begin(deviceStatesUri);
  http.addHeader("Accept", "application/json");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization",  authorization);

  int httpCode = http.GET();
  String payload = http.getString();
  http.end();
  return payload;
}

void  setDeviceOutputStates(String json) {
  Serial.println("");
  Serial.println("Start parsing json response...");
  Serial.println(json);
  
  JsonArray& rootArray = jsonBuffer.parseArray(json);
  int arraySize=rootArray.size();
  Serial.println(rootArray.size()); //0   
  rootArray.prettyPrintTo(Serial); //[]
  
  for (int i = 0; i < arraySize; i++) {  
    
      JsonObject& arrayElement = rootArray[i];
      uint8_t pin=arrayElement["pinNumber"];
      boolean activate= arrayElement["activated"];
      pinMode(pin, OUTPUT);
      digitalWrite(pin, activate ? HIGH : LOW);
  }

  
  jsonBuffer.clear();

}

void readStatesExpiredHanlder(MillisTimer &mt){
   setDeviceOutputStates(getDeviceOutputStates());
}

void readDHTExpiredHanlder(MillisTimer &mt){
  getDHT();
   
}

void refreshStates(){

  setDeviceOutputStates(getDeviceOutputStates());
  server.send(200, "text/plain", "STATES UPDATED FROM DB" );
}


void setMacAddress() {
  uint8_t MAC_array[6];
  WiFi.macAddress(MAC_array);
  for (int i = 0; i < sizeof(MAC_array); ++i) {
    sprintf(MAC_char, "%s%02x:", MAC_char, MAC_array[i]);
  }
  Serial.println(MAC_char);
}

void stopregisterTimer() {
  registerTimer.stop();
  server.send(200, "text/plain", "STOP ACTIVITY REGISTRATION" );

}

void startregisterTimer() {
  registerTimer.start();
  server.send(200, "text/plain", "START ACTIVITY REGISTRATION" );
}

void setRegisterRequestBody() {

  JsonObject& root = jsonBuffer.createObject();
  root["deviceId"] = MAC_char;
  root["ipAddress"] = WiFi.localIP().toString();
  root["listeningPort"] = "80";
  root["comment"] = "Some additional inforation about device";
  root.printTo(registerRequestBody);
  Serial.println("");
  Serial.println("Register request body : ");
  Serial.println(registerRequestBody);

  jsonBuffer.clear();
}

void setup() {
Serial.begin(115200);
  lcd.begin(16,2);
  lcd.init();
  lcd.backlight();
  WiFi.begin(ssid, password);
 
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.println("");
  Serial.print("Device IP Adress: ");
  Serial.print(WiFi.localIP());
  setMacAddress();
  setRegisterRequestBody();

  server.on("/info", info);
  server.on("/reg-start", startregisterTimer);
  server.on("/reg-stop", stopregisterTimer);
  server.on("/refresh-states",refreshStates);
  server.begin();

  registerTimer.setInterval(1800000);
  registerTimer.expiredHandler(registerDeviceExpiredHanlder);

  readStatesTimer.setInterval(60000);
  readStatesTimer.expiredHandler(readStatesExpiredHanlder);

  readDHTTimer.setInterval(10000);
  readDHTTimer.expiredHandler(readDHTExpiredHanlder);
  readDHTTimer.start();

  while(!authenticated){
      Serial.print(".");
      authenticate();
      delay(500);
  }
  registerDevice();
  setDeviceOutputStates(getDeviceOutputStates());
  getDHT();
}
 
void loop() {
 
 server.handleClient();
 registerTimer.run();
 readDHTTimer.run();
}



