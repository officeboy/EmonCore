// This #include statement was automatically added by the Spark IDE.
//#include "EmonLib/EmonLib.h"             // Include Emon Library
#include "EmonLib.h"             // Include Emon Library
#include "HttpClient/HttpClient.h"
#include "application.h"

//Tinker Variable
int tinkerDigitalRead(String pin);
int tinkerDigitalWrite(String command);
int tinkerAnalogRead(String pin);
int tinkerAnalogWrite(String command);

/**
* Declaring the variables.
*/
unsigned int nextTime =     0;          // Next time to contact the server
int node_id =               0;
const float Vcal=           136.5;      //Calibration for my US AC-AC adapter (Nokia 3v charger)
const int LEDpin =          D7;         // On-board spark core LED
#define APIKEY "<Your Key Here>"

//-----------------------RFM12B / RFM69CW SETTINGS----------------------------------------------------------------------------------------------------
//#define RF_freq RF12_433MHZ                                     // Frequency of RF69CW module can be RF12_433MHZ, RF12_868MHZ or RF12_915MHZ. You should use the one matching the module you have.
//byte nodeID = 10;                                               // emonTx RFM12B node ID
//const int networkGroup = 210;  
typedef struct { 
int power1, power2, power3, power4, Vrms; //temp[MaxOnewire]; 
int pulseCount; 
} PayloadTX;     // create structure - a neat way of packaging data for RF comms
  PayloadTX emontx; 
//-------------------------------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------------------------------

HttpClient http;
http_request_t request;
http_response_t response;
EnergyMonitor ct1;             // Create an instance for each channel




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
    
    // Request path and body can be set at runtime or at setup.
    request.hostname = "emoncms.org";
    request.port = 80;
    
    // (ADC input, calibration, phase_shift)
    ct1.voltage(A0, Vcal, 1.7);  // Voltage: input pin, calibration, phase_shift
    
    // Calibration factor = CT ratio / burden resistance = (100A / 0.05A) / 33 Ohms = 60.606  // (2000 turns / 22 Ohm burden) = 90.9
    ct1.current(A1, 90.9);       // Current: input pin, calibration.
    
//Tinker Setup
    Spark.function("digitalread", tinkerDigitalRead);
    Spark.function("digitalwrite", tinkerDigitalWrite);
    Spark.function("analogread", tinkerAnalogRead);
    Spark.function("analogwrite", tinkerAnalogWrite);

    delay(1000);
    digitalWrite(LEDpin,LOW);

}

void loop() {
    if (nextTime > millis()) {
        return;
    }
    emontx.Vrms = 0;                      //Set Vrms to zero, this will be overwirtten by CT 1+
    
    
    ct1.calcVI(20,2000);         // Calculate all. No.of half wavelengths (crossings), time-out
    emontx.Vrms = ct1.Vrms*100;
    
    // Available properties: ct1.realPower, ct1.apparentPower, ct1.powerFactor, ct1.Irms and ct1.Vrms

    Serial.println();
    Serial.println("Application>\tStart of Loop.");

    request.path = "/input/post.json?apikey="APIKEY"&json={ct1Vrms:"+String(emontx.Vrms)+",ct1Irms:"+String(ct1.Irms)+",NumSamp:"+String(ct1.samp)+",ct1realpower:"+String(ct1.realPower)+"}";

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

   nextTime = millis() + 10000;
}

//Tinker Code

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
