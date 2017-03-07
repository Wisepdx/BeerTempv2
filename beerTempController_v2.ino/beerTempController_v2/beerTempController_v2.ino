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

// Motor Controller xxx[0] controls '1' outputs and xxx[1] controls '2' outputs (PELTIER)
int inApin[2] = {7, 4};  // INA: Clockwise input
int inBpin[2] = {8, 9};  // INB: Counter-clockwise input;
int pwmpin[2] = {5, 3};  // PWM input
int cspin[2] = {A2, A3}; // CS: Current sense ANALOG input
int enpin[2] = {A0, A1}; // EN: Status of switches output (Analog pin)

/*-------------- Datapoints ----------------
--------------------------------------------*/
int batchId = 0;  // beer ID (always make 3 digit)
int batchIdOld = 0;  // beer ID (always make 3 digit)
String batchName = "unknown"; // beer name
int batchSize = 10;  // beer batch size - Deafult 10 Gallons
float targetTemp = 68;  // target temp of beer (In Fahrenheit) - Default 68
//int pumpStatus = 0; // pump Status (0 = Off, 1 = On)
int peltStatus = 0; // peltier status (0 = Off, 1 = Cool, 2 = Heat)
float currentTemp;  // current temperature of beer (In Fahrenheit)
float ambientTemp;  // ambient temperature of room (In Fahrenheit)
float tempDiff = 0.5; // range at which temperature can drift from target
int targetTempHigh = targetTemp + tempDiff; // high end of temp range
int targetTempLow = targetTemp - tempDiff;  // low end of temp range
int postType = 0; // 1 = BatchData, 2 = Sensor Data
String data = ""; // holds data to POST
String dataTemp; // temp hold area for int and floats when printing into data
String divider = "----------"; // debug output divider
int messageCount = 0; //message count

//IP Address of the sever on which there is the WS: http://www.mywebsite.com/
IPAddress server(10,0,1,114); // internal ip address

//YunClient client;
BridgeClient client;


/*----------------------------
  SETUP LOOP START HERE
------------------------------*/

void setup() {

  Bridge.begin();
  Mailbox.begin();
  Console.begin();
  Serial.begin(115200);

  // 12v Fans for Peltiers
  pinMode(13, OUTPUT);

  // start Temperature Sensors
  TempSensor.begin();
  // motor Shield (Set to Off)
  motorOff(0);
  motorOff(1);

  //Delay before starting session
  delay(10000);
  debug("Starting","Session");

  //Check connection to server
  if (client.connect(server, 80)) {
    debugPost("Sweet! Connected");
    delay(2500);
  } else {
    debugPost("Ah Snap! Connection Failed");
  }

  // Turn Fans Off
  digitalWrite(13,LOW);
  delay(2000);
  // Turn Fans On
  digitalWrite(13,HIGH);
  delay(2000);
  // Turn Fans Off
  digitalWrite(13,LOW);
}

/*---------------------------
  VOID LOOP START HERE
-----------------------------*/

void loop(){
  debugPost("Start Loop");

  // testing client connect
  if (client.connect(server,80)) {
      debugPost("Works!!!");
    } else{
        debugPost("Nope");
      }


  //Check Mailbox for new data
  mailboxCheck();

  // read temperature
  readTemp();

  // set postType to 0 if Batch ID = 0 to postpone data collection and wait for more instructions;
  if (batchId == 0){
    postType = 0;
    debug("Stopping logging until new batch entered", "N");
  }

  // check post type and build post data
  if (postType != 0 ){
    //write out post type
    debug(String(postType), "PostType");

    //POST TYPE 1 - Batch Data Post
    if (postType == 1){
      //compile data var from batch vars
      dataWriteBatch();
      //POST Data
      postData();
      // set post type to 2 to start collecting sensor data
      postType = 2;

      // wait 1 minute and then loop
      debugPost("waiting for 5 minutes to check sensors again...");
      off();
      delay(300000);
    }

    //POST TYPE 2 - Sensor Data Post
    else if (postType == 2){
      // check temperatures against optimum settings and turn peltier on or off

      // grab all temperatures from sensors and write to variables
      readTemp();

      // heat
      if (currentTemp < targetTempLow){
        do {
          // grab all temperatures from sensors and write to variables
          readTemp();
          //Heat until current temp is hit then let residual heat take over (turn off peltiers)
          heat();
          //compile data var from sensor data
          dataWriteSensors();
          //POST Data
          postData();
          // wait 5 minutes and then loop
          debugPost("waiting for 5 minutes to check sensors again...");
          delay(300000);
          // check messages
          mailboxCheck();
        }
        while((currentTemp < targetTemp) && (messageCount == 0));
      }
      // cool
      else if (currentTemp > targetTempHigh){
        do {
          // grab all temperatures from sensors and write to variables
          readTemp();
          cool();
          //compile data var from sensor data
          dataWriteSensors();
          //POST Data
          postData();
          // wait 5 minutes and then loop
          debugPost("waiting for 5 minutes to check sensors again...");
          delay(300000);
          // check messages
          mailboxCheck();
        }
        while((currentTemp > targetTemp) && (messageCount == 0));
      }
      else{
        // grab all temperatures from sensors and write to variables
        readTemp();
        off();
      }
    }
  } else if (postType == 0){

    debugPost("no post type set, waiting for input");

    // wait for mailbox request
    mailboxCheck();
    delay(10000);  // wait 10 seconds and check again
  }

  delay(10000); //wait 10 seconds
  debug("End Loop","N");
}

/*
------------------------------------
FUNCTIONS START BELOW HERE
------------------------------------
*/

/*----- DEBUG POSTIN' ------------------------
--------------------------------------------*/
//debug function: outputs to Console (web) and Serial (local usb)

void debug(String value, String valueHeader){
  if (valueHeader == "N" ){
    // output to console and serial
    Console.println(value);
    Serial.println(value);
  } else {
    // output to console and serial
    Console.println(valueHeader + ": " + value);
    Serial.println(valueHeader + ": " + value);
  }
  // output to console and serial
  Console.println(divider);
  Serial.println(divider);
}

void debugPost(String value){
    // output to console and serial
    Console.println(value);
    Serial.println(value);
}

/*----- DATA WRITIN' -------------------------
--------------------------------------------*/

void dataWriteBatch(){
  //clear data
  data = "";
  // input post Type
  data += "postType=1";
  // input batch ID
  dataTemp = String(batchId);
  data += "&batchId=" + dataTemp;
  // input batch name
  data += "&batchName=" + batchName;
  // input batch size
  dataTemp = batchSize;
  data += "&batchSize=" + dataTemp;
  // write debug of data
  debug(data, "Full Batch Data String");
}

void dataWriteSensors(){
  //clear sensor data
  data = "";
  // input post Type
  data += "postType=2";
  // input batch ID
  dataTemp = String(batchId);
  data += "&batchId=" + dataTemp;
  // input targetTemp
  dataTemp = String(targetTemp);
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
  debug(data, "Full Sensor Data String");
}

void postData(){
  debugPost("entering post data section...");
  if (client.connect(server,80)) { // REPLACE WITH YOUR SERVER ADDRESS
    //HTTP POST
    client.println("POST /add.php HTTP/1.1");
    client.println("Host: beerdev.wisepdx.net"); // SERVER ADDRESS HERE TOO
    client.println("Accept: */*");
    client.println("Content-Length: " + String(data.length()));
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.println();
    client.println(data);

    //debug write
    debugPost("Sending POST to add.php @ beerdev.wisepdx.net");
    debug(String(data.length()),"Length");
    debug(data, "Data Sent");
  } else{
    debugPost( "Posting failed" );
    }

  if (client.connected()) {
    client.stop();  // DISCONNECT FROM THE SERVER
  }
}

/*----- TEMPERATURE FINDIN' ------------------
--------------------------------------------*/
void readTemp(){
  TempSensor.requestTemperatures();
  ambientTemp = TempSensor.getTempFByIndex(0);  // Black Small Sensor
  currentTemp = TempSensor.getTempFByIndex(1);  // (Metal Probe Sensor)

  // write debug of Temp
  debug(String(ambientTemp,3),"Ambient Temp");
  debug(String(currentTemp,3), "Current Temp");
}

/*----- MAILBOX MESSAGE PARSIN' --------------
--------------------------------------------*/
void mailboxCheck(){
  String message;
  debugPost("Checking Mailbox...");
  int messageCount = 0; // message increment var
  // if there is a message in the Mailbox
  if (Mailbox.messageAvailable()){
    // read all the messages present in the queue
    while (Mailbox.messageAvailable()){
      Mailbox.readMessage(message);
      //write out message
      debug(message, "Message");
      String variableName = "";
      String variableValue = "";
      bool readingName = false;
      messageCount++; //increment variable l
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
      //record variable
      recordVariablesFromWeb(variableName, variableValue);
    }
    debug(String(messageCount,2),"Number of Messages");
  }
}

void recordVariablesFromWeb(String variableName, String variableValue){
  // set values to their particular variables in this section
  // parse Variables to the Proper Variable

  //write out variables
  debug(variableValue,variableName);

  if(variableName == "batchid"){
    batchId = variableValue.toInt();
  }
  else if(variableName == "posttype"){
    postType = variableValue.toInt();
  }
  else if(variableName == "batchname"){
    batchName = variableValue;
  }
  else if(variableName == "batchsize"){
    batchSize = variableValue.toInt();
  }
  else if(variableName == "tempdiff"){
    tempDiff = variableValue.toFloat();
  }
  else if(variableName == "targettemp"){
    targetTemp = variableValue.toFloat();
    // set high/low range of target temperature range
  }

  // Update high/low temp difference
  targetTempHigh = targetTemp + tempDiff;
  targetTempLow = targetTemp - tempDiff;
}

/*----- MOTOR SHIELD FUNCTIONS ---------------
--------------------------------------------*/
void heat(){
  // run peltier as heater

  // if temp is higher than bar then set to this pwm level else
  if (currentTemp < targetTemp){
    motorGo(0,CW,160); // peltier 1
    motorGo(1,CW,160); // peltier 2
    digitalWrite(13, HIGH); // turn fans on
    peltStatus = 2;
    debug("Heating", "Peltier Status");
  } else{
    motorOff(0); // peltier 1
    motorOff(1); // peltier 2
    digitalWrite(13, LOW); // turn fans Off
    peltStatus = 0;
    debug("Cool Down", "Peltier Status");
  }
}

void cool(){
  // run peltier as cooler
  motorGo(0,CCW,255); // peltier 1
  motorGo(1,CCW,255); // peltier 2
  digitalWrite(13, HIGH); // turn fans on
  peltStatus = 1;
  debug("Cooling", "Peltier Status");
}

void off(){
  motorOff(0); // peltier 1
  motorOff(1); // peltier 2
  digitalWrite(13, LOW); // turn fans off
  peltStatus = 0;
  debug("Off", "Peltier Status");
}

void motorOff(int motor){
  // initialize braked
  for (int i=0; i<2; i++){
    digitalWrite(inApin[i], LOW);
    digitalWrite(inBpin[i], LOW);
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
  }
}
