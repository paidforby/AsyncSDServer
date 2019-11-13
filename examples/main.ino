#define FS_NO_GLOBALS

#include <WiFi.h>
#include <FS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFSEditor.h>
#include <SPIFFS.h>
#include <SD.h>
#include <SPI.h>
#include <AsyncSDServer.h>

uint8_t mac[ADDR_LENGTH];
char macaddr[ADDR_LENGTH*2];

char ssid[32] = "asyncSDtest ";
const char * hostName = "asyncSDtest";

IPAddress local_IP(192, 162, 4, 1);
IPAddress gateway(0, 0, 0, 0);
IPAddress netmask(255, 255, 255, 0);

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
AsyncEventSource events("/events");

int sdInitialized = 0;

const int LoRaChipSelect = 18;
const int SDChipSelect = 2;

bool echo_on = false;

struct wsMessage {
    uint8_t id[2];
    uint8_t type;
    uint8_t delimiter;
    uint8_t data[240];
};

/*
  FORWARD-DEFINED FUNCTIONS
*/
struct wsMessage buildWSMessage(char data[240], size_t length){
    wsMessage message;
    memset(&message, 0, sizeof(message));
    memcpy(&message, data, length);
    return message;
}

void sendToWS(struct wsMessage message, size_t length){
    uint8_t msg[240];  
    memcpy(msg, &message, length);
    ws.binaryAll(msg, length);
}

/*
  CALLBACK FUNCTIONS
*/
void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
    if(type == WS_EVT_CONNECT){
        Serial.printf("ws[%s][%u] connect\r\n", server->url(), client->id());
        wsMessage message1 = buildWSMessage("FFc|Welcome to DISASTER RADIO", 29);
        sendToWS(message1, 29);
        if(echo_on){
            wsMessage message2 = buildWSMessage("FFc|echo enabled, to turn off, enter '$' after logging in", 57);
            sendToWS(message2, 57);
        }
        if(!sdInitialized){
            wsMessage message3 = buildWSMessage("FFc|WARNING: SD card not found, functionality may be limited", 60);
            sendToWS(message3, 60);
        }
        if(!loraInitialized()){
            wsMessage message4 = buildWSMessage("FFc|WARNING: LoRa radio not found, functionality may be limited", 63);
            sendToWS(message4, 63);
        }
        client->ping();
    } else if(type == WS_EVT_DISCONNECT){
        Serial.printf("ws[%s][%u] disconnect: %u\r\n", server->url(), client->id());
    } else if(type == WS_EVT_ERROR){
        Serial.printf("ws[%s][%u] error(%u): %s\r\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
    } else if(type == WS_EVT_PONG){
        Serial.printf("ws[%s][%u] pong[%u]: %s\r\n", server->url(), client->id(), len, (len)?(char*)data:"");
    } else if(type == WS_EVT_DATA){

        AwsFrameInfo * info = (AwsFrameInfo*)arg;
        char msg_id[4];
        char usr_id[32];
        char msg[256];
        int msg_length;
        int usr_id_length = 0;
        int usr_id_stop = 0;

        if(info->final && info->index == 0 && info->len == len){
            //the whole message is in a single frame and we got all of it's data

            Serial.printf("ws[%s][%u] %s-message[%llu]: \r\n", server->url(), client->id(), (info->opcode == WS_TEXT)?"text":"binary", info->len);
            //cast data to char array
            for(size_t i=0; i < info->len; i++) {
                //TODO check if info length is bigger than allocated memory
                msg[i] = (char) data[i];
                msg_length = i; 
                    
                if(msg[i] == '$'){
                    echo_on = !echo_on;
                }

                // check for stop char of usr_id
                if(msg[i] == '>'){
                    usr_id_stop = i;  
                }
            }
            msg_length++;
            msg[msg_length] = '\0';

            //parse message id 
            memcpy( msg_id, msg, 2 );
            msg_id[2] = '!';
            msg_id[3] = '\0';   

            //parse username
            for( int i = 5 ; i < usr_id_stop ; i++){
                usr_id[i-5] = msg[i];
            }
            usr_id_length = usr_id_stop - 5;

            //print message info to serial
            Serial.printf("Message Length: %d\r\n", msg_length);
            Serial.printf("Message ID: %02d%02d %c\r\n", msg_id[0], msg_id[1], msg_id[2]);
            Serial.printf("Message:");
            for( int i = 0 ; i <= msg_length ; i++){
                Serial.printf("%c", msg[i]);
            }
            Serial.printf("\r\n");

            //TODO delay ack based on estimated transmit time
            //send ack to websocket
            ws.binary(client->id(), msg_id, 3);

            //echoing message to ws
            if(echo_on){
                /*
                char echo[256]; 
                char prepend[7] = "<echo>";
                int prepend_length= 6;
                memcpy(echo, msg, 4);
                for( int i = 0 ; i < prepend_length ; i++){
                    echo[4+i] = prepend[i];
                }
                for( int i = 0 ; i < msg_length-usr_id_stop ; i++){
                    echo[4+prepend_length+i] = msg[i+usr_id_stop+1];
                }
                int echo_length = prepend_length - usr_id_length + msg_length - 1;
                */
                ws.binaryAll(msg, msg_length);
            }
        } 
        else {
            //TODO message is comprised of multiple frames or the frame is split into multiple packets

        }
    }
}

/*
  SETUP FUNCTIONS
*/
void wifiSetup(){
    WiFi.mode(WIFI_AP);
    WiFi.macAddress(mac);
    sprintf(macaddr, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac [5]);
    setLocalAddress(macaddr);
    strcat(ssid, macaddr);
    WiFi.setHostname(hostName);
    WiFi.softAP(ssid);
}

SPIClass SDSPI(HSPI);

void sdCardSetup(){
    
    Serial.print("\r\nWaiting for SD card to initialise...");

    /* for esp32 TTGO v1.6 */
    SDSPI.begin(14, 2, 15, -1);
//    SDSPI.begin(sck, miso, mosi, -1);
    
    if (SD.begin(13, SDSPI)) { 

        Serial.print("SD Card initialized");
        Serial.print("\r\n");
        sdInitialized = 1;
        File entry;
        File dir = SD.open("/");
        dir.rewindDirectory();
        Serial.print("ROOT DIRECTORY:");
        Serial.print("\r\n");
        while(true){
            entry = dir.openNextFile();
            if (!entry) break;
            Serial.printf("%s, type: %s, size: %ld", entry.name(), (entry.isDirectory())?"dir":"file", entry.size());
            entry.close();
            Serial.print("\r\n");
        }
        dir.close();
    } else{
        Serial.print("SD Card Not Found!");
        Serial.print("\r\n");
    }
}

void spiffsSetup(){
    if (SPIFFS.begin()) {
        Serial.print("ok\r\n");
        if (SPIFFS.exists("/index.htm")) {
            Serial.printf("The file exists!\r\n");
            fs::File f = SPIFFS.open("/index.htm", "r");
            if (!f) {
                Serial.printf("Some thing went wrong trying to open the file...\r\n");
            }
            else {
                int s = f.size();
                Serial.printf("Size=%d\r\n", s);
                String data = f.readString();
                Serial.printf("%s\r\n", data.c_str());
                f.close();
            }
        }
        else {
            Serial.printf("No such file found.\r\n");
        }
    }
}


void webServerSetup(){

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    events.onConnect([](AsyncEventSourceClient *client){
        client->send("hello!",NULL,millis(),1000);
    });

    server.addHandler(&events);

    if(sdInitialized){
        server.addHandler(new AsyncStaticSDWebHandler("/", SD, "/", "max-age=604800"));
    }else{
        server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.htm");
    }
    
    server.onNotFound([](AsyncWebServerRequest *request){
        Serial.printf("NOT_FOUND: ");
        if(request->method() == HTTP_GET)
        Serial.printf("GET");
        else if(request->method() == HTTP_POST)
        Serial.printf("POST");
        else if(request->method() == HTTP_DELETE)
        Serial.printf("DELETE");
        else if(request->method() == HTTP_PUT)
        Serial.printf("PUT");
        else if(request->method() == HTTP_PATCH)
        Serial.printf("PATCH");
        else if(request->method() == HTTP_HEAD)
        Serial.printf("HEAD");
        else if(request->method() == HTTP_OPTIONS)
        Serial.printf("OPTIONS");
        else
        Serial.printf("UNKNOWN");
        Serial.printf(" http://%s%s\r\n", request->host().c_str(), request->url().c_str());

        if(request->contentLength()){
            Serial.printf("_CONTENT_TYPE: %s\r\n", request->contentType().c_str());
            Serial.printf("_CONTENT_LENGTH: %u\r\n", request->contentLength());
        }

        int headers = request->headers();
        int i;
        for(i=0;i<headers;i++){
            AsyncWebHeader* h = request->getHeader(i);
            Serial.printf("_HEADER[%s]: %s\r\n", h->name().c_str(), h->value().c_str());
        }

        int params = request->params();
        for(i=0;i<params;i++){
            AsyncWebParameter* p = request->getParam(i);
            if(p->isFile()){
                Serial.printf("_FILE[%s]: %s, size: %u\r\n", p->name().c_str(), p->value().c_str(), p->size());
            } else if(p->isPost()){
                Serial.printf("_POST[%s]: %s\r\n", p->name().c_str(), p->value().c_str());
            } else {
                Serial.printf("_GET[%s]: %s\r\n", p->name().c_str(), p->value().c_str());
            }
        }

        request->send(404);
    });

    server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
        if(!index)
        Serial.printf("BodyStart: %u\r\n", total);
        Serial.printf("%s", (const char*)data);
        if(index + len == total)
        Serial.printf("BodyEnd: %u\r\n", total);
    });

    server.begin();

}

/*
  START MAIN
*/
void setup(){
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    pinMode(SDChipSelect, OUTPUT);
    digitalWrite(LoRaChipSelect, HIGH);
    digitalWrite(SDChipSelect, LOW); 
    btStop(); //stop bluetooth as it may cause memory issues
    wifiSetup();
    sdCardSetup();
    if(!sdInitialized){
        spiffsSetup();
    }    
    webServerSetup();
}

void loop(){
    // insert cool synchronus code here
}
