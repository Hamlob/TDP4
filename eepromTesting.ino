#include <ntagsramadapter.h>
#include "Arduino.h"
#define HARDI2C
#include <Wire.h>
String command;
String SerialOutputBuffer;
Ntag ntag(Ntag::NTAG_I2C_1K, 7, 9);
NtagSramAdapter ntagAdapter(&ntag);


void testUserMem() {
  byte eepromdata[2 * 16];
  byte readeeprom[16];


  if (ntag.readEeprom(0, readeeprom, 16)) {
    showBlockInHex(readeeprom, 16);
  }

  for (byte i = 0; i < 2 * 17; i++) {
    eepromdata[i] = 0x00;
    //eepromdata[i]=0x60 | i;
  }

  Serial.println("Writing block 1");
  if (!ntag.writeEeprom(0, eepromdata, 16)) {
    Serial.println("Write block 1 failed");
  }
  Serial.println("Writing block 2");
  if (!ntag.writeEeprom(16, eepromdata + 16, 16)) {
    Serial.println("Write block 2 failed");
  }
  Serial.println("\nReading memory block 1");
  if (ntag.readEeprom(0, readeeprom, 16)) {
    showBlockInHex(readeeprom, 16);
  }
  Serial.println("Reading memory block 2");
  if (ntag.readEeprom(16, readeeprom, 16)) {
    showBlockInHex(readeeprom, 16);
  }
  Serial.println("Reading bytes 10 to 20: partly block 1, partly block 2");
  if (ntag.readEeprom(10, readeeprom, 10)) {
    showBlockInHex(readeeprom, 10);
  }
  Serial.println("Writing byte 15 to 20: partly block 1, partly block 2");
  for (byte i = 0; i < 6; i++) {
    eepromdata[i] = 0x00 | i;
  }
  if (ntag.writeEeprom(15, eepromdata, 6)) {
    Serial.println("Write success");
  }
  Serial.println("\nReading memory block 1");
  if (ntag.readEeprom(0, readeeprom, 16)) {
    showBlockInHex(readeeprom, 16);
  }
  Serial.println("Reading memory block 2");
  if (ntag.readEeprom(16, readeeprom, 16)) {
    showBlockInHex(readeeprom, 16);
  }
}


void showBlockInHex(byte* data, byte size) {
  for (int i = 0; i < size; i++) {
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}

/*
void printNDEFText(byte* data, byte size) {
  Serial.println("entered void");
  for (int i = 1; i < size; i++) { //skips first bit of the register 0x06 which is still header
    if (data[i] == 0xFE) { //if ndef text end character
      break;
    }
    Serial.print((char)data[i]);
  }
  Serial.println();
}*/

int parseText(byte* data, byte size, int PacketCount) {
  
  int start =0;
  if (PacketCount ==0){ // if first packet skip header information
    start = 9;
  }

  for (int i = start; i < size; i++) { //skips first bit of the register 0x06 which is still header
    if (data[i] == 0xFE) { //if ndef text end character
      Serial.println(SerialOutputBuffer);
      return 1; //return finished boolean
    }
    //Serial.print((char)data[i]);
    SerialOutputBuffer += (char)data[i]; //append character to output
  }
  return 0; //not yet finished
  delay(1000);
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println("start");
  if (!ntag.begin()) {
    Serial.println("Can't find ntag");
  }

  //testUserMem();
  SerialOutputBuffer = "";
}

void loop() {
  // put your main code here, to run repeatedly:
  byte readeeprom[100];
  int packetCounter = 0;
  bool readComplete = 0;
  if (Serial.available()) {
    command = Serial.readStringUntil('\n');
    command.trim();
    if (command.equals("read")) {
      SerialOutputBuffer = "";
      Serial.println("reading NDEF text...");

      while(!readComplete){
        if (ntag.readEeprom(16*packetCounter, readeeprom, 16)) {
          readComplete=parseText(readeeprom, 16, packetCounter);
          packetCounter+=1; 
        } else {
          Serial.println("read failed");
        }
      }
    }

    else {
      Serial.println("bad command");
    }

  }
}
