
//#include <dht.h>
//#include <RF24Network.h>
//#include <RF24.h>
//#include <SPI.h>
#include <Sync.h>
#include <RF24Network_config.h>
#include <RF24Network.h>
#include <RF24_config.h>
#include <RF24.h>
#include <printf.h>
#include <nRF24L01.h>
#include <SPI.h>
#include <C:\Users\Ricardo\Documents\Arduino\libraries\DHTLib\dht.h>


constexpr auto DHT11PIN = 9;
dht DHT1;

RF24 radio(7, 8);               // nRF24L01 (CE,CSN)
RF24Network network(radio);      // Include the radio in the network
const uint16_t this_node = 04;   // Address of this node in Octal format ( 04,031, etc)
const uint16_t baseNode = 00;

RF24NetworkHeader headerReceive;

int motionPin = 3;
int pinContactor = 4;
float temperatureReading;
float humidityReading;
bool motionDetected;
unsigned long lastUpdate;
typedef struct {
    unsigned long instruction1;
    unsigned long instruction2;
} instructions;

//Instruction:  0 - turn off the pump, 1 - turn on the pump, 2 - automatic timer for pump (shuts off and 5 minute restart)

typedef struct {
    uint16_t node_Address;
    long isOnline;
    long isON;
    float temperature;
    float humidity;
    long motion;
    long timerLeft;
    float current;
    unsigned long lastHeartBeat;
    float F1;
    float F2;
    float F3;
    float F4;
    float F5;
    long L1;
    long L2;
    long L3;
    long L4;
    long L5;
} contactorStationInformation;

contactorStationInformation instructionsToSend;
instructions incomingData;
unsigned long timerStart;
//************************************************************************************************************************
void setup() {
    pinMode(DHT11PIN, INPUT);
    pinMode(motionPin, INPUT);
    pinMode(pinContactor, OUTPUT);
    digitalWrite(pinContactor, HIGH);
    instructionsToSend.isON = 1;
    incomingData.instruction1 = 1;
    incomingData.instruction2 = 0;
    Serial.begin(115200);
    SPI.begin();
    radio.begin();
    radio.setPALevel(RF24_PA_MAX);
    network.begin(90, this_node);  //(channel, node address)
    Serial.println("Starting...");
    lastUpdate = millis();
}
//************************************************************************************************************************
void loop() {
    
    // To account for millis() rollover.
    if (lastUpdate > millis()) {
        lastUpdate = 0;
    }
    //Sends an update to base every 1 minute
    if (millis() - lastUpdate > 60000) {
        SendUpdate();
    }

    //if the pump is off and we need to restart it with a timer.
    if (instructionsToSend.isON == 2) {
        instructionsToSend.timerLeft = 600000 - (millis() - timerStart);
        //when pump is down we send an update to base every 10 seconds.
        if (millis() - lastUpdate > 10000) {
            SendUpdate();
        }
        //when timer expires we re-start pump
        if (instructionsToSend.timerLeft <= 0) {
            instructionsToSend.isON = 1;
            digitalWrite(pinContactor, HIGH);
            instructionsToSend.timerLeft = 0;
        }
    }

    //NOw we check for messages in the RF24 network
    network.update();
    while (network.available()) {     // Is there any incoming data?
        network.read(headerReceive, &incomingData, sizeof(incomingData)); // Read the incoming data
        Serial.print(F("Instruction 1: ")); Serial.println(incomingData.instruction1);
        Serial.print(F("Instruction 2: ")); Serial.println(incomingData.instruction2);
        //zero is turn off the pump
        if (incomingData.instruction1 == 0) {
            digitalWrite(pinContactor, LOW);
            instructionsToSend.isON = 0;
            instructionsToSend.timerLeft = 0;
        }
        // one it turn on th pump
        else if (incomingData.instruction1 == 1) {
            digitalWrite(pinContactor, HIGH);
            instructionsToSend.isON = 1;
            instructionsToSend.timerLeft = 0;
        }
        // two is shutdown the pump and re-start on timer
        else if (incomingData.instruction1 == 2) {
            digitalWrite(pinContactor, LOW);
            instructionsToSend.isON = 2;
            instructionsToSend.timerLeft = 600000;
            timerStart = millis();
        }
        // four is the base requesting an update
        else if (incomingData.instruction1 == 4) {   
                SendUpdate();
            Serial.println(F("Update requested from base..."));
        }
    }
}
//************************************************************************************************************************
void SendUpdate() {
    // we make sure we don't send updates too frequently, 2 seconds between updates.
    if (millis() - lastUpdate < 2000) {
        return;
    }
    DHT1.read11(DHT11PIN);
    temperatureReading = (DHT1.temperature * 9 / 5) + 32;
    if (temperatureReading > 120 || temperatureReading < 0) {
        temperatureReading = 0;
    }
    humidityReading = DHT1.humidity;
    motionDetected = digitalRead(motionPin);
    Serial.print("Pump is: "); Serial.println(instructionsToSend.isON);
    Serial.print("Humidity (%): "); Serial.println(humidityReading);
    Serial.print("Temperature(C): "); Serial.println(temperatureReading);
    Serial.print("Motion Sensor: "); Serial.println(motionDetected);
    Serial.print("Timer Left: "); Serial.println(instructionsToSend.timerLeft);
    instructionsToSend.isOnline = true;
    instructionsToSend.temperature = temperatureReading;
    instructionsToSend.humidity = humidityReading;
    instructionsToSend.motion = motionDetected;
    
    RF24NetworkHeader headerTransmit(baseNode); // (Address where the data is going)
    bool ok = network.write(headerTransmit, &instructionsToSend, sizeof(instructionsToSend)); // Send the data
    if (ok == true) {
        Serial.print("Succesful Transmission to NODE: "); Serial.println(baseNode);       
    }
    else {
        Serial.println("Unable to transmit.");
    }
    Serial.print("Size of data: "); Serial.println(sizeof(instructionsToSend));
    lastUpdate = millis();
    delay(100);
}
