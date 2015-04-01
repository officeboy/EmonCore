/*
  
  emonSparkCore V3.4 Discrete Sampling
  
  If AC-AC adapter is detected assume emonTx is also powered from adapter (jumper shorted) and take Real Power Readings and disable sleep mode to keep load on power supply constant
  If AC-AC addapter is not detected assume powering from battereis / USB 5V AC sample is not present so take Apparent Power Readings and enable sleep mode
  
  Transmitt values via WiFi
  
   -----------------------------------------
  Part of the openenergymonitor.org project
  
  Authors: Glyn Hudson & Trystan Lea 
  Builds upon HttpClient, OpenEnergyMonitor's EmonLib library and Spark
  
  Licence: GNU GPL V3
	Modified for Spark Core by Dustin Sellinger
*/

// This #include statement was automatically added by the Spark IDE.
#include "EmonLib/EmonLib.h"             // Include Emon Library
//#include "EmonLib.h"             // Include Emon Library
#include "HttpClient/HttpClient.h"
#include "application.h"
#define cli noInterrupts
#define sei interrupts

//Tinker Variable
//int tinkerDigitalRead(String pin);
//int tinkerDigitalWrite(String command);
//int tinkerAnalogRead(String pin);
//int tinkerAnalogWrite(String command);

/**
* Declaring the variables.
*/
EnergyMonitor ct1, ct2, ct3, ct4;     
//----------------------------SparkEmonTx V3 Settings---------------------------------------------------------------------------------------------------------------
const byte Vrms=                  120;                               // Vrms for apparent power readings (when no AC-AC voltage sample is present)
const byte TIME_BETWEEN_READINGS= 10;            //Time between readings   

//http://openenergymonitor.org/emon/buildingblocks/calibration

const float Ical1=                90.9;                                 // (2000 turns / 22 Ohm burden) = 90.9
const float Ical2=                90.9;                                 // (2000 turns / 22 Ohm burden) = 90.9
const float Ical3=                90.9;                                 // (2000 turns / 22 Ohm burden) = 90.9
const float Ical4=                16.67;                               // (2000 turns / 120 Ohm burden) = 16.67

float Vcal=                       268.97;                             // (230V x 13) / (9V x 1.2) = 276.9 Calibration for UK AC-AC adapter 77DB-06-09 
//float Vcal=276.9;
//const float Vcal=               260;     
unsigned int nextTime =     0;          // Next time to contact the server
int node_id =               0;
const float Vcal_USA=       	  136.5;      //Calibration for my US AC-AC adapter (Nokia 3v charger)
boolean USA=TRUE; 

const float phase_shift=          1.7;
const int no_of_samples=          1480; 
const int no_of_half_wavelengths= 20;
const int timeout=                2000;                               //emonLib timeout 
const int ACAC_DETECTION_LEVEL=   3000;
const int TEMPERATURE_PRECISION=  11;                 //9 (93.8ms),10 (187.5ms) ,11 (375ms) or 12 (750ms) bits equal to resplution of 0.5C, 0.25C, 0.125C and 0.0625C
const byte MaxOnewire=             6;                            // +1 since arrya starts at 0. maximum number of DS18B20 one wire sensors
#define ASYNC_DELAY 375                                          // DS18B20 conversion delay - 9bit requres 95ms, 10bit 187ms, 11bit 375ms and 12bit resolution takes 750ms
//-------------------------------------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------------------------------------

//----------------------------emonTx V3 hard-wired connections--------------------------------------------------------------------------------------------------------------- 
const byte LEDpin =         		   D7;         			// On-board spark core LED
//const byte DS18B20_PWR=            19;                  // DS18B20 Power
//const byte DIP_switch1=            8;                   // Voltage selection 230 / 110 V AC (default switch off 230V)  - switch off D8 is HIGH from internal pullup
//const byte DIP_switch2=            9;                   // RF node ID (default no chance in node ID, switch on for nodeID -1) switch off D9 is HIGH from internal pullup
//const byte battery_voltage_pin=    7;                   // Battery Voltage sample from 3 x AA
const byte pulse_countINT=         1;                   // INT 1 / Dig 3 Terminal Block / RJ45 Pulse counting pin(emonTx V3.4) - (INT0 / Dig2 emonTx V3.2)
const byte pulse_count_pin=        3;                   // INT 1 / Dig 3 Terminal Block / RJ45 Pulse counting pin(emonTx V3.4) - (INT0 / Dig2 emonTx V3.2)
//#define ONE_WIRE_BUS               5                    // DS18B20 Data                     
//-------------------------------------------------------------------------------------------------------------------------------------------

/******  No Temp Support  *********************
//Setup DS128B20
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
byte allAddress [MaxOnewire][8];  // 8 bytes per address
byte numSensors;
**********************************************/
//-------------------------------------------------------------------------------------------------------------------------------------------

#define APIKEY "b5ea4f694e9478fe6c8704a75cd4c2df"

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

//Random Variables 
//boolean settled = false;
boolean CT1, CT2, CT3, CT4, ACAC, debug, DS18B20_STATUS; 
byte CT_count=0;
volatile byte pulseCount = 0;


// Headers currently need to be set at init, useful for API keys etc.
http_header_t headers[] = {
    //  { "Content-Type", "application/json" },
    //  { "Accept" , "application/json" },
      { "Accept" , "*/*"},
    { NULL, NULL } // NOTE: Always terminate headers with NULL
};

void setup() 
{
    pinMode(LEDpin, OUTPUT); 	// Setup indicator LED
	emontx.pulseCount=0;                                        // Make sure pulse count starts at zero
    digitalWrite(LEDpin, HIGH);  

    Serial.begin(9600);

 if (USA==TRUE){                                                                             //if USA mode is true
  Vcal=Vcal_USA;                                                                               //Assume USA AC/AC adatper is being used, set calibration accordingly 
 } 
   
  if (analogRead(1) > 0) {CT1 = 1; CT_count++;} else CT1=0;              // check to see if CT is connected to CT1 input, if so enable that channel
  if (analogRead(2) > 0) {CT2 = 1; CT_count++;} else CT2=0;              // check to see if CT is connected to CT2 input, if so enable that channel
  if (analogRead(3) > 0) {CT3 = 1; CT_count++;} else CT3=0;              // check to see if CT is connected to CT3 input, if so enable that channel
  if (analogRead(4) > 0) {CT4 = 1; CT_count++;} else CT4=0;              //  check to see if CT is connected to CT4 input, if so enable that channel
  
  if ( CT_count == 0) CT1=1;                                              // If no CT's are connect ed CT1-4 then by default read from CT1
  // Quick check to see if there is a voltage waveform present on the ACAC Voltage input
  // Check consists of calculating the RMS from 100 samples of the voltage input.
  delay(10000);            //wait for settle
  digitalWrite(LEDpin,LOW); 
  
  // Calculate if there is an ACAC adapter on analog input 0
  //double vrms = calc_rms(0,1780) * (Vcal * (3.3/1024) );
  double vrms = calc_rms(0,1780) * 0.87;
  if (vrms>90) ACAC = 1; else ACAC=0;
  if (ACAC) 
  {
    for (int i=0; i<10; i++)                                              // indicate AC has been detected by flashing LED 10 times
    { 
      digitalWrite(LEDpin, HIGH); delay(200);
      digitalWrite(LEDpin, LOW); delay(300);
    }
  }
  else 
  {
    delay(1000);
    digitalWrite(LEDpin, HIGH); delay(2000); digitalWrite(LEDpin, LOW);   // indicate DC power has been detected by turing LED on then off
  }
     
    // (ADC input, calibration, phase_shift)
    //ct1.voltage(A0, Vcal, 1.7);  // Voltage: input pin, calibration, phase_shift
    
    // Calibration factor = CT ratio / burden resistance = (100A / 0.05A) / 33 Ohms = 60.606  // (2000 turns / 22 Ohm burden) = 90.9
    //ct1.current(A1, 90.9);       // Current: input pin, calibration.
	
  if (Serial.available()) debug = 1; else debug=0;          //if serial UART to USB is connected show debug O/P. If not then disable serial
  if (debug==1)
  {
    Serial.print("CT 1 Calibration: "); Serial.println(Ical1);
    Serial.print("CT 2 Calibration: "); Serial.println(Ical2);
    Serial.print("CT 3 Calibration: "); Serial.println(Ical3);
    Serial.print("CT 4 Calibration: "); Serial.println(Ical4);
    delay(1000);

    Serial.print("RMS Voltage on AC-AC Adapter input is: ~");
    Serial.print(vrms,0); Serial.println("V");
      
    if (ACAC) 
    {
      Serial.println("AC-AC adapter detected - Real Power measurements enabled");
      Serial.println("assuming powering from AC-AC adapter (jumper closed)");
      if (USA==TRUE) Serial.println("USA mode activated"); 
      Serial.print("Vcal: "); Serial.println(Vcal);
      Serial.print("Phase Shift: "); Serial.println(phase_shift);
    }
     else 
     {
       Serial.println("AC-AC adapter NOT detected - Apparent Power measurements enabled");
       Serial.print("Assuming VRMS to be "); Serial.print(Vrms); Serial.println("V");
       Serial.println("Assuming powering from batteries / 5V USB - power saving mode enabled");
     }  

    if (CT_count==0) Serial.println("NO CT's detected, sampling from CT1 by default");
    else   
    {
      if (CT1) Serial.println("CT 1 detected");
      if (CT2) Serial.println("CT 2 detected");
      if (CT3) Serial.println("CT 3 detected");
      if (CT4) Serial.println("CT 4 detected");
    }
    /************  No Temp Support  ****************
	if (DS18B20_STATUS==1) {Serial.print("Detected "); Serial.print(numSensors); Serial.println(" DS18B20..using this for temperature reading");}
      else Serial.println("Unable to detect DS18B20 temperature sensor");
	************************************************/
	
    #if (RF69_COMPAT)
       Serial.println("RFM69CW Initiated: ");
    #else
      Serial.println("RFM12B Initiated: ");
    #endif
    
   /******  WiFi only for now  ********** 
    Serial.print("Node: "); Serial.print(nodeID); 
    Serial.print(" Freq: "); 
    if (RF_freq == RF12_433MHZ) Serial.print("433Mhz");
    if (RF_freq == RF12_868MHZ) Serial.print("868Mhz");
    if (RF_freq == RF12_915MHZ) Serial.print("915Mhz"); 
    Serial.print(" Network: "); Serial.println(networkGroup);
	**********************************/
    Serial.print("CT1 CT2 CT3 CT4 VRMS/BATT PULSE");
    // if (DS18B20_STATUS==1){Serial.print(" Temperature 1-"); Serial.print(numSensors);}   // No Temp Support
    Serial.println(" "); 
   delay(500);  

  }
  else 
    Serial.end();
  
  
    
  if (CT1) ct1.current(1, Ical1);             // CT ADC channel 1, calibration.  calibration (2000 turns / 22 Ohm burden resistor = 90.909)
  if (CT2) ct2.current(2, Ical2);             // CT ADC channel 2, calibration.
  if (CT3) ct3.current(3, Ical3);             // CT ADC channel 3, calibration. 
  //CT 3 is high accuracy @ low power -  4.5kW Max @ 240V 
  if (CT4) ct4.current(4, Ical4);                                      // CT channel ADC 4, calibration.    calibration (2000 turns / 120 Ohm burden resistor = 16.66)


  if (ACAC)
  {
    if (CT1) ct1.voltage(0, Vcal, phase_shift);          // ADC pin, Calibration, phase_shift
    if (CT2) ct2.voltage(0, Vcal, phase_shift);          // ADC pin, Calibration, phase_shift
    if (CT3) ct3.voltage(0, Vcal, phase_shift);          // ADC pin, Calibration, phase_shift
    if (CT4) ct4.voltage(0, Vcal, phase_shift);          // ADC pin, Calibration, phase_shift
  }

if (DS18B20_STATUS==0) attachInterrupt(pulse_countINT, onPulse, FALLING);     // Attach pulse counting interrupt pulse counting only when no Temperature sensor is connected as they both use the same port
 
   
//Tinker Setup
//Spark.function("digitalread", tinkerDigitalRead);
//Spark.function("digitalwrite", tinkerDigitalWrite);
//Spark.function("analogread", tinkerAnalogRead);
//Spark.function("analogwrite", tinkerAnalogWrite);

} //end SETUP

void loop() 
{
  if (ACAC) {
    delay(200);                         //if powering from AC-AC allow time for power supply to settle    
    emontx.Vrms=0;                      //Set Vrms to zero, this will be overwirtten by CT 1-4
  }
  
  if (CT1) 
  {
   if (ACAC) 
   {
     ct1.calcVI(no_of_half_wavelengths,timeout); emontx.power1=ct1.realPower;
     emontx.Vrms=ct1.Vrms*100;
   }
   else
     emontx.power1 = ct1.calcIrms(no_of_samples)*Vrms;                               // Calculate Apparent Power 1  1480 is  number of sample

  }
  
  if (CT2) 
  {
   if (ACAC) 
   {
     ct2.calcVI(no_of_half_wavelengths,timeout); emontx.power2=ct2.realPower;
     emontx.Vrms=ct2.Vrms*100;
   }
   else
     emontx.power2 = ct2.calcIrms(no_of_samples)*Vrms;                               // Calculate Apparent Power 1  1480 is  number of samples

  }

  if (CT3) 
  {
   if (ACAC) 
   {
     ct3.calcVI(no_of_half_wavelengths,timeout); emontx.power3=ct3.realPower;
     emontx.Vrms=ct3.Vrms*100;
   }
   else
     emontx.power3 = ct3.calcIrms(no_of_samples)*Vrms;                               // Calculate Apparent Power 1  1480 is  number of samples

  }
  

  if (CT4) 
  {
   if (ACAC) 
   {
     ct4.calcVI(no_of_half_wavelengths,timeout); emontx.power4=ct4.realPower;
     emontx.Vrms=ct4.Vrms*100;
   }
   else
     emontx.power4 = ct4.calcIrms(no_of_samples)*Vrms;                               // Calculate Apparent Power 1  1480 is  number of samples

  }
  

/******  No Battery Support for now   ****** 
 if (!ACAC){                                                                                         //read battery voltage if powered by DC
    int battery_voltage=analogRead(battery_voltage_pin) * 0.681322727;     //6.6V battery = 3.3V input = 1024 ADC
    emontx.Vrms= battery_voltage;
  }
********************************************/
  
  
  if (pulseCount)                                                       // if the ISR has counted some pulses, update the total count
  {
    cli();                                                             // Disable interrupt just in case pulse comes in while we are updating the count
    emontx.pulseCount += pulseCount;
    pulseCount = 0;
    sei();                                                            // Re-enable interrupts
  }
 
  if (debug==1) {
    Serial.print(emontx.power1); Serial.print(" ");
    Serial.print(emontx.power2); Serial.print(" ");
    Serial.print(emontx.power3); Serial.print(" ");
    Serial.print(emontx.power4); Serial.print(" ");
    Serial.print(emontx.Vrms); Serial.print(" ");
    Serial.print(emontx.pulseCount); Serial.print(" ");
    /****** No Temp Support *************
	if (DS18B20_STATUS==1){
      for(byte j=0;j<numSensors;j++){
        Serial.print(emontx.temp[j]);
       Serial.print(" ");
      } 
	} 
	********************************************/

    Serial.println(" ");
    delay(50);
  } 
  
  if (ACAC) {digitalWrite(LEDpin, HIGH); delay(200); digitalWrite(LEDpin, LOW);}    // flash LED if powered by AC
  
  send_rf_data();                                                       // *SEND RF DATA* - see emontx_lib
    
    if (ACAC)                                                               //If powered by AC-AC adaper (mains power) then delay instead of sleep
    {
     delay(TIME_BETWEEN_READINGS*1000);
    }
    else                                                                  //if powered by battery then sleep rather than dealy and disable LED to lower energy consumption  
      delay(TIME_BETWEEN_READINGS*1000);                                  // sleep or delay in seconds 
}


void send_rf_data()
{
    // Request path and body can be set at runtime or at setup.
    request.hostname = "emoncms.org";
    request.port = 80;
	request.path = "/input/post.json?apikey="APIKEY"&json={"&emontx"}";
	//request.path = "/input/post.json?apikey="APIKEY"&json={ct1Vrms:"+String(emontx.Vrms)+",ct1Irms:"+String(ct1.Irms)+",NumSamp:"+String(ct1.samp)+",ct1realpower:"+String(ct1.realPower)+",string:"+emontx+"}";
  // Get request
    http.get(request, response, headers);
    
    //Monitoring Options
    //Serial.print("Application>\tResponse status: ");
    //Serial.println(response.status);


    //Serial.print("Application>\tHTTP Response Body: ");
    //Serial.println(response.body);

//  rf12_sleep(RF12_WAKEUP);                                   
//  rf12_sendNow(0, &emontx, sizeof emontx);                           //send temperature data via RFM12B using new rf12_sendNow wrapper
//  rf12_sendWait(2);
//  if (!ACAC) rf12_sleep(RF12_SLEEP);                             //if powred by battery then put the RF module into sleep inbetween readings 
}


double calc_rms(int pin, int samples)
{
  unsigned long sum = 0;
  for (int i=0; i<samples; i++) // 178 samples takes about 20ms
  {
    int raw = (analogRead(0)-512);
    sum += (unsigned long)raw * raw;
  }
  double rms = sqrt((double)sum / samples);
  return rms;
}

// The interrupt routine - runs each time a falling edge of a pulse is detected
void onPulse()                  
{
   pulseCount++;
}

/**********  No Temp Support  **************************
int get_temperature(byte sensor)                
{
  float temp=(sensors.getTempC(allAddress[sensor]));
  if ((temp<125.0) && (temp>-55.0)) return(temp*10);            //if reading is within range for the sensor convert float to int ready to send via RF
  }
********************************************************/