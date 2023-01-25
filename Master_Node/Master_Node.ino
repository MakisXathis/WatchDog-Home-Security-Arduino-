/*This example creates an SSLclient object that connects and transfers
data using always SSL.

I then use it to connect to smpt.gmail.com and send an email.

Things to note:
* gmail sending account: must have 2 factor identification enabled and an app specific password created
*                        for this usage
* lots of things are tricky here!
*/

#include <SPI.h>
#include <WiFiNINA.h>
#include "helpers.h"
#include "arduino_secrets.h"
#include "Seeed_BMP280.h"
#include "stdlib.h"
#include <string.h>


///////please enter your sensitive data in the Secret tab/arduino_secrets.h
const char ssid[] = SECRET_SSID;        // your network SSID (name)
const char pass[] = SECRET_PASS;        // your network password (use for WPA)

const String gAcc   = SECRET_SEND_ACCOUNT,           // "something@somewhere.com"
             gPass  = SECRET_SEND_ACCOUNT_PASSWORD;  // "the_password"

const int port = 465; //587 for TLS does not work
const char mailserver[] = "smtp.gmail.com";    // name address for Gmail SMTP (using DNS)
const String recipient = "MNB1000@yahoo.gr";

bool system_activation = true;                                            //If false the system cannot go in alarm state in case of sensor detection
bool in_alarm = false;                                                    //In case of sensor detection goes to true
bool kitchen_heartbit_email_flag = false;

unsigned long kitchen_heartbit = 0;                                                       //used to detect if an Edge Node is offline
bool kitchen_pressure = false, kitchen_accelerometer = false, kitchen_motion = false;     //stores the alarm state of the sensors from one Edge Device
bool kitchen_locked=false;                                                                //stores the state of the Edge Node lock
bool kitchen_lock_change=false;
char kitchen_ip[] = "192.168.161.195";                                                       //stores the IP of the Edge Node in case we need to control its lock

WiFiServer server(80);                                                                    //server to receive requests from the users


bool sendToEdge(char dest[],char message[]){                   //Sends a message to the Edge node received in order to control the lock (servo)
  WiFiClient client_tosend;
  Serial.println("Attempting communication with "+String(dest));
  if(client_tosend.connect(dest, 8080)){
    Serial.println("connected to kitchen");
    client_tosend.println(message);
    client_tosend.println();
    Serial.println("Message Sent");
  }
  if(client_tosend.connected()){
    client_tosend.stop();
    return true;
  }
  return false;
}

void SendEmail(char server[], int port, String sender,String password, String recipient, char outgoing[], String subject) {             //Use this method to send an email from your google account using the smtp.gmail.com on SSL port 465

  int encodedLength = Base64.encodedLength(sender.length());
  char encodedAccount[encodedLength+1];
  encode64(sender, encodedAccount);
  encodedAccount[encodedLength] = '\0';
  
  encodedLength = Base64.encodedLength(password.length());
  char encodedPass[encodedLength+1];
  encode64(gPass, encodedPass);
  encodedPass[encodedLength] = '\0';

  Serial.println("\nConnecting to server: " + String(server) +":" +String(port));

  WiFiSSLClient sslclient;
  
  if (sslclient.connectSSL(server, port)){
    Serial.println("Connected to server");
    if (response(sslclient) ==-1){
      String s = server + String(" port:")+ String(port);
      Serial.print("no reply on connect to ");
      Serial.println(s);
    }

    /////////////////////// now do the SMTP dance ////////////////////////////
    
    Serial.println("Sending Extended Hello: <start>EHLO yourDomain.org<end>");

    //// PLEASE UPDATE /////
    sslclient.println("EHLO yourDomain.org");
    if (response(sslclient) ==-1){
      Serial.println("no reply EHLO yourDomain.org");
    }
    
    Serial.println("Sending auth login: <start>AUTH LOGIN<end>");
    sslclient.println("AUTH LOGIN");
    if (response(sslclient) ==-1){
      Serial.println("no reply AUTH LOGIN");
    }

    Serial.println("Sending account: <start>" +String(encodedAccount) + "<end>");
    sslclient.println(encodedAccount); 
    if (response(sslclient) ==-1){
      Serial.println("no reply to Sending User");
    }
    
    Serial.println("Sending Password: <start>" +String(encodedPass) + "<end>");  
    sslclient.println(encodedPass); 
    if (response(sslclient) ==-1){
      Serial.println("no reply Sending Password");
    }
    
    //// PLEASE UPDATE /////
    Serial.println("Sending From: <start>MAIL FROM: <"+sender+"><end>");
    // your email address (sender) - MUST include angle brackets
    sslclient.println("MAIL FROM: <"+sender+">");
    if (response(sslclient) ==-1){
      Serial.println("no reply Sending From");
    }

    //// PLEASE UPDATE /////
    // change to recipient address - MUST include angle brackets
    Serial.println("Sending To: <start>RCPT To: <"+recipient+"><end>");
    sslclient.println("RCPT To: <"+recipient+">");
    if (response(sslclient) ==-1){
      Serial.println("no reply Sending To");
    }
    
    Serial.println("Sending DATA: <start>DATA<end>");
    sslclient.println("DATA");
    if (response(sslclient) ==-1){
      Serial.println("no reply Sending DATA");
    }

    //// PLEASE UPDATE /////
    Serial.println("Sending email: <start>");
    // recipient address (include option display name if you want)
    sslclient.println("To: SomebodyElse <"+recipient+">");

    //// PLEASE UPDATE /////
    // send from address, subject and message
    sslclient.println("From: Arduino <"+sender+">");
    sslclient.println("Subject: "+subject);
    sslclient.println(outgoing);
  
    // IMPORTANT you must send a complete line containing just a "." to end the conversation
    // So the PREVIOUS line to this one must be a println not just a print
    sslclient.println(".");
    if (response(sslclient) ==-1){
      Serial.println("no reply Sending '.'");
    }
    
    Serial.println("Sending QUIT");
    sslclient.println("QUIT");
    if (response(sslclient) ==-1){
      Serial.println("no reply Sending QUIT");
    }
    sslclient.stop();
  }
  else{
    Serial.println("SSL client status: "+String(sslclient.status()));
    Serial.println("failed to connect to server");
  }
  Serial.println("Done.");

}

void resetAllAlarms(){                                                              //Resets all the active alarms on the web interface
  kitchen_pressure = false, kitchen_accelerometer = false, kitchen_motion = false;
}

void deactivateAlarm(){                                                             //Stops the buzzer and light and resets all active alarms
  in_alarm = false;
  resetAllAlarms();
  digitalWrite(2,LOW);
  tone(6,0);  
}

void systemShutdownOrStartUp(bool activate){         //Disables the system from sending notifications and activating the Buzzer/LED
  if (activate){
    system_activation = true;
    in_alarm=false;
    resetAllAlarms();
  }
  else{
    system_activation = false;
    in_alarm = false;
    resetAllAlarms();
    digitalWrite(2,LOW);
    tone(6,0);
  }
}

void activateAlarm(char message[], String subject){                                 //Activates the Buzzer and LED if system is in alarm and sends a notification email
  if (system_activation && in_alarm){
    digitalWrite(2,HIGH);
    tone(6,200);
    SendEmail(mailserver, port, gAcc, gPass, recipient, message, subject);
  }
}

void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(2,OUTPUT);
  pinMode(6,OUTPUT);
  while (!Serial); 
  //////////////////////  First Connect to WIFI /////////////////////////
  _connectToWifi(ssid,pass);

  //Start Server after Wifi connection has been established 
  server.begin();
  

  Serial.println("Setup Ended");
  ///////////////////// then connect to server ///////////////////////////
  //SendEmail(mailserver, port, gAcc, gPass, recipient, "This is a message from your Friendly Arduino", "Good To Go!");
}

void loop() {

  WiFiClient client = server.available();
  String readString;

  bool kitchen_lock_change = false;
  bool kitchen_to_lock = false;

  if(client){
    Serial.print("New Client!");
    String currentLine = "";
    while (client.connected()){
      if(client.available()){

        char c = client.read();
        Serial.write(c);
        readString += c;
          if (c=='\n'){
            if (readString.indexOf("kitchen/pressure") > 0){             //If receive message on endpoint kitchen_air do this
              Serial.print("kitchen/pressure");
              kitchen_heartbit = millis();
              if (!kitchen_pressure){
              kitchen_pressure = true; in_alarm = true;
              activateAlarm("Kitchen - Significant Pressure change detected!!","HOME ALARM");
              }
            }else if (readString.indexOf("kitchen/accelerometer") > 0){     //If receive message on endpoint kitchen_accel do this
              Serial.print("kitchen/accelerometer");
              kitchen_heartbit = millis();
              if(!kitchen_accelerometer){
                kitchen_accelerometer = true; in_alarm = true;
                activateAlarm("Kitchen - Door handle movement detected!!","HOME ALARM");
              }
            }else if (readString.indexOf("kitchen/motion") > 0){      //If receive message on endpoint kitchen_move do this
              Serial.print("kitchen/move");
              kitchen_heartbit = millis();
              if(!kitchen_motion){
                kitchen_motion = true; in_alarm = true;
                activateAlarm("Kitchen - Movement detected inside the house!!","HOME ALARM");
              }
            }else if (readString.indexOf("kitchen/heartbit") > 0){
              Serial.print("received heartbit from kitchen");
              kitchen_heartbit = millis();
              kitchen_heartbit_email_flag = false;
            }else{
              if (currentLine.length() == 0) {
                // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
                // and a content-type so the client knows what's coming, then a blank line:
                client.println("HTTP/1.1 200 OK");
                client.println("Content-type:text/html");
                client.println();

                // the content of the HTTP response follows the header:
                client.print("<h1>System Status: </h1>");
                if(in_alarm && system_activation){
                  client.print("<h1 style=\"background-color:red;\">In Alarm</h1>");
                  client.print("<a href=\"/H\">Deactivate Alarm</a><br>");
                }else{
                  client.print("<h1 style=\"background-color:green;\">OK</h1>");
                }                 
            
                client.print("<br>");
                if((millis() - kitchen_heartbit) > 660000){
                  client.print("<h2 style=\"background-color:grey;\">Kitchen Sensors</h2>");
                }else{
                  client.print("<h2>Kitchen Sensors</h2>");
                }
                client.print("<table border='1'>");
                client.print("<tr><th>Air Pressure</th>");
                client.print("<th>Motion Sensor</th>");
                client.print("<th>Door Handle</th>");
                client.print("<th>Lock Status</th></tr>");
                client.print("<tr><th>");
                if(kitchen_pressure)
                  client.print("NOK");
                else
                  client.print("OK");
                client.print("</th>");
                client.print("<th>");
                if(kitchen_motion)
                  client.print("NOK");
                else
                  client.print("OK");
                client.print("</th>");
                client.print("<th>");
                if(kitchen_accelerometer)
                  client.print("NOK");
                else
                  client.print("OK");
                client.print("</th>");
                client.print("<th>");
                if(kitchen_locked)
                  client.print("<a href=\"/kitchen/unlock\">Unlock Door</a><br>");
                else
                  client.print("<a href=\"/kitchen/lock\">Lock Door</a><br>");
                client.print("</th></tr>");
                client.print("</table>");
                client.print("<br>");
                
                if(system_activation){
                  client.print("<a href=\"/D\">Deactivate System</a><br>");
                }else{
                  client.print("<a href=\"/A\">Activate System</a><br>");
                }            
                

                // The HTTP response ends with another blank line:
                client.println();
                // break out of the while loop:
                break;
              } else {    // if you got a newline, then clear currentLine:
              currentLine = "";
              }
            }
          } else if (c != '\r') {  // if you got anything else but a carriage return character,
            currentLine += c;      // add it to the end of the currentLine
          }

          // Check to see if the client request was "GET /H" , "GET /D" ,"GET /A" or "GET /kitchen/(un)lock":
          if (currentLine.endsWith("GET /H")) {
            deactivateAlarm();               // GET /H stops the current alarm
          }
          if (currentLine.endsWith("GET /D")) {
            systemShutdownOrStartUp(false);          // GET /D turns the system alarms off
          }
          if (currentLine.endsWith("GET /A")) {
            systemShutdownOrStartUp(true);               // GET /A turns the system alarms on
          }
          if (currentLine.endsWith("GET /kitchen/lock")) {
            kitchen_lock_change = true; kitchen_to_lock = true;
          }
          if (currentLine.endsWith("GET /kitchen/unlock")) {
            kitchen_lock_change = true; kitchen_to_lock = false;
          }
        }
      }
    // close the connection:
    client.stop();
    Serial.println("client disconnected");
  }

  if(kitchen_lock_change){
    if(kitchen_to_lock){
      if(sendToEdge(kitchen_ip,"GET /kitchen/lock HTTP/1.1"))           // GET /kitchen/lock sends signal to Kitchen Edge to lock the door
        kitchen_locked = true;
    }
    if(!kitchen_to_lock){
      if(sendToEdge(kitchen_ip,"GET /kitchen/unlock HTTP/1.1"))
        kitchen_locked = false;                                          // GET /kitchen/unlock sends signal to Kitchen Edge to unlock the door
    }
  }

  if ((millis() - kitchen_heartbit) > 660000){
    if(!kitchen_heartbit_email_flag){
      SendEmail(mailserver, port, gAcc, gPass, recipient, "The kitchen Edge sensor has not sent any data for the past 10 minutes", "Kitchen Edge Offline");
      kitchen_heartbit_email_flag=true;
    }
  }

  delay(200);
}