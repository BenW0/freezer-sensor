#pragma once

#include <TimeLib.h>
#include <string>

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

String StrTime(time_t epoch)
{
  TimeElements tm;
  breakTime(epoch, tm);
  String s = String(tm.Year - 30 + 2000) + "-" + String(tm.Month) + "-" + String(tm.Day) + " " +
             String(tm.Hour) + ":" + (tm.Minute < 10 ? "0" : "") + String(tm.Minute) + ":" + (tm.Second < 10 ? "0" : "") + String(tm.Second);
  return s;
}