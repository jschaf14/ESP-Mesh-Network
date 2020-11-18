#define TEMP1_READ_PIN 2
#define MS_TO_SEC 1000
#define ONE_SECOND (1 * MS_TO_SEC)
#define FIVE_SECONDS (5 * MS_TO_SEC)
#define HEAT_ENABLE_AT 21
#define HEAT_DISABLE_AT 24

static int tempVoltRead = 0;
static int tempInCelsius = 0;
static bool heatOnFlag = false;
enum ESP_One_st {temp_reading_st, transmit_st} ESP_One_current_st;

static int volt_to_celsius(int x) {
  return ((x - 500) / 10);
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(TEMP1_READ_PIN, INPUT);
  delay(ONE_SECOND);
}
 
void loop() {
  // put your main code here, to run repeatedly:
  tempVoltRead = analogRead(TEMP1_READ_PIN);
  Serial.print("Pin 2 Read: ");
  Serial.println(tempVoltRead);
  tempInCelsius = volt_to_celsius(tempVoltRead);
  Serial.println(tempInCelsius);
  
  
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
    ESP_One_current_st = temp_reading_st;
    break;
  }
  delay(ONE_SECOND);
}
