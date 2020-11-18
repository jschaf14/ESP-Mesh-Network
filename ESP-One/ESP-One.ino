#include <esp_now.h>
#include <WiFi.h>

#define TEMP1_READ_PIN 2
#define MS_TO_SEC 1000
#define ONE_SECOND (1 * MS_TO_SEC)
#define FIVE_SECONDS (5 * MS_TO_SEC)
#define HEAT_ENABLE_AT 21
#define HEAT_DISABLE_AT 24
#define ESP_TWO_CHANNEL 0
#define PEER_ADDR_MEM_BYTES 6
#define TRANSMIT_LED_PIN 12
#define RESET 0

const uint8_t ESP_Two_address[] = {0x24, 0x6F, 0x28, 0xB2, 0xD6, 0x84};

static uint32_t tempVoltRead = RESET;
static uint32_t tempInCelsius = RESET;
static uint32_t swapper = RESET;
static bool heatOnFlag = false;

enum ESP_One_st {temp_reading_st, transmit_st} ESP_One_current_st;

static uint32_t volt_to_celsius(uint32_t x) {
  return ((x - 500) / 10);
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(TEMP1_READ_PIN, INPUT);
  pinMode(TRANSMIT_LED_PIN, OUTPUT);
  delay(ONE_SECOND);

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, ESP_Two_address, PEER_ADDR_MEM_BYTES);
  peerInfo.channel = ESP_TWO_CHANNEL;  
  peerInfo.encrypt = true;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
}
 
void loop() {
  // put your main code here, to run repeatedly:
  tempVoltRead = analogRead(TEMP1_READ_PIN);
  tempInCelsius = volt_to_celsius(tempVoltRead);
  if (swapper >= 3) {
    swapper = RESET;
    heatOnFlag = !heatOnFlag;
    esp_err_t result = esp_now_send(ESP_Two_address, (uint8_t *) &heatOnFlag, sizeof(heatOnFlag));
    if (result == ESP_OK) {
      Serial.println("Sent with success");
    }
    else {
      Serial.println("Error sending the data");
    }
    Serial.print("Transmitting LOOP ");
    Serial.println(heatOnFlag);
  }
  swapper++;
  
  switch (ESP_One_current_st) {
  case temp_reading_st:
    // if below enable cutoff or above disable cutoff, move to transmit
    if ((!heatOnFlag && tempInCelsius <= HEAT_ENABLE_AT) || 
          (heatOnFlag && tempInCelsius >= HEAT_DISABLE_AT)) {
      heatOnFlag = (tempInCelsius <= HEAT_ENABLE_AT); //set flag to true or false based on tempature.
      ESP_One_current_st = transmit_st; //change state to transmit
    }
    break;
   case transmit_st:
    Serial.println("Transmitting");
    esp_now_send(ESP_Two_address, (uint8_t *) heatOnFlag, sizeof(heatOnFlag));
    ESP_One_current_st = temp_reading_st;
    break;
  }
  delay(ONE_SECOND);
}
