#include <ntagsramadapter.h>
#include "Arduino.h"
#include <arduino-timer.h>
#define HARDI2C
#include <Wire.h>
#include <Encoder.h>

#define LED_R 5                 //LED PWM
#define LED_B 6
#define LED_G 9
#define NFC_FD 2
#define PUSHB_1 4             //button inputs
#define PUSHB_2 7
#define ENC_PUSH A3


bool push1_old, push2_old;               // old values of the inputs, internally pulled up so = 1
bool push1_new, push2_new;
bool fd_old, fd_new;
bool encPush_old, encPush_new;

Ntag ntag(Ntag::NTAG_I2C_1K, 7, 9);
NtagSramAdapter ntagAdapter(&ntag);
auto timer = timer_create_default();
auto LEDtimer = timer_create_default();



byte serialOutputBuffer[100];
int bufferPointer = 0;
byte storedText[100];
bool isShowingEmail = false;


//debug

uint8_t rBaseLevel = 255;
uint8_t gBaseLevel = 50;
uint8_t bBaseLevel = 150;

float r = 255;
float g = 255;
float b = 255;

bool fallingEdge(bool old_value, bool new_value) {
  return (old_value == 1 && new_value == 0);
}

//probably can be replaced with something from peter's?
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

//function which receives  16 bytes at a time, parses for key characters | and 0xFE, and returns a pointer to the location of the final character
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
  return 0; //not yet finished
}

//function to set each eeprom byte to 0 between a start byte and a finish byte (inclusive)
void erase(int startByte = 0, int finishByte = 0x37 * 16, bool doPrint = false) {
  byte zeros[2 * 16];
  int startRemainder = (16 - startByte % 16) % 16;
  int endRemainder = finishByte % 16 + 1;

  //defines empty packet of data
  for (byte i = 0; i < 17; i++) {
    zeros[i] = 0x00;
  }

  if (startRemainder != 0) { //overwrite starting partial packet
    if (ntag.writeEeprom(startByte, zeros, startRemainder)) {
    }
  }
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


//function which receives parameters, reads from the eeprom and passes data to parseText. returns the address of the end character 0xFE
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

//function which when called checks if a new name has been written, if so displays the name, stores it in 0x30 in eeprom and purges NDEF record from 0x0...
void preserveName() {
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
    erase(0, 109);
    writeNameSerial();
  }
}

//function which reads email from 0x30 and shows it in NDEF form or alternatively purges NDEF email from 0x0 if it is already showing
bool toggleShowEmail(void *) {
  byte customNdefTextHeader[9] = {0x03, 0x00, 0xD1, 0x01, 0x00, 0x54, 0x02, 0x65, 0x6E};

  isShowingEmail = !isShowingEmail;
  readText(false, true, 0x30, true);
  customNdefTextHeader[1] = bufferPointer + 7; //calculated from length of text from NDEF text standard
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
  return false; //tells timer not to repeat
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(PUSHB_1, INPUT_PULLUP);
  pinMode(PUSHB_2, INPUT_PULLUP);
  pinMode(ENC_PUSH, INPUT_PULLUP);


  pinMode(LED_R, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(LED_G, OUTPUT);

  Serial.println("start");
  if (!ntag.begin()) {
    Serial.println("Can't find ntag");
  }
  preserveName();//deals with programming whilst turned off
}

void loop() {
  byte eepromdata[2 * 16];
  timer.tick();
  LEDtimer.tick();

  push1_new = digitalRead(PUSHB_1);
  push2_new = digitalRead(PUSHB_2);
  encPush_new = digitalRead(ENC_PUSH);

  fd_new = !digitalRead(NFC_FD); //active low
  if (fallingEdge(fd_old, fd_new) && (!isShowingEmail)) { //if nfc field removed i.e process finished and not showing email, update
    preserveName();
  }


  //CHANGE THIS TO APPROPRIATE BUTTON COMBO FOR NETWORKING MODE
  if (fallingEdge(push1_old, push1_new) && (!isShowingEmail)) { //if nfc field removed i.e process finished and not showing email, update
    timer.in(0, toggleShowEmail); //show email for 10 seconds
    timer.in(10000, toggleShowEmail);
  }





  push1_old = push1_new;
  push2_old = push2_new;
  encPush_old = encPush_new;
  fd_old = fd_new;

  delay(5);
}
