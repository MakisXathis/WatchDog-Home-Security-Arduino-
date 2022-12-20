#ifndef HELPERS_H
#define HELPERS_H

#include <Arduino.h>
#include "Base64.h"
#include <SPI.h>
#include <WiFiNINA.h>

extern void _connectToWifi(const char *ssid, const char *pass);
extern void _printWiFiStatus();
extern byte response(WiFiSSLClient client);
extern void encode64(String InputString, char *res);   
  /* res must point to a char[] of the proper length
   *  usage:
   *    int encodedLength = Base64.encodedLength(InputString.length());
   *    char encodedString[encodedLength+1];
   *    encode64(InputString, encodedString);
   *    res[encodedLength] = '\0';
   */

#endif

