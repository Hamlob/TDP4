#include <ntagsramadapter.h>
#include "Arduino.h"
#define HARDI2C
#include <Wire.h>
String command;
byte serialOutputBuffer[100];
int bufferPointer = 0;
byte storedText[100];
Ntag ntag(Ntag::NTAG_I2C_1K, 7, 9);
NtagSramAdapter ntagAdapter(&ntag);

#define NFC_FD 2
bool fd_old, fd_new;
bool fallingEdge(bool old_value, bool new_value) {
  if (old_value == 1 && new_value == 0) {
    return true;
  }
  return false;
}

void writeNameSerial() {
  Serial.print("My name is ");
  for (int i = 0; i < 100; i++) {
    if (storedText[i] == 0x7C) { // | char separates email from name
      break;
    }
    Serial.print((char)storedText[i]);
  }
  Serial.println("");
}

void showBlockInHex(byte* data, byte size) {
  for (int i = 0; i < size; i++) {
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}

void readMemory() {
  byte readeeprom[16];
  for (int i = 0; i < 0xFF; i++) {
    if (ntag.readEeprom(16 * i, readeeprom, 16)) {
      Serial.print("0x");
      Serial.print(i, HEX);
      Serial.print(": ");
      for (int j = 0; j < 16; j++) {
        if (readeeprom[j] < 0x10) {
          Serial.print("0"); //if one hex digit print leading zero
        }
        Serial.print(readeeprom[j], HEX);
        Serial.print(" : ");
      }
      Serial.println("");

    } else {
      Serial.println("memory read finished");
      break;
    }
  }
}

int parseText(byte* data, byte size, bool isNDEF, bool doPrint = false, int PacketCount = 0, bool findEmail = false) {
  int start = 0;
  //bool reachedEmailYet = false;
  if (isNDEF && PacketCount == 0) { // if reading NDEF, skip header information
    start = 9;
  }

  for (int i = start; i < size; i++) { //skips first bit of the register 0x06 which is still header
    if (findEmail) {
      if (data[i - 1] == 0x7C) { //if | character, email found!
        Serial.println("found it");
        bufferPointer = 0; //write over stored name with email

        /*serialOutputBuffer[bufferPointer] = data[i];
          reachedEmailYet = true;
          Serial.println("set true");
          continue; //avoids adding | into email string
          }
          if (reachedEmailYet == false) { //if looking for email section and separating char | not reached yet
          Serial.println("in not found if");
          Serial.println();
          //Serial.println("looking for |");
          continue; //skip as haven't reached email portion of string yet
          }*/
      }
    }

    serialOutputBuffer[bufferPointer] = data[i];
    if (data[i] == 0xFE) { //if ndef text end character
      
      for (int i = bufferPointer+1; i < 100; i++) { //purges remainder of old buffer
        serialOutputBuffer[i] = 0;
      }
      if (doPrint) {
        Serial.println();
      }
      return i; //return pointer to end char
    }
    bufferPointer += 1;
    if (doPrint) {
      Serial.print((char)data[i]);
    }
  }
  /*for (int i = bufferPointer; i < 100; i++) { //purges remainder of old buffer
    serialOutputBuffer[i] = 0;
    }*/

  return 0; //not yet finished
}

void erase(int startByte = 0, int finishByte = 0x37 * 16, bool doPrint = false) {
  byte zeros[2 * 16];
  int startRemainder = (16 - startByte % 16) % 16;
  int endRemainder = finishByte % 16 + 1;
  /*debugging
     Serial.println("start byte" + String(startByte, HEX));
    Serial.println("end byte" + String(finishByte, HEX));
    Serial.println("start remainder" + String(startRemainder));
    Serial.println("end remainder" + String(endRemainder));
  */

  //defines empty packet of data
  for (byte i = 0; i < 17; i++) {
    zeros[i] = 0x00;
  }

  if (startRemainder != 0) { //overwrite starting partial packet
    //Serial.println("erasing start packet");
    if (ntag.writeEeprom(startByte, zeros, startRemainder)) {
    }
  }
  //Serial.println("erasing middle packets");
  for (int j = startByte; j < int(finishByte / 16); j++) {
    if (ntag.writeEeprom(16 * j, zeros, 16)) { //overwrite integer number of packets
    }
    else if (j != 0x37) {
      Serial.print("Erase failed on line");
      Serial.println(j, HEX);
      //break;
    }
  }
  if (endRemainder != 0) { //overwrite ending partial packet
    //Serial.println("erasing end packet");
    if (ntag.writeEeprom((int(finishByte / 16) * 16), zeros, endRemainder)) {
    }
  }


  zeros[15] = 0xFF; //ensures end character still in place
  if (ntag.writeEeprom(16 * 0x37, zeros, 16)) {
  }
  if (doPrint) {
    Serial.println("Erase Succesful");
  }
}


int toggleNDEF(bool printSuccessMessage = false, int ndefHeadAddr = 0x36) {
  byte localTextHeader[9] = {0xB0, 0x0B, 0x1E, 0x5B, 0x00, 0xB1, 0x35, 0x69, 0x69};
  byte NDEFHeader[9];
  byte readeeprom[9];
  //unsigned char NDEFHeader[];
  byte isLocal = true;
  if (ntag.readEeprom(0, readeeprom, 9)) { //if read succesful

    //check if header section matches the local code
    for (int i = 0; i < 9; i++) {
      if ( readeeprom[i] != localTextHeader[i]) {
        isLocal = false;
        break;
      }
    }

    if (isLocal) { //if local header
      if (ntag.readEeprom(16 * ndefHeadAddr, NDEFHeader, 9)) { //read ndef header from eeprom
        ntag.writeEeprom(0, NDEFHeader, 9); //swap to NDEF
      } else {
        Serial.println("Swap back to NDEF was unsuccesful");
      }
    } else { //if not local
      //NDEFHeader = readeeprom;
      ntag.writeEeprom(ndefHeadAddr * 16, readeeprom, 9); //save ndef header
      ntag.writeEeprom(0, localTextHeader, 9); //swap to local
    }
    if (printSuccessMessage) {
      Serial.println("NDEF Toggle Complete");
    }
    return ndefHeadAddr; //return ndef header address
  }
}

int readText(bool doPrint = true, bool isNDEF = true, int addr = 0x0, bool findEmail = false) {
  bufferPointer = 0;
  byte readeeprom[16];
  int packetCounter = addr;
  int endPointer = 0;

  while (endPointer == 0) {
    if (ntag.readEeprom(16 * packetCounter, readeeprom, 16)) {
      endPointer = parseText(readeeprom, 16, isNDEF, doPrint, packetCounter, findEmail);
      packetCounter += 1;
      if (packetCounter == 0x38) { //if nothing found
        Serial.println("No text found");
        //readComplete = true;
      }
    } else {
      Serial.println("read failed");
      Serial.println(packetCounter, HEX);
      break;
    }
  }
  return endPointer + 16 * (packetCounter - 1); //returns byte number of 0xFE end character
}


void preserveName() {
  //byte localTextHeader[9] = {0xB0, 0x0B, 0x1E, 0x5B, 0x00, 0xB1, 0x35, 0x69, 0x69};
  byte readeeprom[9];
  bool newText = false;
  int endPointer = readText(false, true, 0x0); //reads memory to find text, updating the buffer string, returns end pointer

  //showBlockInHex(serialOutputBuffer, 100);
  //showBlockInHex(storedText, 100);
  for (int i = 0; i < 100; i++) {
    if (serialOutputBuffer[i] != storedText[i]) {
      newText = !newText;
      break;
    }
  }
  if (newText) {
    erase(endPointer + 1, 109, false);
    for (int j = 0; j < 100; j++) {
      storedText[j] = serialOutputBuffer[j];
    }
    if (ntag.writeEeprom(0x30 * 16, storedText, 100)) {
      //Serial.println("New Name Stored");

    } else {
      Serial.println("Name not stored");
    }
    toggleNDEF(false);
    //purge remainder of old name
    //erase(endPointer+1, 109);
    erase(0, 109);
    writeNameSerial();
    //readMemory();
  }
}
void showEmail() {
  byte customNdefTextHeader[9] = {0x03, 0x00, 0xD1, 0x01, 0x00, 0x54, 0x02, 0x65, 0x6E};
  readText(false, true, 0x30, true);
  customNdefTextHeader[1]=bufferPointer+7;//from NDEF text standard
  customNdefTextHeader[4]=bufferPointer+3;
  //showBlockInHex(serialOutputBuffer, 100);
  //showBlockInHex(storedText, 100);
  Serial.println(bufferPointer,HEX);
  if (ntag.writeEeprom(0, customNdefTextHeader, 9)) {
    //Serial.println("New Name Stored");
  }
  if (ntag.writeEeprom(9, serialOutputBuffer, 100)) {
    //Serial.println("New Name Stored");
  }else{
    Serial.println("write failed");
  }
  //readMemory();
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println("start");
  if (!ntag.begin()) {
    Serial.println("Can't find ntag");
  }
  //preserveName();//deals with programming whilst turned off
}

void loop() {
  // put your main code here, to run repeatedly:
  //byte readeeprom[16];
  //int packetCounter = 0;
  //bool readComplete = 0;
  byte eepromdata[2 * 16];


  fd_new = !digitalRead(NFC_FD); //active low
  if (fallingEdge(fd_old, fd_new)) { //if nfc field removed i.e process finished
    preserveName();
  }
  fd_old = fd_new;


  if (Serial.available()) {
    command = Serial.readStringUntil('\n');
    command.trim();


    if (command.equals("read")) {
      readText(true, true, 0x30);
    }
    if (command.equals("email")) {
      showEmail();
    }

    else if (command.equals("memory")) {
      readMemory();
    }
    else if (command.equals("write")) {
      for (byte i = 0; i < 2 * 17; i++) {
        eepromdata[i] = 0x00 | i;
      }
      if (ntag.writeEeprom(0, eepromdata, 9)) {
        Serial.println("Write complete");
      }
      else {
        Serial.println("Write failed");
      }
    }
    else if (command.equals("erase")) {
      erase(0, 0x37 * 16, true);
    }
    else if (command.equals("toggle")) {
      toggleNDEF(true);
    }
    else {
      Serial.println("bad command");
    }
    command = "";
  }
  delay(1000);
}
