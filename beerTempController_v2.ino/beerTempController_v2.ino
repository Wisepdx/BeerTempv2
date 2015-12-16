#include <Bridge.h>
#include <YunClient.h>
#include <SPI.h> // Serial Peripheral Interface library
#include <DallasTemperature.h>  // Temp Sensor library
#include <OneWire.h>  // For One Wire Temp Sensor Library
#include <Mailbox.h>  // Mailbox Library
#include <Console.h>  // Mailbox Library

// Temperature Sensors
#define ALLTEMP 0
#define CURRENT 3
#define PELTIER 2
#define AMBIENT 1

// Motor Controller
#define BRAKEVCC 0
#define CW 1
#define CCW 2
#define BRAKEGND 3
#define CS_THRESHOLD 100

// One Wire Temperature Sensors using Digital Pin 2
OneWire TempSensorPin(2);
DallasTemperature TempSensor(&TempSensorPin);



/*
Motor Controller
xxx[0] controls '1' outputs (PUMP)
xxx[1] controls '2' outputs (PELTIER)
*/
int inApin[2] = {7, 4};  // INA: Clockwise input
int inBpin[2] = {8, 9};  // INB: Counter-clockwise input {8, 9};
int pwmpin[2] = {5, 6};  // PWM input
int cspin[2] = {A2, A3}; // CS: Current sense ANALOG input
int enpin[2] = {A0, A1}; // EN: Status of switches output (Analog pin)

/*-------------- Datapoints ----------------
--------------------------------------------*/
int batchId = 0;  // beer ID (always make 3 digit)
int batchIdOld = 0;  // beer ID (always make 3 digit)
String batchName = "unknown"; // beer name
int batchSize = 10;  // beer batch size
int targetTemp = 68;  // target temp of beer (In Fahrenheit)
int pumpStatus = 0; // pump Status (0 = Off, 1 = On)
int peltStatus = 0; // peltier status (0 = Off, 1 = Cool, 2 = Heat)
float currentTemp;  // current temperature of beer (In Fahrenheit)
float ambientTemp;  // ambient temperature of room (In Fahrenheit)
int tempDiff = 1; // range at which temperature can drift from target
int targetTempHigh = targetTemp + tempDiff; // high end of temp range
int targetTempLow = targetTemp - tempDiff;  // low end of temp range
int postType = 0; // 1 = BatchData, 2 = Sensor Data
String data = ""; // holds data to POST
String dataTemp; // temp hold area for int and floats when printing into data

//IP Address of the sever on which there is the WS: http://www.mywebsite.com/
IPAddress server(10,0,1,114);

//YunClient client;
BridgeClient client;

/*
------------------------------
SETUP LOOP START HERE
------------------------------
*/

void setup() { 
  Bridge.begin();
  Mailbox.begin();
  Console.begin();
  Serial.begin(115200);

  // start Temperature Sensors
  TempSensor.begin();
  // motor Shield Temp Setup (Set to Off)
  motorOff(0);
  motorOff(1);

  delay(10000);
  debug("Starting","Session");
  
  //Check connection to server
  if (client.connect(server, 80)) {
    Serial.println("connected to server");
    Console.println("connected to server");
    delay(2500);
  } else {
    Serial.println("connection to server failed");
    Console.println("connection to server failed");
  }

}

/*
-----------------------------
VOID LOOP START HERE
-----------------------------
*/
void loop(){
  Console.println( "Start of Loop");

  // set postType to 0 if Batch ID = 0 to postpone data collection;
  if (batchId == 0){
    postType = 0;
    debug("PostType",String(postType));
  } 

  // check post type and build post data
  if (postType != 0 ){

    //POST TYPE 1
    
    if (postType == 1){
    
      debug("PostType",String(postType));
      
      //compile data var from batch vars 
      dataWriteBatch();
      
      //POST Data
      postData();
      
      // set post type to 2 to start collecting sensor data
      postType = 2;
      
    } 

    //POST TYPE 2
    
    else if (postType == 2){
      
      debug("PostType",String(postType));
      
      // check temperatures against optimum settings and turn pump/peltier on or off, update screen with new statuses and temps
      // includes function to write datafiles and update screen every minute
      motorCheck();
      
      //compile data var from sensor data
      dataWriteSensors();
      
      //POST Data
      postData();
    }    
    
      //Check Mailbox for new data
      mailboxCheck();
      
      // wait 5 minutes and then loop
      delay(60000);
    
  } else if (postType == 0){
    
    debug("","no post type, waiting for input...");
    // wait for mailbox request
    mailboxCheck();
    delay(10000);  // wait 10 seconds and check again 
  }
  
  delay(10000); //wait 10 seconds
  Console.println("End of Loop");
}

/*
------------------------------------
FUNCTIONS START BELOW HERE
------------------------------------
*/

/*----- DEBUG ------------------------------
--------------------------------------------*/
//debug function: outputs to Console (web) and Serial (local usb)
void debug(String valueHeader, String value){
  if (valueHeader == ""){
    Console.print(valueHeader + ": ");
    Serial.print(valueHeader + ": ");
  }
  Console.println(value);
  Serial.println(value);
  Console.println("----");
  Serial.println("----");
}


/*----- DATA WRITING -----------------------
--------------------------------------------*/

void dataWriteBatch(){
  //create a BEER BATCH POST
  
  //clear data
  data = "";
  // input post Type
  data += "postType=1";
  // input batch ID
  dataTemp = String(batchId,5);
  data += "&batchId=" + dataTemp;
  // input batch name
  data += "&batchName=" + batchName;
  // input batch size
  dataTemp = batchSize;
  data += "&batchSize=" + dataTemp;
  
  // write debug of data
  Console.println("Full data string: " + data);
  Console.println("----");
}
void dataWriteSensors(){
  //create a SENSOR POST  
  
  //clear sensor data
  data = "";
  // input post Type
  data += "postType=2";
  // input batch ID
  dataTemp = String(batchId,5);
  data += "&batchId=" + dataTemp;
  // input targetTemp
  dataTemp = String(targetTemp); //removed ,3 from parens
  data += "&targetTemp=" + dataTemp;
  // input currentTemp
  dataTemp = String(currentTemp,3);
  data += "&currentTemp=" + dataTemp;
  // input ambientTemp
  dataTemp = String(ambientTemp,3);
  data += "&ambientTemp=" + dataTemp;
  // input peltStatus
  dataTemp = String(peltStatus); 
  data += "&peltStatus=" + dataTemp;
  // input tempDiff
  dataTemp = String(tempDiff); 
  data += "&tempDiff=" + dataTemp; 
  // write debug of data
  Console.println("Full data string: " + data);
  Console.println("----");
}

void postData(){
  if (client.connect("beerdev.wisepdx.net",80)) { // REPLACE WITH YOUR SERVER ADDRESS
    client.println("POST /add.php HTTP/1.1"); 
    Console.println("POST /add.php HTTP/1.1");
    client.println("Host: beerdev.wisepdx.net"); // SERVER ADDRESS HERE TOO
    Console.println("Host: beerdev.wisepdx.net");
    client.println("Accept: */*");
    Console.println("Accept: */*"); 
    client.println("Content-Length: " + String(data.length())); 
    Console.println("Content-Length: " + String(data.length())); 
    //client.println(data.length());
    client.println("Content-Type: application/x-www-form-urlencoded"); 
    Console.println("Content-Type: application/x-www-form-urlencoded"); 
    client.println(); 
    client.println(data);
    Console.println(data);
  } 
  
  if (client.connected()) { 
    client.stop();  // DISCONNECT FROM THE SERVER
  }
}

/*----- TEMPERATURE ------------------------
--------------------------------------------*/
void readTemp(){
  TempSensor.requestTemperatures();
  ambientTemp = TempSensor.getTempFByIndex(0);  // Black Small Sensor
  currentTemp = TempSensor.getTempFByIndex(1);  // (Metal Probe Sensor)

  // write debug of Temp
  debug("Ambient Temp", String(ambientTemp,3));
  debug("Current Temp", String(currentTemp,3));
}

/*----- PARSE MAILBOX MESSAGE ----------------
--------------------------------------------*/
void mailboxCheck(){
  String message;
  // if there is a message in the Mailbox
  if (Mailbox.messageAvailable()){
    
    for (int i=1;i<4;i++){
      digitalWrite(13,HIGH);
      delay(1000);
      digitalWrite(13,LOW);
    }
    
    digitalWrite(13,HIGH);
    // read all the messages present in the queue
    while (Mailbox.messageAvailable()){
      Mailbox.readMessage(message);
        // write messages in debug
        debug("mailbox",message);
      String variableName = "";
      String variableValue = "";
      bool readingName = false;

      // loop though the message (in HTTP GET format)
      for(int i = 0; i < message.length();i++){
        // do somthing with each chr
        char currentCharacter = message[i];
        if (i == 0){
          // the first letter will always be the name
          readingName = true;
          variableName += currentCharacter;
        } else if(currentCharacter == '&'){
          // starting the next variable
          readingName = true;

          // if there is a name recorded
          if (variableName != ""){
            // Write variables
            recordVariablesFromWeb(variableName, variableValue);
            // Reset variables for possible next in string 'message'
            variableName = "";
            variableValue = "";
          }
        } else if (currentCharacter == '='){
          // now reading the value
          readingName = false;
        } else {
          if(readingName == true){
            // add the current letter to the name
            variableName += currentCharacter;
          } else{
            // add the current letter to the value
            variableValue += currentCharacter;
          }
        }
      }
      recordVariablesFromWeb(variableName, variableValue);
      digitalWrite(13, LOW); // after Message Read turn LED13 off
    }
  }
}

void recordVariablesFromWeb(String variableName, String variableValue){
  // set values to their particular variables in this section
  // parse Variables to the Proper Variable

  if(variableName == "batchid"){
    // if batch ID does not equal current batch ID flag as a new postType
    if (batchId != variableValue.toInt()){
        postType = 1; // flagged as a new beer post
        batchId = variableValue.toInt();
      } else {
        batchId = variableValue.toInt();
      }
  }
  else if(variableName == "batchname"){
    batchName = variableValue;
  }
  else if(variableName == "batchsize"){
    batchSize = variableValue.toInt();
  }
  else if(variableName == "tempdiff"){
    tempDiff = variableValue.toInt();
  }
  else if(variableName == "targettemp"){
    targetTemp = variableValue.toInt();
    // set high/low range of target temperature range
  }
  
  // Update high/low temp difference
  targetTempHigh = targetTemp + tempDiff;
  targetTempLow = targetTemp - tempDiff;
}

/*----- MOTOR SHIELD FUNCTIONS ---------------
--------------------------------------------*/
void motorCheck(){
  // grab all temperatures from sensors and write to variables
  readTemp();
  
  // check temperature against target and alter motors accordingly
  if (currentTemp < targetTempLow){
    // run peltier as heater
    motorGo(1,CW,220); // peltier 1
    motorGo(0,CW,220); // peltier 2
    // after starting motors, run the following until currentTemp is higher than target Temp
    //while(currentTemp < targetTemp){
    //}
  } 
  else 
if (currentTemp > targetTempHigh){
    // run peltier as cooler
    motorGo(1,CCW,220); // peltier 1
    motorGo(0,CCW,220); // peltier 2
    // after starting motors, run the following until currentTemp is lower than target Temp
    //while(currentTemp > targetTemp){
    //}
  }
  else{
    motorOff(0); // peltier 1
    motorOff(1); // peltier 2
    debug("Peltiers","Off");
    //  after stopping motors, run the following until currentTemp is outside the target temp range (high/low)
    //while(currentTemp < targetTempHigh && currentTemp > targetTempLow){
    //}
  }
}

void motorOff(int motor){
  // initialize braked
  for (int i=0; i<2; i++){
    digitalWrite(inApin[i], LOW);
    digitalWrite(inBpin[i], LOW);
  }
  if (motor == 1){
    peltStatus = 0;
  }
  analogWrite(pwmpin[motor], 0);
}

void motorGo(uint8_t motor, uint8_t direct, uint8_t pwm){
  /* motorGo() will set a motor going in a specific direction the motor will continue going in that direction, at that speed until told to do otherwise.
  - motor: this should be either 0 or 1, will selet which of the two motors to be controlled
  - direct: Should be between 0 and 3, with the following result
  0: Brake to VCC
  1: Clockwise ...or use CW
  2: CounterClockwise ...or use CCW
  3: Brake to GND
  - pwm: should be a value between ? and 1023, higher the number, the faster it'll go
  */
  if (motor <= 1){
    if (direct <= 4){
      // Set inA[motor]
      if (direct <=1){
        digitalWrite(inApin[motor], HIGH);
      }
      else{
        digitalWrite(inApin[motor], LOW);
      }
      // set inB[motor]
      if ((direct == 0)||(direct == 2)){
        digitalWrite(inBpin[motor], HIGH);
      }
      else{
        digitalWrite(inBpin[motor], LOW);
      }
      analogWrite(pwmpin[motor], pwm);
    }
    // set status
    if (motor == 1){
      // set pelt status based on motor direction
      if (direct == CW){
        peltStatus = 2;
      }else if (direct == CCW){
        peltStatus = 1;
      }
    }
  }
}
