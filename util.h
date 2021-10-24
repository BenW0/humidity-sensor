#pragma once

#include <TimeLib.h>
#include <string>

void PrintTime(time_t epoch, bool forUrl=false)
{
  TimeElements tm;
  breakTime(epoch, tm);
  Serial.print(tm.Year - 30 + 2000);
  Serial.print("-");
  Serial.print(tm.Month);
  Serial.print("-");
  Serial.print(tm.Day);
  if (forUrl)
    Serial.print("%20");
  else
    Serial.print(" ");
  Serial.print(tm.Hour);
  Serial.print(":");
  Serial.print(tm.Minute);
  Serial.print(":");
  Serial.print(tm.Second);
}

String StrTime(time_t epoch, bool forUrl=false)
{
  TimeElements tm;
  breakTime(epoch, tm);
  String s = String(tm.Year - 30 + 2000) + "-" + String(tm.Month) + "-" + String(tm.Day) + (forUrl ? "%20" : " ") +
             String(tm.Hour) + ":" + (tm.Minute < 10 ? "0" : "") + String(tm.Minute) + ":" + (tm.Second < 10 ? "0" : "") + String(tm.Second);
  return s;
}