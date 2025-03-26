/*
  Udp NTP Client with Timezone Adjustment

  Get the time from a Network Time Protocol (NTP) time server
  Demonstrates use of UDP sendPacket and ReceivePacket
  For more on NTP time servers and the messages needed to communicate with them,
  see http://en.wikipedia.org/wiki/Network_Time_Protocol

  created 4 Sep 2010
  by Michael Margolis
  modified 9 Apr 2012
  by Tom Igoe
  modified 28 Dec 2022
  by Giampaolo Mancini
  modified 29 Jan 2024
  by Karl Söderby

  This code is in the public domain.
 */

#include <WiFi.h>
#include <WiFiUdp.h>
#include <mbed_mktime.h>
#include "arduino_secrets.h"


//~~~~~~~~~~ WiFi Variables ~~~~~~~~~~~~//
  int timezone = -5;
  int status = WL_IDLE_STATUS;

  char ssid[] = SECRET_SSID;
  char pass[] = SECRET_PASS;

  int keyIndex = 0;  // your network key index number (needed only for WEP)
  unsigned int localPort = 2390;  // local port to listen for UDP packets

  constexpr auto timeServer{ "pool.ntp.org" };
  const int NTP_PACKET_SIZE = 48;  // NTP timestamp is in the first 48 bytes of the message

  byte packetBuffer[NTP_PACKET_SIZE];  // buffer to hold incoming and outgoing packets

  // A UDP instance to let us send and receive packets over UDP
  WiFiUDP Udp;

//~~~~~~~~~~ RTC  Variables ~~~~~~~~~~~~//
  constexpr unsigned long clockInterval{ 1000 };
  unsigned long clockCheck{};

void setup() {
  Serial.begin(9600);

  //~~~~~~~~~~ WiFi Setup ~~~~~~~~~~~~//
    // check for the WiFi module:
    if (WiFi.status() == WL_NO_SHIELD) {
      Serial.println("Communication with WiFi module failed!");
      // don't continue
      while (true)
        ;
    }

    // attempt to connect to WiFi network:
    while (status != WL_CONNECTED) {
      Serial.print("Attempting to connect to SSID: ");
      Serial.println(ssid);
      // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
      status = WiFi.begin(ssid, pass);

      // wait 10 seconds for connection:
      delay(10000);
    }

    Serial.println("Connected to WiFi");
    printWifiStatus();

  //~~~~~~~~~~ RTC Setup ~~~~~~~~~~~~~//
    setNtpTime();
  
}

void loop() {
  if (millis() > clockCheck) {
    Serial.print("System Clock:          ");
    Serial.println(getLocaltime());
    clockCheck = millis() + clockInterval;
  }
}

//~~~~~~~~~~~ WiFi/RTC functions ~~~~~~~~~~~~~~~~~~//

  void setNtpTime() {
    Udp.begin(localPort);
    sendNTPpacket(timeServer);
    delay(1000);
    parseNtpPacket();
  }

  // send an NTP request to the time server at the given address
  unsigned long sendNTPpacket(const char* address) {
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    packetBuffer[0] = 0b11100011;  // LI, Version, Mode
    packetBuffer[1] = 0;           // Stratum, or type of clock
    packetBuffer[2] = 6;           // Polling Interval
    packetBuffer[3] = 0xEC;        // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;

    Udp.beginPacket(address, 123);  // NTP requests are to port 123
    Udp.write(packetBuffer, NTP_PACKET_SIZE);
    Udp.endPacket();
  }

  unsigned long parseNtpPacket() {
    if (!Udp.parsePacket())
      return 0;

    Udp.read(packetBuffer, NTP_PACKET_SIZE);
    const unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    const unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    const unsigned long secsSince1900 = highWord << 16 | lowWord;
    constexpr unsigned long seventyYears = 2208988800UL;
    const unsigned long epoch = secsSince1900 - seventyYears;
    const unsigned long new_epoch = epoch + (3600 * timezone); //multiply the timezone with 3600 (1 hour)
    set_time(new_epoch);
    return epoch;
  }

  String getLocaltime() {
    char buffer[32];
    tm t;
    _rtc_localtime(time(NULL), &t, RTC_FULL_LEAP_YEAR_SUPPORT);
    strftime(buffer, 32, "%Y-%m-%d %k:%M:%S", &t);
    String buf = String(buffer);
    //buf = buf.substring(11, 16); //HH:MM only
    buf = buf.substring(11, 19); // HH:MM:SS only 
    return buf;
    //return String(buffer);
  }

  void printWifiStatus() {
    // print the SSID of the network you're attached to:
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());

    // print your board's IP address:
    IPAddress ip = WiFi.localIP();
    Serial.print("IP Address: ");
    Serial.println(ip);

    // print the received signal strength:
    long rssi = WiFi.RSSI();
    Serial.print("signal strength (RSSI):");
    Serial.print(rssi);
    Serial.println(" dBm");
  }