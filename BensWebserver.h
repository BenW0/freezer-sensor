// Simple webserver for log data


typedef void (*webCallback_t)(WiFiClient &);

class BensWebserver
{
private:
  // Variable to store the HTTP request
  String header;

  // Set web server port number to 80
  WiFiServer server;
 
  // Define timeout time in milliseconds (example: 2000ms = 2s)
  unsigned long timeoutTime{2000};

  // Function handle for gathering data to write when requested
  webCallback_t callback{nullptr};
  
public:

  /*
   * Initializes a bensWebserver object.
   * 
   * Args:
   *   callback: Callback function executed whenever we render a web page. This is the caller's
   *      chance to add html snippets to the <body> of the output page
   *   port: Port to serve on
   *   timeout:timeout before 
   */
  BensWebserver(webCallback_t callback, uint16_t port = 80, unsigned long timeout = 2000)
     : server(port),
     timeoutTime(timeout),
     callback(callback)
  {
  }

  /*
   * Starts the webserver
   */
   void begin()
   {
    server.begin();
   }

  /* process() - check for and respond to any http requests
   *  
   */
  void process()
  {
    WiFiClient client = server.available();
  
    if (client) {
      Serial.println("New Client.");
      String currentLine = "";
      
      unsigned long currentTime = millis();
      unsigned long previousTime = currentTime;
      
      while (client.connected() && currentTime - previousTime <= timeoutTime) {
        currentTime = millis();         
        if (client.available()) {
          char c = client.read();
          Serial.write(c);
          header += c;
          if (c == '\n') {
            if (currentLine.length() == 0) {
              // write header
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/html");
              client.println("Connection: close");
              client.println();
              
              // Display the HTML web page
              client.println("<!DOCTYPE html><html>");
              client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
              client.println("<link rel=\"icon\" href=\"data:,\">");
              client.println("<link rel=\"stylesheet\" crossorigin=\"anonymous\" href=\"https://raw.githubusercontent.com/macloo/html_css_templates/master/css/base.css\">");
              client.println("</head>");
              
              // Web Page body
              client.println("<body><h1>ESP8266 Web Server</h1>");
              callback(client);
              
              client.println("</body></html>");
              
              // The HTTP response ends with another blank line
              client.println();
              // Break out of the while loop
              break;
            } else { // if you got a newline, then clear currentLine
              currentLine = "";
            }
          } else if (c != '\r') {  // if you got anything else but a carriage return character,
            currentLine += c;      // add it to the end of the currentLine
          }
        }
      }
      // Clear the header variable
      header = "";
      // Close the connection
      client.stop();
      Serial.println("Client disconnected.");
      Serial.println("");
    }
  }
};
