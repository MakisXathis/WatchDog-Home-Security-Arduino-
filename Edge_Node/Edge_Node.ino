#include <Arduino.h>
#include <SPI.h>
#include <WiFiNINA.h>
#include "arduino_secrets.h"
#include "Seeed_BMP280.h"
#include "Arduino_SensorKit.h"
#include <Servo.h>
#include <string.h>
#define digital_pir_sensor 5


///////please enter your sensitive data in the Secret tab/arduino_secrets.h
const char ssid[] = SECRET_SSID;        // your network SSID (name)
const char pass[] = SECRET_PASS;        // your network password (use for WPA)

BMP280 pressure_sensor;
int prev_pressure = 0;

Servo mylock;
int servo_position = 0;

double accelerometer_x_prev,accelerometer_y_prev,accelerometer_z_prev = 10;

IPAddress ip(192,168,161,195);
WiFiServer server(8080);
WiFiClient send_client;

char server_ip[] = "192.168.161.194";

char edge_location = "kitchen";

unsigned long previous_millis = 0;

void(* resetFunc) (void) = 0; //Reset entire Edge Node programmatically because of Servo bugs with Arduino Uno WiFi Rev2

void sendToMaster(char message[]){                  //Sends notification to Master Node
  if(send_client.connect(server_ip, 80)){
    Serial.println("connected to server");
    send_client.println(message);
    send_client.println();
    previous_millis = millis();
    Serial.println("Message Sent");
  }

  if(send_client.connected()){
    send_client.stop();
  }
}

void sendHeartbit(){                                //Sends a heartbit to Master Node
  Serial.println("Attempting to heartbit");
  sendToMaster("GET /kitchen/heartbit HTTP/1.1");
}

void connectToWifi(char ssid[],char pass[]){                //Attempts WiFi connection
  int status = WL_IDLE_STATUS;
  WiFi.config(ip);
  while (status != WL_CONNECTED){
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    digitalWrite(LED_BUILTIN, LOW);
    status = WiFi.begin(ssid, pass);
    Serial.print("Status: ");
    Serial.println(status);
    delay(1000);
    digitalWrite(LED_BUILTIN, HIGH);
  }
  _printWiFiStatus();
}

void _printWiFiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

//Reads accelerometer values if movement is detected sends notification to Master Node
//and stores the current accelerometer values

void doorHandleHandler(){
  double accelerometer_x_curr = Accelerometer.readX();    //Read the accelerometer values
  double accelerometer_y_curr = Accelerometer.readY();
  double accelerometer_z_curr = Accelerometer.readZ(); 

  if (doorHandleMovement(accelerometer_x_curr,accelerometer_y_curr,accelerometer_z_curr)){ //Check if there is movement in the handle
    sendToMaster("GET /kitchen/accelerometer HTTP/1.1");
  }
  accelerometer_x_prev=accelerometer_x_curr;
  accelerometer_y_prev=accelerometer_y_curr;
  accelerometer_z_prev=accelerometer_z_curr;
}

//Checks if movement is detected on the door accelerometer
//millis are checked as well because sensor fires initially

bool doorHandleMovement(double accelerometer_x_curr, double accelerometer_y_curr, double accelerometer_z_curr){
  if (((fabs(accelerometer_x_prev-accelerometer_x_curr)>=0.12 && fabs(accelerometer_y_prev-accelerometer_y_curr)>=0.12) || (fabs(accelerometer_x_prev-accelerometer_x_curr)>=0.12 && fabs(accelerometer_z_prev-accelerometer_z_curr)>=0.12) || (fabs(accelerometer_z_prev-accelerometer_z_curr)>=0.12 && fabs(accelerometer_y_prev-accelerometer_y_curr)>=0.12)) && accelerometer_y_prev!=0.00 && int(millis()) > 8000){
    Serial.print("Movement Detected");
    Serial.println("  ");

    Serial.print("Current X:");
    Serial.println(accelerometer_x_curr);
    Serial.print("Previous X:");
    Serial.println(accelerometer_x_prev);
    Serial.print("Current Y:");
    Serial.println(accelerometer_y_curr);
    Serial.print("Previous Y:");
    Serial.println(accelerometer_y_prev);
    Serial.print("Current Z:");
    Serial.println(accelerometer_z_curr);
    Serial.print("Previous Z:");
    Serial.println(accelerometer_z_prev);

    return true;
  }
  return false;
}

void motionDetectionHandler(){                                        //if it detects motion sends notification to Master Node
  if(digitalRead(digital_pir_sensor) && int(millis()) > 8000){      //millis is checked because initially the sensor fires
    Serial.println("Motion Detected");
    sendToMaster("GET /kitchen/motion HTTP/1.1");
    previous_millis = millis();
  }
}

//Detects variation in the air pressure and sends notification to the Master Node

void pressureHandler(){
  int current_pressure = pressure_sensor.getPressure();

  if(abs(current_pressure - prev_pressure) > 8 && abs(current_pressure - prev_pressure) < 1000 && prev_pressure != 0){                 
    sendToMaster("GET /kitchen/pressure HTTP/1.1");
    previous_millis = millis();
  }
  prev_pressure = current_pressure;
}

void moveLock(int pos){                    //Setting up attach and detach because Servo is impacting WiFi Antenna performance
  mylock.attach(7);
  mylock.write(pos);
  delay(1000);
  mylock.detach();
  resetFunc();
}

int readLock(){
  mylock.attach(7);
  int current_position = mylock.read();
  delay(1000);
  mylock.detach();
  return current_position;
}

void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  pinMode(digital_pir_sensor, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  while (!Serial);

  //////////////////////  First Connect to WIFI /////////////////////////
  connectToWifi(ssid,pass);

  //Start Server after Wifi connection has been established 
  server.begin();
  if(!pressure_sensor.init()){
    Serial.println("Error initiating pressure sensor");
  }
  Accelerometer.begin();

  Serial.println("Setup Ended");
}

void loop() {

  doorHandleHandler();

  motionDetectionHandler();

  pressureHandler();

  if(millis() - previous_millis > 300000){
    sendHeartbit();
  }


  WiFiClient client = server.available();
  String readString;

  if(client){
    Serial.print("New Client!");
    String currentLine = "";
    while (client.connected()){
      if(client.available()){

        char c = client.read();
        Serial.write(c);
        readString += c;
        if (c=='\n'){
          if (readString.indexOf("kitchen/lock") > 0){             //If receive message on endpoint kitchen_air do this
            Serial.print("kitchen/lock");
            moveLock(20);
          }else if (readString.indexOf("kitchen/unlock") > 0){
            Serial.print("kitchen/unlock");
            moveLock(0);
          }else if (c != '\r') {  // if you got anything else but a carriage return character,
            currentLine += c;      // add it to the end of the currentLine
          }
        }
      }
    }
    // close the connection:
    client.stop();
    Serial.println("client disconnected");
  }

  delay(200);
}
