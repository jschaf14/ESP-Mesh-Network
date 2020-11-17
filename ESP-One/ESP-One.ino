void setup() {
   // put your setup code here, to run once:
   pinMode(15, OUTPUT);
 }
 
 void loop() {
   // put your main code here, to run repeatedly:
   digitalWrite(15, HIGH);
   delay(1000);
   digitalWrite(15, LOW);
   delay(1000);
 }
