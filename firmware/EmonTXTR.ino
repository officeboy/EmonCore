// This #include statement was automatically added by the Spark IDE.
#include "sh1106/sh1106.h"  //Display library

#include "EmonLib/EmonLib.h"             // Include Emon Library
#include "HttpClient/HttpClient.h"
#include "application.h"

//Tinker Variable
/********** Tinker ***********************
int tinkerDigitalRead(String pin);
int tinkerDigitalWrite(String command);
int tinkerAnalogRead(String pin);
int tinkerAnalogWrite(String command);
*****************************************/

/**
* Declaring the variables.
*/
unsigned int nextTime =     0;          // Next time to contact the server
int node_id =               0;
const float Vcal=           136.5;      //Calibration for my US AC-AC adapter (Nokia 3v charger)
const int LEDpin =          D7;         // On-board spark core LED
#define APIKEY <Your Key Here>"
//String rssi = 0;

HttpClient http;
http_request_t request;
http_response_t response;
EnergyMonitor ct1, ct2, ct3, ct4, ct5, ct6;             // Create an instance for each channel

volatile boolean g_writeValue = false;
volatile int g_displayValue = 0;

sh1106_lcd *glcd = NULL;
char charV[10], charCt1[10], charCt2[10], charCt3[10],charCt4[10];



// Headers currently need to be set at init, useful for API keys etc.
http_header_t headers[] = {
    //  { "Content-Type", "application/json" },
    //  { "Accept" , "application/json" },
      { "Accept" , "*/*"},
    { NULL, NULL } // NOTE: Always terminate headers with NULL
};


void setup() {
    // Setup indicator LED
    pinMode(LEDpin, OUTPUT);
    digitalWrite(LEDpin, HIGH);

    Serial.begin(9600);
    
  glcd = sh1106_lcd::getInstance();

  if (glcd != NULL)
  {
    glcd->ClearScreen();
  }


    // (ADC input, calibration, phase_shift)
    ct1.voltage(A0, Vcal, 1.7);  // Voltage: input pin, calibration, phase_shift
    ct2.voltage(A0, Vcal, 1.7);
    ct3.voltage(A0, Vcal, 1.7);
    ct4.voltage(A0, Vcal, 1.7);
    //ct5.voltage(A0, Vcal, 1.7);
    //ct6.voltage(A0, Vcal, 1.7);
    //ct7.voltage(A0, Vcal, 1.7);
    
    // Calibration factor = CT ratio / burden resistance = (100A / 0.05A) / 33 Ohms = 60.606  // (2000 turns / 22 Ohm burden) = 90.9
    ct1.current(A1, 187.5);  //32omh - sct-019     Current: input pin, calibration. 
    ct2.current(A2, 187.5);  //32ohms - sct-019
    ct3.current(A3, 40);  //50ohm - sct-013
    ct4.current(A4, 40);  //50ohm - sct-013
    //ct5.current(A5, 90.9);
    //ct6.current(A6, 90.9);
    //ct7.current(A7, 90.9);
    

/**Tinker Setup
    Spark.function("digitalread", tinkerDigitalRead);
    Spark.function("digitalwrite", tinkerDigitalWrite);
    Spark.function("analogread", tinkerAnalogRead);
    Spark.function("analogwrite", tinkerAnalogWrite);
******tinker end*******/
   
    delay(1000);
    digitalWrite(LEDpin,LOW);

}

void loop() {
    if (nextTime > millis()) {
        return;
    }

    // Available properties: ct1.realPower, ct1.apparentPower, ct1.powerFactor, ct1.Irms and ct1.Vrms
    ct1.calcVI(30,2000);         // Calculate all. No.of half wavelengths (crossings), time-out
    ct2.calcVI(30,2000);
    ct3.calcVI(30,2000);
    ct4.calcVI(30,2000);
    ct1.Vrms = ct1.Vrms*100;
    
    //Send WiFi Signal Strength 
    //String rssi = String(WiFi.RSSI(), DEC);   (rssi:"+String(rssi)+",)  //Needs testing, system locking up. 
    send_data();    //Send data off to web
    update_display();
   nextTime = millis() + 10000;
}


void send_data()
{
  //Serial.println();
  //Serial.println("Application>\tStart of Loop.");
  // Request path and body can be set at runtime or at setup.
  request.hostname = "emoncms.org";
  request.port = 80;
  request.path = "/input/post.json?apikey="APIKEY"&json={Vrms:"+String(ct1.Vrms)+",ct1realpower:"+String(ct1.realPower)+",ct2realpower:"+String(ct2.realPower)+",ct3realpower:"+String(ct3.realPower)+",ct4realpower:"+String(ct4.realPower)+"}";

  // The library also supports sending a body with your request:
  //request.body = "{\"key\":\"value\"}";

  // Get request
  http.get(request, response, headers);

  //Monitoring Options
  //Serial.print("Application>\tResponse status: ");
  //Serial.println(response.status);

  //Serial.print("Application>\tHTTP Response Body: ");
  //Serial.println(response.body);

  digitalWrite(LEDpin,HIGH);
  delay(200);
  digitalWrite(LEDpin,LOW);
}

void update_display()

/*
char str[80];
    strcpy (str," Variable=");
    strcat (str,charVAR);
    strcat (str," !");
    glcd->PrintLine("");
    glcd->PrintLine("");
    glcd->PrintLine(str);   //"Usage:"+charVAR+" Watts");
*/

{
char str[80];
  String(ct1.Vrms).toCharArray(charV, 3); // Convert string to char for display output.
  strcpy (str," V=");
  strcat (str,charV);
  strcat (str,"; ");
  String(ct1.realPower).toCharArray(charCt1, 3); // ct1 watts
  strcpy (str," ct1=");
  strcat (str,charV);
  strcat (str,"w;");
  String(ct2.realPower).toCharArray(charCt2, 3); // ct2 watts
  strcpy (str," ct2=");
  strcat (str,charV);
  strcat (str,"w; ");
  String(ct3.realPower).toCharArray(charCt3, 3); // ct3 watts
  strcpy (str," Ct3=");
  strcat (str,charV);
  strcat (str,"w; ");
  String(ct4.realPower).toCharArray(charCt4, 3); // ct3 watts
  strcpy (str," Ct4=");
  strcat (str,charV);
  strcat (str,"w; ");
  

  //glcd->ClearScreen();
  glcd->PrintLine("");
  glcd->PrintLine(str);   //prints the string we have been building
  //glcd->DrawRectangle(2, 12, 114, 26, 1); // draw a box around the text
  //glcd->Show();
}
//Tinker Code
/***************
int tinkerDigitalRead(String pin) {
    int pinNumber = pin.charAt(1) - '0';
    if (pinNumber< 0 || pinNumber >7) return -1;
    if(pin.startsWith("D")) {
        pinMode(pinNumber, INPUT_PULLDOWN);
        return digitalRead(pinNumber);}
    else if (pin.startsWith("A")){
        pinMode(pinNumber+10, INPUT_PULLDOWN);
        return digitalRead(pinNumber+10);}
    return -2;}

int tinkerDigitalWrite(String command){
    bool value = 0;
    int pinNumber = command.charAt(1) - '0';
    if (pinNumber< 0 || pinNumber >7) return -1;
    if(command.substring(3,7) == "HIGH") value = 1;
    else if(command.substring(3,6) == "LOW") value = 0;
    else return -2;
    if(command.startsWith("D")){
        pinMode(pinNumber, OUTPUT);
        digitalWrite(pinNumber, value);
        return 1;}
    else if(command.startsWith("A")){
        pinMode(pinNumber+10, OUTPUT);
        digitalWrite(pinNumber+10, value);
        return 1;}
    else return -3;}

int tinkerAnalogRead(String pin){
    int pinNumber = pin.charAt(1) - '0';
    if (pinNumber< 0 || pinNumber >7) return -1;
    if(pin.startsWith("D")){
        pinMode(pinNumber, INPUT);
        return analogRead(pinNumber);}
    else if (pin.startsWith("A")){
        pinMode(pinNumber+10, INPUT);
        return analogRead(pinNumber+10);}
    return -2;}

int tinkerAnalogWrite(String command){
    int pinNumber = command.charAt(1) - '0';
    if (pinNumber< 0 || pinNumber >7) return -1;
    String value = command.substring(3);
    if(command.startsWith("D")){
        pinMode(pinNumber, OUTPUT);
        analogWrite(pinNumber, value.toInt());
        return 1;}
    else if(command.startsWith("A")){
        pinMode(pinNumber+10, OUTPUT);
        analogWrite(pinNumber+10, value.toInt());
        return 1;}
    else return -2;}
******End Tinker*************************************/
