
//#include <dht.h>
#include <RF24Network.h>
#include <RF24.h>
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
    long isOnline;
    long isON;
    float temperature;
    float humidity;
    long motion;
    long timerLeft;
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
    network.update();
    if (millis() - lastUpdate > 60000) {
        SendUpdate();
    }

    if (instructionsToSend.isON == 2) {
        instructionsToSend.timerLeft = 300000 - (millis() - timerStart);
        Serial.println(instructionsToSend.timerLeft);
        if (instructionsToSend.timerLeft <= 0) {
            instructionsToSend.isON = 1;
            digitalWrite(pinContactor, HIGH);
            instructionsToSend.timerLeft = 0;
        }
    }

    while (network.available()) {     // Is there any incoming data?
        network.read(headerReceive, &incomingData, sizeof(incomingData)); // Read the incoming data
        Serial.print(F("Instruction 1: ")); Serial.println(incomingData.instruction1);
        Serial.print(F("Instruction 2: ")); Serial.println(incomingData.instruction2);
        if (incomingData.instruction1 == 0) {
            digitalWrite(pinContactor, LOW);
            instructionsToSend.isON = 0;
            instructionsToSend.timerLeft = 0;
        }
        else if (incomingData.instruction1 == 1) {
            digitalWrite(pinContactor, HIGH);
            instructionsToSend.isON = 1;
            instructionsToSend.timerLeft = 0;
        }
        else if (incomingData.instruction1 == 2) {
            digitalWrite(pinContactor, LOW);
            instructionsToSend.isON = 2;
            instructionsToSend.timerLeft = 300000;
            timerStart = millis();
        }
        else if (incomingData.instruction1 == 4) {
            SendUpdate();
        }
    }
}
//************************************************************************************************************************
void SendUpdate() {
    DHT1.read11(DHT11PIN);
    temperatureReading = (DHT1.temperature * 9 / 5) + 32;
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
        lastUpdate = millis();
    }
    else {
        Serial.println("Unable to transmit.");
    }
    delay(50);
}
