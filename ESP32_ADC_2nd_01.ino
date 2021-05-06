/*   Wifi - ESP32-ADC
  Components needed
    1:ESP32
    2:SD module
    3:ADC module
  Ciucuitry
    Disconnect ESP32 from computer
    ESP32       IO18                to  SD module    SCK
    ESP32       IO5                 to  SD module    CS
    ESP32       IO23                to  SD module    MOSI
    ESP32       IO19                to  SD module    MISO
    SD module   VCC                 to  Breadboard   5V
    SD module   GND                 to  Breadboard   GND
    ADC module  A0 divided voltage  to  ESP32        IO34
    ADC module  VCC                 to  Breadboard   5V
    ADC module  GND                 to  Breadboard   GND
    Connect ESP32 to computer
*/
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

WiFiClient client;

File file; 

const char* otahost = "esp32";                        //OTA Host
WebServer server(80);                                 //OTA WebServer
bool shouldSaveConfig = false;                        //flag for saving WIFI data

const char* host = "192.168.137.1";                   //TCP server address
int port = 5000;                                      //TCP server port  

#define samplecount 20                                //save once per 20 samples
int32_t filecount = 0;                                
const int ADCPin = 34;                                //ADC Pin GPIO 34 (Analog ADC1_CH6)

int ADCValue = 0;                                     
int ADC_Array[samplecount]={};                        
int count;                                            
String path = "/ADC.csv";                             //File name to store ADC value
unsigned long previoustime;                           //Time for taking sample
unsigned long sendtime;                               //Time for TCP sending sample


/*
 * Login page,OTA Webserver
 */
const char* loginIndex = 
 "<form name='loginForm'>"
    "<table width='20%' bgcolor='A09F9F' align='center'>"
        "<tr>"
            "<td colspan=2>"
                "<center><font size=4><b>ESP32 Login Page</b></font></center>"
                "<br>"
            "</td>"
            "<br>"
            "<br>"
        "</tr>"
        "<td>Username:</td>"
        "<td><input type='text' size=25 name='userid'><br></td>"
        "</tr>"
        "<br>"
        "<br>"
        "<tr>"
            "<td>Password:</td>"
            "<td><input type='Password' size=25 name='pwd'><br></td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
            "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
        "</tr>"
    "</table>"
"</form>"
"<script>"
    "function check(form)"
    "{"
    "if(form.userid.value=='admin' && form.pwd.value=='admin')"
    "{"
    "window.open('/serverIndex')"
    "}"
    "else"
    "{"
    " alert('Error Password or Username')/*displays error message*/"
    "}"
    "}"
"</script>";
 
/*
 * Server Index Page, OTAWebserver
 */
const char* serverIndex = 
"<script src='https://cdn.bootcss.com/jquery/3.3.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
    "</form>"
 "<div id='prg'>progress: 0%</div>"
 "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')" 
 "},"
 "error: function (a, b, c) {"
 "}"
 "});"
 "});"
 "</script>";
 SPIClass hspi(HSPI);



void setup() {
  Serial.begin(9600);
  delay(1000);
  
  analogReadResolution(10);   
  pinMode(ADCPin,INPUT);

  pinMode(5, OUTPUT);         //VSPI SS
  if(!SD.begin(5)){           //VSPI: SCK 18 MISO 19 MOSI 23 CS 5
    Serial.println("Card Mount Failed");
    return;
  }
  
  uint8_t cardType = SD.cardType(); //Judge SD module
  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
    return;
  }
  WIFI_Manager();             //Web management
  WIFI_OTA();                 //OTAServer
  previoustime = millis();
  sendtime = millis();
}

void loop() {
  if(millis() - previoustime >= 50){  //set time to 50ms
    ADCValue = analogRead(ADCPin);    
    ADC_Array[count++] = ADCValue;    //save the value，while count+1
    previoustime = millis();          //reset
  }
  if(count >= samplecount){           
    count = 0;
    WriteData();
    if(client.connected()){           //if TCP is connected
      TCPPrintData();
    } else {                          //if not connected，and time passes 2s, report "Failed"
      if (!client.connect(host,port,2000)) {
        Serial.println("Connected Failed!");
      } else {
        TCPPrintData();  
      }
    }
  }
  server.handleClient();              //OTA Handle
  delay(1);
}

/*
 Write date to SD card module
*/
void WriteData(){
  if(!SD.exists(path)){               //create a new file if file doesn't exist
    file = SD.open(path, FILE_WRITE);
  } else {
    file = SD.open(path, FILE_APPEND);
  }
  if(!file){
    Serial.println("Failed to open file for writing");
    return;
  } else {
    for(int i = 0;i < samplecount;i++){
      file.println(ADC_Array[i]);
    }
    Serial.println("Success Write");
  }
  file.close();
}

/*
 TCP sends data wirelessly
*/
void TCPPrintData(){
  Serial.println("Connected ok!,Now Printing");                       
  if (client.connected()){
    client.println();
    for(int i = 0;i < samplecount;i++){
      client.println(ADC_Array[i]);
    }
    Serial.println("Success Send");
  }
}

/*
 web management
*/
void WIFI_Manager(){
  Serial.println("Config WIFI");
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  WiFiManager wifiManager;                                 //create wifimanager
//  wifiManager.resetSettings();                            
  wifiManager.setConnectTimeout(10);                          
  wifiManager.setDebugOutput(false);                        
  wifiManager.setMinimumSignalQuality(30);                 //set minimum signal
  IPAddress _ip = IPAddress(192, 168, 4, 25);
  IPAddress _gw = IPAddress(192, 168, 4, 1);
  IPAddress _sn = IPAddress(255, 255, 255, 0);
  wifiManager.setAPStaticIPConfig(_ip, _gw, _sn);           
  wifiManager.setAPCallback(configModeCallback);             
  wifiManager.setSaveConfigCallback(saveConfigCallback);     
  wifiManager.setBreakAfterConfig(true);                     
  wifiManager.setRemoveDuplicateAPs(true);                  
  if(!wifiManager.autoConnect("ConnectMe")) {              
    Serial.println("Failed to connect and hit timeout");
    delay(1000);
  }
  else{
    Serial.println("Succ Config WIFI");
    Serial.println("connected..OK)");
  }
}

/*
 Set up LAN OTA，the screen will show last three digits of IP address
*/
void WIFI_OTA(){
  if (!MDNS.begin(otahost)) { //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while(1);
  }
  Serial.println("OTA started");
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  server.begin();
}

/*
 Configuration of call back in AP mode
*/
void configModeCallback(WiFiManager *myWiFiManager) {

}

/*
 Safe the call back
*/
void saveConfigCallback() {
  shouldSaveConfig = true;
}
