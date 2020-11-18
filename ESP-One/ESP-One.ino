#include <esp_now.h>
#include <WiFi.h>

#define TEMP1_READ_PIN 32
#define TEMP2_READ_PIN 33
#define TEMP3_READ_PIN 34
#define VOLT_READ_PIN 35
#define TRANSMIT_LED_PIN 2
#define MS_TO_SEC 1000
#define ONE_SECOND (1 * MS_TO_SEC)
#define FIVE_SECONDS (5 * MS_TO_SEC)
#define HEAT_ENABLE_AT 68
#define HEAT_DISABLE_AT 72
#define ESP_TWO_CHANNEL 0
#define PEER_ADDR_MEM_BYTES 6
#define RESET 0
#define C_TO_F 9 / 5 + 32
#define TMP36_F_MODIFIER 1
#define E_D_SETTLER 3

const uint8_t ESP_Two_address[] = {0x24, 0x6F, 0x28, 0xB2, 0xD6, 0x84};

static int voltRead = RESET;
static int tempVoltRead = RESET;
static int tempInF = RESET;
static int temp1InF = RESET;
static int temp2InF = RESET;
static int temp3InF = RESET;
static int TempDiff = RESET;
static uint8_t modifySettler = RESET;

// Structure example to receive data
// Must match the sender structure
typedef struct struct_transmit_packet {
  int temp;
  bool heatOnFlag;
} struct_transmit_packet;

struct_transmit_packet myData;

enum ESP_One_st {temp_reading_st, transmit_st} ESP_One_current_st;

static int volt_to_farenheit(int x) {
  return ((x - 500) / 10) * C_TO_F - TMP36_F_MODIFIER;
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(VOLT_READ_PIN, INPUT);
  pinMode(TEMP1_READ_PIN, INPUT);
  pinMode(TEMP2_READ_PIN, INPUT);
  pinMode(TEMP3_READ_PIN, INPUT);
  pinMode(TRANSMIT_LED_PIN, OUTPUT);
  digitalWrite(TRANSMIT_LED_PIN, LOW);

  voltRead = RESET;
  tempVoltRead = RESET;
  tempInF = RESET;
  temp1InF = RESET;
  temp2InF = RESET;
  temp3InF = RESET;
  modifySettler = RESET;
  myData.temp = RESET;
  myData.heatOnFlag = false;
  ESP_One_current_st = temp_reading_st;

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, ESP_Two_address, PEER_ADDR_MEM_BYTES);
  peerInfo.channel = ESP_TWO_CHANNEL;  
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  delay(ONE_SECOND);
}
 
void loop() {
  voltRead = analogRead(VOLT_READ_PIN);
  Serial.print(voltRead);
  
  // put your main code here, to run repeatedly:
  tempVoltRead = analogRead(TEMP1_READ_PIN);
  temp1InF = volt_to_farenheit(tempVoltRead);
  Serial.print("TEMP1: ");
  Serial.println(temp1InF);

  tempVoltRead = analogRead(TEMP2_READ_PIN);
  temp2InF = volt_to_farenheit(tempVoltRead);
  Serial.print("TEMP2: ");
  Serial.println(temp2InF);

  tempVoltRead = analogRead(TEMP3_READ_PIN);
  temp3InF = volt_to_farenheit(tempVoltRead);
  Serial.print("TEMP3: ");
  Serial.println(temp3InF);

  tempInF = (temp1InF + temp2InF + temp3InF) / 3;

  Serial.print("TEMP: ");
  Serial.println(tempInF);
  if (myData.temp >= tempInF)
    TempDiff = myData.temp - tempInF;
  else
    TempDiff = tempInF - myData.temp;

  if (voltRead > 2275 || voltRead < 2245) {
    Serial.println("out of bounds");
    digitalWrite(TRANSMIT_LED_PIN, LOW);
  }
  else digitalWrite(TRANSMIT_LED_PIN, HIGH);
  
  myData.temp = tempInF;
  //esp_now_send(ESP_Two_address, (uint8_t *) &myData, sizeof(myData));
  
  switch (ESP_One_current_st) {
  case temp_reading_st:
    Serial.println("temp_read_st");
    // if below enable cutoff or above disable cutoff, move to transmit
    if ((!myData.heatOnFlag && tempInF <= HEAT_ENABLE_AT) || 
          (myData.heatOnFlag && tempInF >= HEAT_DISABLE_AT)) {
      Serial.println(modifySettler);
      modifySettler++;
      if (modifySettler >= E_D_SETTLER) {
        myData.heatOnFlag = (tempInF <= HEAT_ENABLE_AT); //set flag to true or false based on tempature.
        ESP_One_current_st = transmit_st; //change state to transmit
        modifySettler = RESET;
      }
    }
    else {
      modifySettler = RESET;
    }
    break;
   case transmit_st:
    Serial.println("Transmitting");
    
    esp_now_send(ESP_Two_address, (uint8_t *) &myData, sizeof(myData));
    ESP_One_current_st = temp_reading_st;
    break;
   default:
    Serial.println("Default");
    break;
  }
  
  delay(ONE_SECOND);
}
