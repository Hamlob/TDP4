#include <ntagsramadapter.h>
#include "Arduino.h"
#include <arduino-timer.h>
#define HARDI2C
#include <Wire.h>
String command;
byte serialOutputBuffer[100];
int bufferPointer = 0;
byte storedText[100];
bool isShowingEmail = false;
Ntag ntag(Ntag::NTAG_I2C_1K, 7, 9);
NtagSramAdapter ntagAdapter(&ntag);
auto timer = timer_create_default();
#define NFC_FD 2
#define PUSHB_1 4             //button inputs
#define PUSHB_2 7
bool push1_old, push2_old;               // old values of the inputs, internally pulled up so = 1
bool push1_new, push2_new;
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
  //Serial.println("entered parseText");
  if (isNDEF && PacketCount == 0) { // if reading NDEF, skip header information
    start = 9;
  }

  for (int i = start; i < size; i++) { //skips first bit of the register 0x06 which is still header
    if (findEmail) {
      if (data[i - 1] == 0x7C) { //if | character, email found! //issue with getting 0-1th element?
        bufferPointer = 0; //write over stored name with email
      }
    }

    serialOutputBuffer[bufferPointer] = data[i];
    if (data[i] == 0xFE) { //if ndef text end character

      for (int i = bufferPointer + 1; i < 100; i++) { //purges remainder of old buffer
        serialOutputBuffer[i] = 0;
      }
      if (doPrint) {
        Serial.println();
      }
      return i; //return pointer to end char
    }
    bufferPointer += 1;
    if (bufferPointer == 100) { //break if reached end of buffer without finding anything
      return 0xFF; //return nothing found code
    }
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
      if (endPointer == 0xFF) { // if nothing found
        return 0xFF;
      }
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

  //Serial.println("entered preserve name");
  byte readeeprom[9];
  bool newText = false;
  int endPointer = readText(false, true, 0x0); //reads memory to find text, updating the buffer string, returns end pointer
  if (endPointer == 0xFF) { //if no new text has been written, skip
    //Serial.println("skipping rest of preserve name, no tag found");
    return;
  }
  //Serial.print("endPointer:");
  //Serial.println(endPointer, HEX);
  //showBlockInHex(serialOutputBuffer, 100);
  //showBlockInHex(storedText, 100);
  for (int i = 0; i < (bufferPointer + 1); i++) { //change 100 to bufferPointer?
    if ((serialOutputBuffer[i] != storedText[i]) && (serialOutputBuffer[i] != 0) ) { //was & and !=
      //Serial.println("different");
      newText = !newText;
      break;
    }
  }
  //delay(1000); //for debugging to delay crash. REMOVE
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
bool toggleShowEmail(void *) {
  byte customNdefTextHeader[9] = {0x03, 0x00, 0xD1, 0x01, 0x00, 0x54, 0x02, 0x65, 0x6E};
  isShowingEmail = !isShowingEmail;
  readText(false, true, 0x30, true);
  customNdefTextHeader[1] = bufferPointer + 7; //from NDEF text standard
  customNdefTextHeader[4] = bufferPointer + 3;
  //showBlockInHex(serialOutputBuffer, 100);
  //showBlockInHex(customNdefTextHeader, 9);
  byte writeData[16];
  bool finished = false;
  int packetCounter = 0;
  //showBlockInHex(storedText, 100);
  //Serial.println(bufferPointer, HEX);

  if (isShowingEmail) {
    Serial.println("Email now showing");
    bufferPointer = 0;
    for (int i = 0; i < 16; i++) {  //generates first packet including header
      if (i < 9) {
        writeData[i] = customNdefTextHeader[i];
      } else {
        writeData[i] = serialOutputBuffer[bufferPointer];
        bufferPointer += 1;
      }
    }
    if (ntag.writeEeprom(0, writeData, 16)) { //writes first packet
      packetCounter += 1;
    }
    while (!finished) {
      for (int j = 0; j < 16; j++) { //generates further packets
        if (finished) {
          writeData[j] = 0;
          continue;
        }
        writeData[j] = serialOutputBuffer[bufferPointer];
        if (serialOutputBuffer[bufferPointer] == 0xFE) {
          finished = true;
        }
        bufferPointer += 1;
      }
      if (ntag.writeEeprom(16 * packetCounter, writeData, 16)) { //writes remaining packets
        packetCounter += 1;
      }
    }
  } else {
    Serial.println("Email now hidden");
    erase(0, 109, false);
  }
  /*
    if (ntag.writeEeprom(9, serialOutputBuffer, 109)) {
    //Serial.println("New Name Stored");
    }else{
    Serial.println("write failed");
    }
    //readMemory();*/
  return false; //tells timer not to repeat
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(PUSHB_1, INPUT_PULLUP);
  pinMode(PUSHB_2, INPUT_PULLUP);

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
  timer.tick();

  push1_new = digitalRead(PUSHB_1);
  push2_new = digitalRead(PUSHB_2);
  fd_new = !digitalRead(NFC_FD); //active low
  if (fallingEdge(fd_old, fd_new) && (!isShowingEmail)) { //if nfc field removed i.e process finished and not showing email, update
    preserveName();
  }

  
  if (fallingEdge(push1_old, push1_new) && (!isShowingEmail)) { //if nfc field removed i.e process finished and not showing email, update
    timer.in(0, toggleShowEmail); //show email for 10 seconds
    timer.in(10000, toggleShowEmail);
  }

  push1_old = push1_new;
  push2_old = push2_new;
  fd_old = fd_new;


  if (Serial.available()) {
    command = Serial.readStringUntil('\n');
    command.trim();


    if (command.equals("read")) {
      readText(true, true, 0x30);
    }

    /*
      if (command.equals("email")) {
      Serial.println("toggling show email");
      //isShowingEmail = !isShowingEmail;
      //toggleShowEmail(isShowingEmail);
      //toggleShowEmail(true);
      //toggleShowEmail();
      timer.in(0, toggleShowEmail);
      timer.in(10000, toggleShowEmail);
      }*/

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
  delay(5);
}
