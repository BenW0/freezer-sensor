/*
* Upload data to Google Sheets using a Google Script API (see details at
* http://embedded-lab.com/blog/post-data-google-sheets-using-esp8266/)
*/
// BY: Akshaya Niraula, heavily modified by Ben Weiss
// ON: Nov 11, 2016
// AT: http://www.embedded-lab.com/
#pragma once
#include "HTTPSRedirect.h"
#include "secrets.h"

const int httpsPort = 443;

class GoogleLogging
{
private:
  const char *host = "script.google.com";
  const char *googleRedirHost = "script.googleusercontent.com";

  HTTPSRedirect client;

  // Prepare the url (without the varying data)
  String url = String("/macros/s/") + GScriptId + "/exec?";

  const char *fingerprint = "F0 5C 74 77 3F 6B 25 D7 3B 66 4D 43 2F 7E BC 5B E9 28 86 AD";

public:
  GoogleLogging() : client(httpsPort)
  {}

  void setup()
  {
    
    client.setPrintResponseBody(true);

    Serial.print(String("Connecting to "));
    Serial.println(host);

    bool flag = false;
    for (int i = 0; i < 5; i++)
    {
      int retval = client.connect(host, httpsPort);
      if (retval == 1)
      {
        flag = true;
        break;
      }
      else
        Serial.println("Connection failed. Retrying…");
    }

    // Connection Status, 1 = Connected, 0 is not.
    Serial.println("Connection Status: " + String(client.connected()));
    Serial.flush();

    if (!flag)
    {
      Serial.print("Could not connect to server: ");
      Serial.println(host);
      Serial.println("Exiting…");
      Serial.flush();
      return;
    }

    // Data will still be pushed even certification don’t match.
    if (client.verify(fingerprint, host))
    {
      Serial.println("Certificate match.");
    }
    else
    {
      Serial.println("Certificate mis-match");
    }
  }

  // This is the main method where data gets pushed to the Google sheet
  void postData(String date, float meanValue, float maxValue)
  {
    if (!client.connected())
    {
      Serial.println("Connecting to client again…");
      client.connect(host, httpsPort);
    }
    String urlFinal = url + "date=" + date + "&meanValue=" + String(meanValue) + "&maxValue=" + String(maxValue);
    client.GET(urlFinal, host, googleRedirHost);
  }

};
