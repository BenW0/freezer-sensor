#pragma once

#include <TimeLib.h>

void PrintTime(time_t epoch)
{
  TimeElements tm;
  breakTime(epoch, tm);
  Serial.print(tm.Year - 30 + 2000);
  Serial.print("-");
  Serial.print(tm.Month);
  Serial.print("-");
  Serial.print(tm.Day);
  Serial.print(" ");
  Serial.print(tm.Hour);
  Serial.print(":");
  Serial.print(tm.Minute);
  Serial.print(":");
  Serial.print(tm.Second);
}
