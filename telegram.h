/* Telegram interaction header -- project specific Telegram code
*/
#include <WiFiClientSecureAxTLS.h>
using namespace axTLS;
#include <UniversalTelegramBot.h>

#include "secrets.h"

String getStatsString();

struct TelegramIO
{
  
  UniversalTelegramBot bot;

#ifdef TESTING
  int Bot_mtbs = 5000; //mean time between scan messages
#else
  int Bot_mtbs = 5000; //mean time between scan messages
#endif
  long Bot_lasttime;   //last time messages' scan has been done
  bool Start = false;

  TelegramIO(WiFiClientSecure & client)
  : bot(TelegramBotToken, client)
  {}

  void handleNewMessage(telegramMessage & msg)
  {
    String chat_id = String(msg.chat_id);
    String text = msg.text;

    String from_name = msg.from_name;
    if (from_name == "")
      from_name = "Guest";

    if (text == "s")
    {
      String s = getStatsString();
      bot.sendMessage(chat_id, s, "");
      }

      if (text == "/start")
      {
        bot.sendMessage(chat_id, "Start", "");
      }
      else
      {
        String welcome = "Welcome to the Freezer Sensor, " + from_name + ".\n";
        welcome += "Commands:\n\n";
        welcome += "s : Check status now\n";
        welcome += "Messages may take several seconds to respond to save energy\n";
        bot.sendMessage(chat_id, welcome, "Markdown");
      }
    
  }

  void update()
  {
    if (millis() > Bot_lasttime + Bot_mtbs)
    {
      int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

      while (numNewMessages)
      {
        Serial.print("Telegram got ");
        Serial.print(numNewMessages);
        Serial.println(" messages");

        for (int i = 0; i < numNewMessages; i++)
        {
          handleNewMessage(bot.messages[i]);
          numNewMessages = bot.getUpdates(bot.last_message_received + 1);
        }
      }

      Bot_lasttime = millis();
    }
  }
};