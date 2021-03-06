/*
 *  Command Node Code
 *
 *  Before using, check:
 *  NUM_NODES, MY_NAME (should always be 0)
 *  Anchor node locations, x1, y1, x2, y2, x3, y3
 *
 */

#include <SPI.h>
#include "cc2500_REG_V2.h"
#include "cc2500_VAL_V2.h"
#include "cc2500init_V2.h"
#include "read_write.h"

//*****************************************Variables********************
//Number of nodes, including Command Node
const byte NUM_NODES = 4;
byte RST[PACKET_LENGTH];

//The LED PIN
int ledPin = 9;

//Names of Node and nodes before it
//Determines when this node's turn is
const byte MY_NAME = 0; //Command node is always 0
const byte PREV_NODE = NUM_NODES - 1;
const byte PREV_PREV_NODE = NUM_NODES - 2;

//State names
const int INIT = 0;
const int RECEIVE = 1;
const int CALCULATE = 2;
int state = INIT;

//Initial conditions of three points, first is command node at origin
//stub convert to (-127)-128 before sending to serial
const int x1 = 0;  //128;  //0
const int y1 = 0;  //128;  //0
const int x2 = 12;  //129;  //1
const int y2 = 36;  //131;  //3
const int x3 = 36;  //127;  //turns into -1
const int y3 = -12;  //124;  //turns into -4

//How many data entries to take an average from, arbitrarily set to 15 stub
const int STRUCT_LENGTH = 15;

//The indexes of where each piece of data is (for readability of code)
const int SENDER = 0;
const int TARGET = 1;
const int DISTANCE = 2;
const int SENSOR_DATA = 3;
const int HOP = 4;
const int END_BYTE = 5;
const int RSSI_INDEX = 6;

//This is how many times to resend all data, for redundancy.  Arbitrarily set to 4
const int REDUNDANCY = 4;

//Timer info
const unsigned long TIMEOUT_PP = 2000; //??? check this timeout number stub
const unsigned long TIMEOUT_P = 1000;

//Global variables
//These control timeouts
unsigned long currTime;
unsigned long lastTime;

//These are the structures that contain the data to be averaged
byte rssiData[NUM_NODES][STRUCT_LENGTH] = {
  0
};

//Each contains a pointer for each of the nodes, indicating where to write in the above tables
int rssiPtr[NUM_NODES] = {
  0
};

//Arrays of averages
byte rssiAvg[NUM_NODES] = {
  0
};

//The main matrices
byte distances[NUM_NODES][NUM_NODES] = {
  0
};
byte allSensorData[NUM_NODES];    //Collected sensor data from all nodes
byte currLoc[NUM_NODES][2] = {
  0
}; //The contents of the R calculations file will go here
byte desired[NUM_NODES][2] = {
  0
};

//Flags for controlling getting new data every cycle or not
boolean wantNewMsg = true;
boolean gotNewMsg = false;

//The current message, and storage for data restoration in case of bad packet
byte currMsg[PACKET_LENGTH] = {
  0
};
byte oldMsg[PACKET_LENGTH] = {
  0
};

//Current Round number
int roundNumber = 0;

//Temporary variable used for averaging
unsigned int temp = 0;
int goodMsg = 0;

//Helps coordinate timeout
byte lastHeardFrom;



//************************************* Functions**************************************

//Round values up to nearest whole number, cast to byte
byte roundUp(float input) {
  float output = input + 0.9999;
  return byte(output);
}

//This function takes in input from serial and parsee + store it
void userCom() {
  //Variables
  char userCommand[500];
  char snippedCommand[500];
  char *tempCommand1, *tempCommand2;
  char *arg1, *arg2, *arg3, *arg4;
  boolean data_command_flag = true;
  int sequenceNumber;
  int endSequenceNumber;
  int nodeNum;

  //when there is data from serial/Computer
  if (Serial.available()) {

    //Variables for tokenizing
    arg1 = NULL;
    arg2 = NULL;  //use for later improve arguments
    for (int x = 0; x < 500; x++) {
      userCommand[x] = NULL;
      snippedCommand[x] = NULL;
    }
    int x = 0;

    //delay a bit to ensure full data transfer
    delay(200);

    //obtain all data into array
    do {
      userCommand[x++] = Serial.read();
    }
    while (Serial.available());

    //variable for snipping actual data packet from noise
    int mostRecent3 = 0;
    int secondMostRecent3 = 0;
    int mostRecent4 = 0;

    //finding location of data packet
    for (int i = 0; userCommand[i + 1] != NULL; i++) {
      if (userCommand[i] == '-' && userCommand[i + 1] == '3') {
        secondMostRecent3 = mostRecent3;
        mostRecent3 = i;

      }
      if (userCommand[i] == '-' && userCommand[i + 1] == '4') mostRecent4 = i;
    }

    //you have a partial message, so you need the previous full message
    if (mostRecent3 > mostRecent4) {
      mostRecent3 = secondMostRecent3;
    }
    
    //snipping data packet
    int countX = 0;
    for (int i = mostRecent3; i < mostRecent4; i++) {
      snippedCommand[countX++] = userCommand[i];
    }
    
    //tokenize first line
    arg1 = strtok_r(snippedCommand, "\n", &tempCommand1);

    //when there is data keep looping
    int lineCounter = 0;
    while (arg1 != NULL) {

      // token again for single words or variables
      arg2 = strtok_r(arg1, " ", &tempCommand2);

      //checking for type of data
      if (!strcmp(arg2, "-1")) {
        //handle next/all
        //This is where you'd parse in different values for new nodes
        //or for new anchor node locations, but we aren't having mobile anchor nodes
        //nor live additions of nodes in the network at this point

      }
      else if (!strcmp(arg2, "-3")) {

        //start of packet, future work if needed
          
        //handle next/all
        sequenceNumber = atoi(strtok_r(NULL, " ", &tempCommand2));

      }
      else if (!strcmp(arg2, "-4")) {
        
        //end of packet, future work location if needed
        
        //handle next/all
        endSequenceNumber = atoi(strtok_r(NULL, " ", &tempCommand2));

      }
      else if ((nodeNum = atoi(arg2)) >= 0) {
        //data for each node number

        //obtain data
        arg3 = strtok_r(NULL, " ", &tempCommand2);
        arg4 = strtok_r(NULL, " ", &tempCommand2);

        //store data for tranmission wirelessly
        currLoc[nodeNum][0] = byte(atoi(arg3));
        currLoc[nodeNum][1] = byte(atoi(arg4));

      }
      else if (!strcmp(arg2, "-5")) {
        
        //user command data

        //obtain the command
        arg3 = strtok_r(NULL, " ", &tempCommand2);
        arg4 = strtok_r(NULL, " ", &tempCommand2);

        String argString = String(arg4);
        int argName = argString.toInt();

        //checks for what command and send out command to broadcast or certain node
        if (!strcmp(arg3, "shutdown") && ((argName < NUM_NODES) || (argName == 255))) {
          digitalWrite(ledPin, HIGH);
          for (int i = 0; i > 1000; i++) {
            delay(10);
            sendPacket(0, argName, 0, 0, 203, 0); //255 is for broadcast2 and 203 is for shudown
          }
          sendPacket(0, argName, 0, 0, 203, 1);
          digitalWrite(ledPin, LOW);
        }
        else if (!strcmp(arg3, "land") && ((argName < NUM_NODES) || (argName == 255))) {
          digitalWrite(ledPin, HIGH);
          for (int i = 0; i > 1000; i++) {
            delay(10);
            sendPacket(0, argName, 0, 0, 204, 0); //255 is for broadcast2 and 204 is for land
          }
          sendPacket(0, argName, 0, 0, 204, 1);
          digitalWrite(ledPin, LOW);
        }
        else if (!strcmp(arg3, "flight") && ((argName < NUM_NODES) || (argName == 255))) {
          digitalWrite(ledPin, HIGH);
          for (int i = 0; i > 1000; i++) {
            delay(10);
            sendPacket(0, argName, 0, 0, 205, 0); //255 is for broadcast2 and 205 is for flight
          }
          sendPacket(0, argName, 0, 0, 205, 1);
          digitalWrite(ledPin, LOW);
        }
      }
      else {
        //Serial.print("read in error");
      }

      //get next line
      arg1 = strtok_r(NULL, "\n", &tempCommand1);

      lineCounter++;
    }
  }
}

//Converts values from 0-255 to (-)127-128
int byteToInt(byte input) {
  int returnNumber;
  returnNumber = int(input) - 127;

  return returnNumber;
}


//************************************* Begin of Arduino Main Setup **************************************
void setup() {
  Serial.begin(9600);
  init_CC2500_V2();

  //For debug to see if register are written
  //comment out before starting all programs in data flow
  
  //Serial.println("After SPI Init");

  //Serial.println(ReadReg(REG_IOCFG0),HEX);
  //Serial.println(ReadReg(REG_IOCFG1),HEX);
  //Serial.println(ReadReg(REG_IOCFG2),HEX);

  pinMode(ledPin, OUTPUT);
  
  //if there is a round robin going, wait for turn, incase of restart/reset
  while (listenForPacket(RST)) {
  }

  //send out reset packet for global reset
  digitalWrite(ledPin, HIGH);
  for (int i = 0; i < 5; i++) {
    sendPacket(0, 255, 0, 0, 200, 0);
  }
  
  //LED on off indication of startup
  delay(500);
  digitalWrite(ledPin, LOW);
  delay(500);

  //This just hardcodes some values into the Desired table, as bytes
  randomSeed(analogRead(3));
  for (int i = 0; i < NUM_NODES; i++) {
    desired[i][0] = roundUp(random(30, 230)); //roundUp((rand() % 200) + 30);  //gets a range of randoms from -100ish to 100ish
    desired[i][1] = roundUp(random(30, 230)); //roundUp((rand() % 200) + 30);
  }
  desired[0][0] = byte(128);
  desired[0][0] = byte(128);

}

void loop() {
  
  
  //******************************************* Check for data***************************************
  
  //This block picks up a new message if the state machine requires one this
  //cycle.  It also accommodates packets not arriving yet
  //It also sets gotNewMsg, which controls data collection later
  if (wantNewMsg) {
    //Save old values in case can't pick up a new packet
    for (int i = 0; i < PACKET_LENGTH; i++) {
      oldMsg[i] = currMsg[i];
    }

    //Get new values. If no packet available, goodMsg will be null
    goodMsg = listenForPacket(currMsg);

    //Check to see if packet is null. If so, put old values back
    if (goodMsg == 0) {
      for (int i = 0; i < PACKET_LENGTH; i++) {
        currMsg[i] = oldMsg[i];
      }
      gotNewMsg = false;
    }
    else {
      //..otherwise, the packet is good, and you got a new message successfully
      lastHeardFrom = currMsg[SENDER];
      gotNewMsg = true;
    }
  }



  //*******************************************State Machine***************************************

  //State machine controlling what to do with packet
  switch (state) {
    case INIT:
      //Serial.println("Init state");
      //Send Startup message, with SENDER == 0, and TARGET == 0;  Sending 10 times arbitrarily
      digitalWrite(ledPin, HIGH);
      for (int i = 0; i < 10; i++) {
        sendPacket(0, 0, 0, 0, 0, 0);
      }
      sendPacket(0, 0, 0, 0, 0, 1);
      sendPacket(0, 0, 0, 0, 0, 1);
      sendPacket(0, 0, 0, 0, 0, 1);

      state = RECEIVE;
      wantNewMsg = true;
      break;

    case RECEIVE:
      //Serial.println("Receive state");
      digitalWrite(ledPin, LOW);

      //If hear from Prev_Prev, and it's a valid message, "start" timeout timer
      if ((currMsg[SENDER] == PREV_PREV_NODE || currMsg[SENDER] == PREV_NODE) && gotNewMsg) {
        lastTime = millis();
      }

      //stub, this allows the case where prev_prev fails, and the node hasn't heard from prev_prev
      //so as soon as prev node says anything, the timer is checked and is of course active,
      //so we need another timer to see whether you've last heard anything from prev_prev, and a timer
      //for whether you've last heard anything from prev
      if (lastHeardFrom == PREV_NODE || lastHeardFrom == PREV_PREV_NODE) {
        currTime = millis() - lastTime;
      }

      //Prev node has finish sending
      if (currMsg[SENDER] == PREV_NODE && currMsg[END_BYTE] == byte(1) && lastHeardFrom == PREV_NODE) {
        state = CALCULATE;
        wantNewMsg = false;
      } else if (((lastHeardFrom == PREV_PREV_NODE) && (currTime > TIMEOUT_PP)) || ((lastHeardFrom == PREV_NODE) && (currTime > TIMEOUT_P))) {
        state = CALCULATE;
        lastTime = millis();
        currTime = 0;
        wantNewMsg = false;
      }

      //Add data from packet to data structure
      distances[currMsg[SENDER]][currMsg[TARGET]] = currMsg[DISTANCE];
      allSensorData[currMsg[SENDER]] = currMsg[SENSOR_DATA];

      break;

    case CALCULATE:
    
      digitalWrite(ledPin, HIGH);

      //Assert "distance to self"s to 0 then
      //average upper and lower triangles of data
      for (int i = 0; i < NUM_NODES; i++) {
        distances[i][i] = 0;
        int h = i;
        for (h = h + 1; h < NUM_NODES; h++) {
          distances[i][h] = roundUp((distances[i][h] + distances[h][i]) / 2);
          distances[h][i] = distances[i][h];
        }
      }
      //Transmit data through serial
      roundNumber = roundNumber + 1;
      
      //Hard-coded formatting that R knows to accept, starting with round number and number of nodes
      Serial.print("-3 ");
      Serial.println(roundNumber);
      Serial.print("-1 0 ");
      Serial.println(NUM_NODES);
      //Send anchor node values
      Serial.print("-1 1 ");
      Serial.println(x1);
      Serial.print("-1 2 ");
      Serial.println(y1);
      Serial.print("-1 3 ");
      Serial.println(x2);
      Serial.print("-1 4 ");
      Serial.println(y2);
      Serial.print("-1 5 ");
      Serial.println(x3);
      Serial.print("-1 6 ");
      Serial.println(y3);
      
      //Send current distance values
      for (int i = 0; i < NUM_NODES; i++) {
        for (int j = 0; j < NUM_NODES; j++) {
          Serial.print(i);
          Serial.print(" ");
          Serial.print(j);
          Serial.print(" ");
          Serial.println(distances[i][j]);
        }
      }
      
      //Send desired values
      for (int i = 0; i < NUM_NODES; i++) {
        Serial.print("-2 ");
        Serial.print(i);
        Serial.print(" 0 ");
        Serial.println(desired[i][0]);

        Serial.print("-2 ");
        Serial.print(i);
        Serial.print(" 1 ");
        Serial.println(byteToInt(desired[i][1]));
      }
      //Send round number again, as "end of serial transmission" indicator
      Serial.print("-4 ");
      Serial.println(roundNumber);

      //Stub, read from Serial to detect results from R, then read in until
      //receive "-4 [roundNumber]" back (signifies end of transmission)
      
      //see if there is data avaible from computer
      userCom();

      //Send current locations and commands a few times
      for (int j = 0; j < REDUNDANCY; j++) {
        for (int i = 0; i < NUM_NODES; i++) {
          sendPacket(MY_NAME, i, currLoc[i][0], currLoc[i][1], 201, 0);
          sendPacket(MY_NAME, i, desired[i][0], desired[i][1], 202, 0); //!!!!!!!!!!!!!!!! which one identifies the type of
        }
      }
      sendPacket(MY_NAME, NUM_NODES, desired[NUM_NODES][0], desired[NUM_NODES][1], 1, 1);

      lastHeardFrom = MY_NAME;

      //Turn is over, begin receiving again

      digitalWrite(ledPin, LOW);
      wantNewMsg = true;
      state = RECEIVE;
      break;
  }

  //Every received packet must have RSSI scraped off and added to calculations
  if (gotNewMsg) {
    //Check if there is already an average, if so, do filter, if not just add data in appropriate position
    //(in both cases pointer must be incremented or looped
    if (rssiAvg[currMsg[SENDER]] != 0) {
      //Filter RSSI based on +-10 around running average
      //if(currMsg[RSSI_INDEX] < rssiAvg[currMsg[SENDER]] + 10 && currMsg[RSSI_INDEX] > rssiAvg[currMsg[SENDER]] - 10){
      rssiData[currMsg[SENDER]][rssiPtr[currMsg[SENDER]]] = currMsg[RSSI_INDEX];
      if (rssiPtr[currMsg[SENDER]] == STRUCT_LENGTH - 1) rssiPtr[currMsg[SENDER]] = 0; //Loop pointer around if it's reached the end of the array
      else rssiPtr[currMsg[SENDER]] += 1;                              //..otherwise just increment
    }
    else {
      rssiData[currMsg[SENDER]][rssiPtr[currMsg[SENDER]]] = currMsg[RSSI_INDEX];
      if (rssiPtr[currMsg[SENDER]] == STRUCT_LENGTH - 1) rssiPtr[currMsg[SENDER]] = 0;
      else rssiPtr[currMsg[SENDER]] += 1;
    }


    //If there's a full row of data to average, average it
    //(detects full row by checking last bit in row filled)
    if (rssiData[currMsg[SENDER]][STRUCT_LENGTH - 1] != 0) {
      temp = 0;
      for (int i = 0; i < STRUCT_LENGTH; i++) {
        temp += rssiData[currMsg[SENDER]][i];
      }
      rssiAvg[currMsg[SENDER]] = temp / STRUCT_LENGTH;
    }

    //Add new average to distance table
    distances[MY_NAME][currMsg[SENDER]] = roundUp(log(float(rssiAvg[currMsg[SENDER]]) / 95) / log(0.99));

  }
}

