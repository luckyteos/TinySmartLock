/*
  TinyDuino WiFi TinyShield Example Sketch

  Just a basic tutorial showing you how to connect to WiFi with the Wifi
  TinyShield

  NOTE: There are a couple things you'll need to change for this to work!

  Written 29 May 2018
  By Laverena Wienclaw
  Modified 07 January 2019
  By Hunter Hykes

  https://TinyCircuits.com
*/

// This library is for the wifi connection
#include <SPI.h>
#include <WiFi101.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//Need dtostrf from avr library to do proper float/double to string conversion
#include <avr/dtostrf.h>
#include "ardunio_secrets.h"

/*********************** EDIT THIS SECTION TO MATCH YOUR INFO *************************/
char ssid[] = SECRET_SSID;  //  your network SSID (name)
char wifiPassword[] = SECRET_PASS;  // your network password

// Define Serial object based on which TinyCircuits processor board is used.
#if defined(ARDUINO_ARCH_SAMD)
  #define SerialMonitorInterface SerialUSB
#else
  #define SerialMonitorInterface Serial
#endif

int status = WL_IDLE_STATUS;
char server[] = "52.0.236.99";
char lockStatus[] = "Opened";
double currentTemp = -25.0;
double thresholdTemp = -38.0;
WiFiClient client;

unsigned long lastConnectionTime = 0;            // last time you connected to the server, in milliseconds
const unsigned long postingInterval = 15L * 1000L; // delay between updates, in milliseconds (15 seconds => milliseconds)

void setup() {
  SerialMonitorInterface.begin(9600);
  WiFi.setPins(8, 2, A3, -1); // VERY IMPORTANT FOR TINYDUINO
  while(!SerialMonitorInterface);

  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("Wifi Shield not present");
    while(true);
  }

  while (status != WL_CONNECTED){
    SerialMonitorInterface.print("Attempting to connect to SSID: ");
    SerialMonitorInterface.println(ssid);
    status = WiFi.begin(ssid, wifiPassword);
    // wait 10 seconds for connection:
    delay(10000);
  }

  // Attempt to connect to Wifi network:
  //SerialMonitorInterface.print("Connecting Wifi: ");
  //SerialMonitorInterface.println(ssid);

  // Connect to WiFi, and loop until connection is secured
  //WiFi.begin(ssid, wifiPassword);
  //while (WiFi.status() != WL_CONNECTED){
    //SerialMonitorInterface.print(".");
    //delay(500);
  //}
    

  // Print out the local IP address
  SerialMonitorInterface.println("");
  SerialMonitorInterface.println("WiFi connected");
  printWiFiStatus();

  SerialMonitorInterface.println("\n Starting connection to server...");
}

void loop()
{
  char respLine[50] = "";
  double recvThreshold = 0;
  char thresholdStr[6] = "";

  // Test code to tell if threshold gets updated
  //if (thresholdTemp < 0) {
     //dtostrf(thresholdTemp, 5, 1, thresholdStr);
  //} else if (thresholdTemp < 10) {
     //dtostrf(thresholdTemp, 3, 1, thresholdStr);
  //} else {
     //dtostrf(thresholdTemp, 4, 1, thresholdStr);
  //}
  
  //SerialMonitorInterface.print("Current Threshold: ");
  //SerialMonitorInterface.println(thresholdStr);
  while(client.available()){
    char c = client.read();
    //SerialMonitorInterface.write(c);
    //SerialMonitorInterface.println();
    
    if (c == '\n') {
       if (strlen(respLine) != 0){
          respLine[0] = '\0';
       }
    } else if (c != '\r'){
       appendChar(respLine, c);
    }
    //SerialMonitorInterface.println(respLine);

    //Result array to store temperature or lock status
    char result[10] = "";
    if (strlen(respLine) == 15){
      if (startsWith("Threshold:", respLine)){
        if (respLine[10] == '+'){
          if (respLine[11] == 'x') {
            strncpy(result, respLine+12, 3);
          } else {
            strncpy(result, respLine+11, 4);
          }
        } else if (respLine[10] == '-'){
          if (respLine[11] == 'x') {
            result[0] = '-';
            char buff[8] = "";
            strncpy(buff, respLine+12, 3);
            mystrcat(result, buff);
          } else {
            strncpy(result, respLine+10, 5);
          }
        }

        result[strlen(result)] = '\0';
        recvThreshold = atof(result);

        if (thresholdTemp != recvThreshold) {
            thresholdTemp = recvThreshold;
        }
        
        //SerialMonitorInterface.println(result);
      }
    } else if (strlen(respLine) == 13){
      if (startsWith("Lock:", respLine)){
          strncpy(result, respLine+5, 8);
          result[strlen(result)] = '\0';
          //SerialMonitorInterface.println(result);

          if (strcmp(result, "Unlocked") == 0) {
             SerialMonitorInterface.println("Unlock Lock");
          } else if (strcmp(result, "Locked") == 0){
             SerialMonitorInterface.println("Lock lock");
          }
      }
    }
  }

  if (millis() - lastConnectionTime > postingInterval) {
    update_device_status();
  }
}

void update_device_status() {
  // close any connection before send a new request.
  // This will free the socket on the WiFi shield
  client.stop();
  
  if (client.connect(server, 80)){
    SerialMonitorInterface.println("Connected to server");
    char tempStr[6] = "";
    char thresholdStr[6] = "";
    char contentLen[3] = "";
    char lenHeader[20] = "Content-Length: ";

    if (currentTemp < 0){
       dtostrf(currentTemp, 5, 1, tempStr);
       strncpy(contentLen, "70", 2);
    } else if (currentTemp < 10) {
       dtostrf(currentTemp, 3, 1, tempStr);
       strncpy(contentLen, "68", 2);
    } else {
       dtostrf(currentTemp, 4, 1, tempStr);
       strncpy(contentLen, "69", 2);
    }
    
    if (thresholdTemp < 0) {
       dtostrf(thresholdTemp, 5, 1, thresholdStr);
       strncpy(contentLen, "70", 2);
    } else if (thresholdTemp < 10) {
       dtostrf(thresholdTemp, 3, 1, thresholdStr);
       strncpy(contentLen, "68", 2);
    } else {
       dtostrf(thresholdTemp, 4, 1, thresholdStr);
       strncpy(contentLen, "69", 2);
    }

    mystrcat(lenHeader, contentLen);
    
    //HTTP Headers
    client.println("POST /device_status HTTP/1.1");
    client.print("Host: ");
    client.println(server);
    client.println(lenHeader);
    client.println("Content-Type: text/plain");
    client.println("User-Agent: ArduinoWiFi/1.1");
    client.println("Connection: close");
    client.println();
    //HTTP Body containing data to be sent to server
    client.print("{\"Lock Status\":\"");
    client.print(lockStatus);
    client.print("\",");
    client.print("\"Temperature\":\"");
    client.print(tempStr);
    client.print("\",");
    client.print("\"Threshold\":\"");
    client.print(thresholdStr);
    client.println("\"}");
    client.println();

    // note the time that the connection was made:
    lastConnectionTime = millis();
  } else {
    SerialMonitorInterface.println("Connection Failed");
  }
}

void printWiFiStatus() {
  // print the SSID of the network you're attached to:
  SerialMonitorInterface.print("SSID: ");
  SerialMonitorInterface.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  SerialMonitorInterface.print("IP Address: ");
  SerialMonitorInterface.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  SerialMonitorInterface.print("signal strength (RSSI):");
  SerialMonitorInterface.print(rssi);
  SerialMonitorInterface.println(" dBm");
}

void appendChar(char *s, char c) {
  int len = strlen(s);
  s[len] = c;
  s[len+1] = '\0';
}

char* mystrcat( char* dest, char* src )
{
     while (*dest) dest++;
     while (*dest++ = *src++);
     return --dest;
}

bool startsWith(const char *pre, const char *str)
{
    size_t lenpre = strlen(pre),
           lenstr = strlen(str);
    return lenstr < lenpre ? false : memcmp(pre, str, lenpre) == 0;
}
