#include <SoftwareSerial.h>
#include <AltSoftSerial.h>

// Library for SD card
#include <SD.h>
#include <Wire.h>
#include "RTClib.h"
RTC_DS1307 rtc;

#define chipSelect 10  // for the data logging shield CS line
#define pushswitch 7   

/**** For RS sensor ****SoftwareSerial ****/

// Define the pins for RS485 communication
#define RX 5
#define TX 2
// enable pins
#define RE 4
#define DE 3

// Request frame for the soil sensor
SoftwareSerial modbus(RX, TX);  // Software serial for RS485 communication
/**** For CO2 sensor **** AltSoftwareSerial ****/

//define how the soft serial port is going to work
AltSoftSerial myserial;  // Arduino pin 9 :: Tx; pin 8 :: Rx

String sensorstring = "";                
//String outputstring = "";

boolean sensor_string_complete = false;  
int Co2_value;



void setup() {
  /*For soil sensor*/
  Serial.flush();
  Serial.begin(9600);  
  modbus.begin(9600);  

  pinMode(RE, OUTPUT);  
  pinMode(DE, OUTPUT);  

  /*For CO2 sensor*/
  myserial.begin(9600);
  sensorstring.reserve(20);  
  //outputstring.reserve(60);
  Wire.begin();
  rtc.begin();

  // Initialise SD card
  pinMode(10, OUTPUT);
  if (!SD.begin(10)) {
    //Serial.println("Card failed");
  }

  rtc.begin();
  if (!rtc.begin()) {
    //Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }
  if (!rtc.isrunning()) {
    //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));//CR1220 Battery
  }
  DateTime now = rtc.now();
  File myFile;
  myFile = SD.open("DT.txt", FILE_WRITE);
  if (myFile)
  {
    //delay(50);
    myFile.println();
    myFile.println(String("\t\t\t\t--- Data logging --- ") + now.timestamp(DateTime::TIMESTAMP_DATE));  // This line takes 50 bytes
    //myFile.println();
    myFile.close();
  }
  /**** Set up Push Switch ****/
  pinMode(pushswitch, INPUT_PULLUP);  // switch bridged to GND
}




void loop() {
  Serial.flush();
  DateTime noww = rtc.now(); /*** Try avoid print statement ***/

  // Call functions/sensors
  Co2_value = CO2();

  /**** Push switch operation ****/
  int pushSwitchState = digitalRead(pushswitch);  // pin 7
  File myFile;
  if (pushSwitchState == LOW) {
    delay(50);  // switch debouncing
    float temperature_Celsius;
    float Moisture_Percent = soilSensor(&temperature_Celsius);
    delay(250);
    float air_temperature_Celsius;
    float air_Moisture_Percent = RHSensor(&air_temperature_Celsius);
    //Open file
    myFile = SD.open("DT.txt", FILE_WRITE);
    if (myFile) {
      //myFile.print(noww.timestamp(DateTime::TIMESTAMP_TIME));  // adding this interupts serial time display
      String outputstring= "\tCo2:" + String(Co2_value) + " CM:" + String(Moisture_Percent) + " CT:" + String(temperature_Celsius) + " AM:" + String(air_Moisture_Percent) + " AT:" + String(air_temperature_Celsius);
      //outputstring=String("\t") + Co2_value + String(" ") + Moisture_Percent + String(" ") + temperature_Celsius + String(" ") + air_Moisture_Percent + String(" ") + air_temperature_Celsius;
      //myFile.println(String("\tCo2:") + Co2_value + String(" | CM:") + Moisture_Percent + String(" | CT:") + temperature_Celsius + String(" | AM:") + air_Moisture_Percent + String(" | AT:") + air_temperature_Celsius);
      myFile.println(noww.timestamp(DateTime::TIMESTAMP_TIME)+outputstring);
      // Try avoid unnecessary variables and lines
      myFile.close();
      //myFile.flush();
      //Serial.println("->");
      //Serial.println(outputstring);
      outputstring = "";
    }
  }

  /*******************************************/
  delay(250);  
}

float soilSensor(float *temp) {
  float Temperature_Celsius;
  float Moisture_Percent;
  const byte soilSensorRequest[] = { 0x01, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC4, 0x0B };
  byte soilSensorResponse[9];
  /*Start the transmission mode for RS485 */
  digitalWrite(DE, HIGH);
  digitalWrite(RE, HIGH);
  delay(10);

  // Send the request frame to the soil sensor
  modbus.write(soilSensorRequest, sizeof(soilSensorRequest));

  /*End the transmission mode and set to receive mode for RS485*/
  digitalWrite(DE, LOW);
  digitalWrite(RE, LOW);
  delay(10);

  // Wait for the response from the sensor or timeout after 1 second
  unsigned long startTime = millis();
  while (modbus.available() < 9 && millis() - startTime < 1000) {
    delay(1);  // help continue the loop
  }
  if (modbus.available() >= 9)  // If valid response received
  {
    // Read the response from the sensor
    byte index = 0;
    while (modbus.available() && index < 9) {
      soilSensorResponse[index] = modbus.read();
      //Serial.print(soilSensorResponse[index], HEX); // Print the received byte in HEX format
      //Serial.print(" ");
      index++;
    }
    //Serial.println("Compost sensor Response Read");
    /* Parse and calculate the Moisture value */

    int Moisture_Int = int(soilSensorResponse[5] << 8 | soilSensorResponse[6]);
    Moisture_Percent = Moisture_Int / 10.0;

    //Serial.println(String("->Moisture content: ")+Moisture_Percent+ String(" %"));
    /* Parse and calculate the Temperature value */

    int Temperature_Int = int(soilSensorResponse[3] << 8 | soilSensorResponse[4]);
    Temperature_Celsius = Temperature_Int / 10.0;
    *temp = Temperature_Celsius;

  }
  return Moisture_Percent;
}


int CO2() {

  int Co2;
  if (myserial.available() > 0) {  
    char inchar = (char)myserial.read();  
    if (inchar != '\r') {                 
      sensorstring += inchar;             
      if (isdigit(sensorstring[0])) {
        Co2 = sensorstring.toInt();
      }
    }

    else {
      sensorstring = "";  // decouple readings per
      sensor_string_complete = true;
    }
    if (sensor_string_complete == true) {  
      Co2_value = Co2;

    }
    sensor_string_complete = false;  
  }
  //modbus.listen();
  return Co2_value;
}


float RHSensor(float *air_temp) {  //float *air_temp
  float air_Temperature_Celsius;
  float air_Moisture_Percent;
  const byte RHSensorRequest[] = { 0x02, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC4, 0x38 };
  byte RHSensorResponse[9];
  /*Start the transmission mode for RS485 */
  digitalWrite(DE, HIGH);
  digitalWrite(RE, HIGH);
  delay(10);

  // Send the request frame to the soil sensor
  modbus.write(RHSensorRequest, sizeof(RHSensorRequest));

  /*End the transmission mode and set to receive mode for RS485*/
  digitalWrite(DE, LOW);
  digitalWrite(RE, LOW);
  delay(10);

  // Wait for the response from the sensor or timeout after 1 second
  unsigned long startTime = millis();
  while (modbus.available() < 9 && millis() - startTime < 1000) {
    delay(1);  
    /**** Reading response frame and store it ****/
    if (modbus.available() >= 9){
      byte index = 0;
      while (modbus.available() && index < 9) {
        RHSensorResponse[index] = modbus.read();
        //Serial.print(soilSensorResponse[index], HEX); // Print the received byte in HEX format
        //Serial.print(" ");
        index++;
      }
      /* Parse and calculate the Moisture value */

      int Moisture_Int = int(RHSensorResponse[5] << 8 | RHSensorResponse[6]);
      air_Moisture_Percent = Moisture_Int / 10.0;

      //Serial.println(String("->Moisture content: ")+Moisture_Percent+ String(" %"));
      /* Parse and calculate the Temperature value */

      int Temperature_Int = int(RHSensorResponse[3] << 8 | RHSensorResponse[4]);
      air_Temperature_Celsius = Temperature_Int / 10.0;
      *air_temp = air_Temperature_Celsius;
    }

  } 
  return air_Moisture_Percent;
}

