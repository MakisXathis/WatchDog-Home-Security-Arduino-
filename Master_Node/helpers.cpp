#include "helpers.h"

void _connectToWifi(const char *ssid, const char *pass){
  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }
  int status = WL_IDLE_STATUS;
  // attempt to connect to WiFi network:
  while (status != WL_CONNECTED) {
    Serial.print("Connecting to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);

    // wait 1 seconds for connection:
    delay(1000);
  }
  Serial.println("Connected to wifi");
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

byte response(WiFiSSLClient client){
  // Wait for a response for up to X seconds
  int loopCount = 0;
  while (!client.available()) {
    delay(1);
    loopCount++;
    // if nothing received for 1O00 milliseconds, timeout
    if (loopCount > 10000) {
      //client.stop();
      Serial.println(F("Timeout"));
      return -1;
    }
  }

  // Take a snapshot of the response code
  byte respCode = client.peek();
  Serial.print("response: <start>");
  while (client.available()){
    Serial.write(client.read());
  }
  Serial.println("<end>");
  
  if (respCode >= '4'){
    Serial.print("Failed in eRcv with response: ");
    Serial.println(respCode);
    return 0;
  }
  return 1;
}
void encode64(String InputString, char *res){   
  /* res must point to a char[] of the proper length
   *  usage:
   *    int encodedLength = Base64.encodedLength(InputString.length());
   *    char encodedString[encodedLength+1];
   *    encode64(InputString, encodedString);
   *    res[encodedLength] = '\0';
   */
  int inputStringLength = InputString.length();
  const char *inputString = InputString.c_str(); 
  Base64.encode(res, (char*)inputString, inputStringLength);
}

