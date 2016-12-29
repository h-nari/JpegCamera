#include "conf.h"

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <stdlib.h>
#include <time.h>

#ifdef USE_TFT
#include <Adafruit_GFX.h>		// https://github.com/adafruit/Adafruit-GFX-Library
#include <Adafruit_ILI9341.h>		// https://github.com/adafruit/Adafruit_ILI9341
#include <Fontx.h>							// https://github.com/h-nari/Fontx
#include <FontxGfx.h>						// https://github.com/h-nari/FontxGfx
#include <Humblesoft_ILI9341.h>	// https://github.com/h-nari/Humblesoft_ILI9341
#endif

#ifdef USE_OTA
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#endif

#include "JpegCamera.h"

#ifdef USE_STATIC_IP
IPAddress staticIP(MY_IP_ADDR);
IPAddress gateway(MY_GATEWAY);
IPAddress subnet(MY_SUBNET);
#endif

#define TFT_CS	 2
#define TFT_DC  15
#define TFT_RST -1

ESP8266WebServer server(80);
#ifdef USE_TFT
Humblesoft_ILI9341 tft = Humblesoft_ILI9341(TFT_CS,TFT_DC,TFT_RST);

static uint16_t c1 = ILI9341_GREEN;
static uint16_t c2 = ILI9341_YELLOW;
static uint16_t bg = ILI9341_BLACK;
#endif
#if USE_OTA
static int8_t ota_col;
#endif

uint8_t buf[256];
SoftwareSerial com2(4,5,false, sizeof(buf)+5);		// IO4-RX IO5-TX
JpegCamera  camera(&com2);

void handleRoot() {
  server.send(200, "text/plain", "hello from esp8266/jpeg_camera");
}

void handleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void handleCamera() {
	unsigned long  tStart = millis();
	String size = server.arg("size");
	JpegCamera::ImageSize s = JpegCamera::IMAGE_SIZE_160_120;
	
	if(size == "" || size == "160x120")
		s = JpegCamera::IMAGE_SIZE_160_120;
	else if(size == "320x240")
		s = JpegCamera::IMAGE_SIZE_320_240;
	else if(size == "640x480")
		s = JpegCamera::IMAGE_SIZE_640_480;
	else if(size == "800x600")
		s = JpegCamera::IMAGE_SIZE_800_600;
	else if(size == "1280x960")
		s = JpegCamera::IMAGE_SIZE_1280_960;
	else if(size == "1600x1200")
		s = JpegCamera::IMAGE_SIZE_1600_1200;
	else {
		String message = "undefined size parameter:";
		message += size;
		message += "\n";
		message += "should be 160x120,320x240,";
		message += "640x480,800x600,1280x9600,1600x1200\n";
		server.send(404,"text/plain",message);
		return;
	}
	
	if(!camera.setImageSize(s)) return;

	camera.takePicture([](uint32_t size) {
			server.setContentLength(size);
			if(server.hasArg("download")) {
				server.sendHeader("Content-Disposition",
													"attachment; filename=camera.jpg");
				server.send(200,"application/octed-stream","");
			} else
				server.send(200,"image/jpeg","");
		},
		[](const uint8_t *buf, uint32_t len){
			WiFiClient client = server.client();
			if(client.connected())
				client.write(buf, len);
		},
		[](){
			WiFiClient client = server.client();
			client.flush();
		});
	Serial.printf("%u ms\n", millis() - tStart);
#ifdef USE_TFT
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	//tft.vsa.printf("%04d/%02d/%02d ",tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday);
	tft.vsa.printf("%02d:%02d:%02d %u ms\n", tm->tm_hour, tm->tm_min, tm->tm_sec,
								 millis() - tStart);
#endif
}

void setup(void){
  bool r;
  Serial.begin(115200);
	com2.begin(38400);
	
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
#if USE_STATIC_IP
  WiFi.config(staticIP, gateway, subnet);
#endif

  Serial.println("\n\nReset:");
#ifdef USE_TFT  
  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextWrap(false);
	tft.setVerticalScrollArea(20,16);
	tft.tfa.setTextSize(2);
	tft.vsa.setTextSize(2);
	tft.bfa.setTextSize(1);
	tft.bfa.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
#endif
	
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
#ifdef USE_TFT
	tft.tfa.setTextColor(ILI9341_GREEN);
  tft.tfa.print("IP: "); 
  tft.tfa.println(WiFi.localIP());
#endif
	
  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }
  configTime( 9 * 3600, 0,
	      "ntp.nict.jp", "ntp.jst.mfeed.ad.jp", NULL);
  
  server.on("/", handleRoot);
	server.on("/camera", handleCamera);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");

#ifdef USE_OTA
#ifdef OTA_PORT
  ArduinoOTA.setPort(OTA_PORT);
#endif
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([](){
      Serial.println("start");
      ota_col = 0;
    });
  ArduinoOTA.onEnd([](){
      Serial.println("\nEnd");
    });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total){
      // Serial.printf("Progress: %u%%\n", (progress / (total / 100)));
      Serial.print('.');
      if(++ota_col >= 50){
	Serial.println();
	ota_col = 0;
      }
    });
  ArduinoOTA.onError([](ota_error_t error){
      Serial.printf("Error[%u]: ", error);
      if(error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if(error == OTA_BEGIN_ERROR) Serial.println("Connect Failed");
      else if(error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if(error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if(error == OTA_END_ERROR) Serial.println("End Failed");
    });
  ArduinoOTA.begin();
#endif
#ifdef USE_TFT
	tft.vsa.println("Start");
#endif
	camera.setImageSize(JpegCamera::IMAGE_SIZE_160_120);
	Serial.println("setup done.");
}

void loop(void){
#if USE_OTA
  ArduinoOTA.handle();
#endif
  server.handleClient();
}

/*** Local variables: ***/
/*** tab-width:2 ***/
/*** End: ***/
