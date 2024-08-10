#define ADC1 PE_3

void setup() {
  // put your setup code here, to run once:
Serial.begin(115200);
pinMode(ADC1, INPUT);
}

void loop() {
  // put your main code here, to run repeatedly: 
  char c;
if(Serial.available()>0){
  c = Serial.read();
  if (c == '#'){
    for (int i =0; i < 10; i++){
      Serial.println(analogRead(ADC1));
//      Serial.print(",");
    delay(500);  
    }
    Serial.println('@');
  }

}
}
