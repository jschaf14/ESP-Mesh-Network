#include <esp_now.h>
#include <WiFi.h>

#define TEMP1_READ_PIN 32
#define TEMP2_READ_PIN 33
#define TEMP3_READ_PIN 34
#define VOLT_READ_PIN 35
#define BAD_VOLT_LED_PIN 15
#define BAD_TEMP_LED_PIN 2
#define VOLT_HIGH_BOUND 2275
#define VOLT_LOW_BOUND 2245
#define MS_TO_SEC 1000
#define ONE_SECOND (1 * MS_TO_SEC)
#define FIVE_SECONDS (5 * MS_TO_SEC)
#define HEAT_ENABLE_AT 68
#define HEAT_DISABLE_AT 72
#define ESP_TWO_CHANNEL 0
#define PEER_ADDR_MEM_BYTES 6
#define RESET 0
#define C_TO_F 9 / 5 + 32
#define TMP36_F_MODIFIER -7
#define E_D_SETTLER 3
#define NUM_SENSORS_READ

const uint8_t ESPTwoAddress[] = {0x24, 0x6F, 0x28, 0xB2, 0xD6, 0x84};

static int tempInF = RESET;
static int temp1InF = RESET;
static int temp2InF = RESET;
static int temp3InF = RESET;
static int tempStack = RESET;
static bool firstPass = true;
static uint8_t tempNumRead = RESET;
static uint8_t modifySettler = RESET;

// Structure example to receive data
// Must match the sender structure
typedef struct struct_transmit_packet {
  int temp;
  bool heatOnFlag;
} struct_transmit_packet;

struct_transmit_packet myData;

enum ESP_One_st {
  temp_reading_st, 
  transmit_st, 
  bad_volt_st, 
  bad_temp_st} ESP_One_current_st;

static int volt_to_farenheit(int x) {
  return ((x - 500) / 10) * C_TO_F + TMP36_F_MODIFIER;
}

void tempRead() {
  tempStack = RESET;
  tempNumRead = RESET;
  
  // put your main code here, to run repeatedly:
  temp1InF = volt_to_farenheit(analogRead(TEMP1_READ_PIN));
  temp2InF = volt_to_farenheit(analogRead(TEMP2_READ_PIN));
  temp3InF = volt_to_farenheit(analogRead(TEMP3_READ_PIN));

  if (abs(temp1InF - (temp2InF + temp3InF) / 2) > 2) {
    Serial.print(abs(temp1InF - (temp2InF + temp3InF / 2)));
    Serial.print(" Temp1 ignored ");
    Serial.println(temp1InF);
  } else {
    tempNumRead++;
    tempStack += temp1InF;
  }

  if (abs(temp2InF - (tempStack + temp3InF) / (tempNumRead + 1)) > 2) {
    Serial.print(abs(temp2InF - (tempStack + temp3InF) / (tempNumRead + 1)));
    Serial.print("Temp2 ignored ");
    Serial.println(temp2InF);
  } else {
    tempNumRead++;
    tempStack += temp2InF;
  }

  if (abs(temp3InF - tempStack / tempNumRead) > 2) {
    Serial.print(abs(temp3InF - tempStack / (tempNumRead + 1)));
    Serial.print("Temp3 ignored ");
    Serial.println(temp3InF);
  } else {
    tempNumRead++;
    tempStack += temp3InF;
  }
  
  tempInF = tempStack / tempNumRead;
  if (analogRead(VOLT_READ_PIN) > VOLT_HIGH_BOUND || analogRead(VOLT_READ_PIN) < VOLT_LOW_BOUND) {
    Serial.println("out of volt bounds");
    digitalWrite(BAD_VOLT_LED_PIN, LOW);
    myData.heatOnFlag = false;
    esp_now_send(ESPTwoAddress, (uint8_t *) &myData, sizeof(myData));
    ESP_One_current_st = bad_volt_st;
  }
    
  if (!firstPass && abs(myData.temp - tempInF) > 5) {
    Serial.println("tempature variation too high");
    Serial.print("Old: ");
    Serial.print(myData.temp);
    Serial.print(" New: ");
    Serial.println(tempInF);
    digitalWrite(BAD_TEMP_LED_PIN, LOW);
    myData.heatOnFlag = false;
    esp_now_send(ESPTwoAddress, (uint8_t *) &myData, sizeof(myData));
    ESP_One_current_st = bad_temp_st;
  } else firstPass = false;
  
  myData.temp = tempInF;
  
  Serial.print("TEMP: ");
  Serial.println(tempInF);
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

/*
 * Setup function
 */
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(VOLT_READ_PIN, INPUT);
  pinMode(TEMP1_READ_PIN, INPUT);
  pinMode(TEMP2_READ_PIN, INPUT);
  pinMode(TEMP3_READ_PIN, INPUT);
  pinMode(BAD_VOLT_LED_PIN, OUTPUT);
  pinMode(BAD_TEMP_LED_PIN, OUTPUT);
  digitalWrite(BAD_VOLT_LED_PIN, HIGH);
  digitalWrite(BAD_TEMP_LED_PIN, HIGH);

  firstPass = true;
  ESP_One_current_st = temp_reading_st;

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, ESPTwoAddress, PEER_ADDR_MEM_BYTES);
  peerInfo.channel = ESP_TWO_CHANNEL;  
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  delay(FIVE_SECONDS);
}

/*
 * Loop state machine
 */
void loop() { 
  switch (ESP_One_current_st) {
  case temp_reading_st:
    Serial.println("temp_reading_st");
    tempRead();
    
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
    } else {
      modifySettler = RESET;
    }
    
    break;
   case transmit_st:
    Serial.println("transmit_st");
    
    esp_now_send(ESPTwoAddress, (uint8_t *) &myData, sizeof(myData));
    ESP_One_current_st = temp_reading_st;
    
    break;  
   case bad_volt_st:
    Serial.println("bad_volt_st");
    tempRead();
    Serial.println(analogRead(VOLT_READ_PIN));
    if (analogRead(VOLT_READ_PIN) < VOLT_HIGH_BOUND && analogRead(VOLT_READ_PIN) > VOLT_LOW_BOUND) {
      digitalWrite(BAD_VOLT_LED_PIN, HIGH);
      ESP_One_current_st = temp_reading_st;
    }
    
    break; 
   case bad_temp_st:
    break;
   default:
    Serial.println("Default");
    break;
  }
  
  delay(ONE_SECOND);
}
