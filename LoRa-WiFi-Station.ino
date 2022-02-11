#include <LoRa.h>
#include <ESP8266WiFi.h>

#define WIFI_SSID "LoRa_Station_2"
#define WIFI_PSK "thereisnospoon"
#define WIFI_CHANNEL 1
#define HIDE_SSID false
#define MAX_CLIENTS 8
#define SERVER_PORT 9597
#define MAC_ADDRESS_LENGTH 6
#define LORA_SS 16
#define LORA_RST 2
#define LORA_DIO0 15
#define LORA_FREQUENCY 433E6
#define LORA_TX_POWER 20
#define LORA_SPREADING_FACTOR 12
#define LORA_SIGNAL_BANDWIDTH 125E3
#define LORA_CODING_RATE 8
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYNC_WORD 0x12
#define RETRY_MAX 3

IPAddress localIP(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
WiFiServer server(SERVER_PORT);
WiFiClient clients[MAX_CLIENTS];
byte macAddresses[MAX_CLIENTS][MAC_ADDRESS_LENGTH];

void setup() {
  if (!WiFi.softAPConfig(localIP, gateway, subnet))
    while (true)
      ;
  if (!WiFi.softAP(WIFI_SSID, WIFI_PSK, WIFI_CHANNEL, HIDE_SSID, MAX_CLIENTS))
    while (true)
      ;
  server.begin();
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQUENCY))
    while (true)
      ;
  LoRa.setFrequency(LORA_FREQUENCY);
  LoRa.setTxPower(LORA_TX_POWER);
  LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
  LoRa.setSignalBandwidth(LORA_SIGNAL_BANDWIDTH);
  LoRa.setCodingRate4(LORA_CODING_RATE);
  LoRa.setPreambleLength(LORA_PREAMBLE_LENGTH);
  LoRa.setSyncWord(LORA_SYNC_WORD);
  LoRa.enableCrc();
  LoRa.disableInvertIQ();
  LoRa.onReceive(onLoRaReceive);
  LoRa.receive();
}

void loop() {
  onClientConnect();
  onClientMessage();
  onClientDisconnect();
}

void onClientConnect() {
  WiFiClient newClient = server.available();
  if (newClient) {
    struct station_info* stationInfo = wifi_softap_get_station_info();
    while (stationInfo != NULL) {
      if (newClient.remoteIP() == stationInfo->ip) {
        addClient(newClient, stationInfo->bssid);
        wifi_softap_free_station_info();
        return;
      }
      stationInfo = STAILQ_NEXT(stationInfo, next);
    }
  }
}

void onClientMessage() {
  for (uint8_t i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i] && clients[i].available()) {
      onWiFiReceive(i);
    }
  }
}

void onClientDisconnect() {
  for (uint8_t i = 0; i < MAX_CLIENTS; i++) {
    if (!clients[i]) {
      memset(macAddresses[i], 0, sizeof(macAddresses[i]));
    }
  }
}

void addClient(WiFiClient newClient, byte* statMacAddress) {
  for (uint8_t i = 0; i < MAX_CLIENTS; i++) {
    if (memcmp(macAddresses[i], statMacAddress, sizeof(macAddresses[i])) == 0) {
      clients[i] = newClient;
      return;
    }
  }
  for (uint8_t i = 0; i < MAX_CLIENTS; i++) {
    if (!clients[i]) {
      clients[i] = newClient;
      memcpy(macAddresses[i], statMacAddress, sizeof(macAddresses[i]));
      return;
    }
  }
}

void onWiFiReceive(uint8_t clientId) {
  int packetSize = clients[clientId].available();
  byte packet[packetSize];
  clients[clientId].readBytes(packet, packetSize);
  sendToLoRa(packet, packetSize);
  for (uint8_t i = 0; i < MAX_CLIENTS; i++) {
    if (i == clientId) continue;
    sendToWiFiClient(i, packet, packetSize);
  }
}

void onLoRaReceive(int packetSize) {
  Serial.println(packetSize);
  byte packet[packetSize];
  LoRa.readBytes(packet, packetSize);
  for (uint8_t i = 0; i < MAX_CLIENTS; i++) {
    sendToWiFiClient(i, packet, packetSize);
  }
}

void sendToWiFiClient(uint8_t clientId, byte* packet, int packetSize) {
  uint8_t retries = 0;
  if (!clients[clientId] || !clients[clientId].connected()) return;
  do {
    retries++;
  } while ((clients[clientId].write(packet, packetSize) != packetSize) && (retries != RETRY_MAX));
}

void sendToLoRa(byte* packet, int packetSize) {
  uint8_t retries = 0;
  do {
    retries++;
    LoRa.beginPacket();
    LoRa.write(packet, packetSize);
  } while ((LoRa.endPacket() == 0) && (retries != RETRY_MAX));
  LoRa.receive();
}
