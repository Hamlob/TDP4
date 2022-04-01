
#include <ntag.h>
#include <ntagadapter.h>
#include <ntageepromadapter.h>
#include <ntagsramadapter.h>

#include "ntageepromadapter.h"
#define HARDI2C

Ntag ntag(Ntag::NTAG_I2C_1K,2,5);
NtagEepromAdapter ntagAdapter(&ntag);

void setup()
{
      ntagAdapter.begin();
      Serial.begin(9600);
}

void loop() {
      NdefMessage message = NdefMessage();
      message.addUriRecord("http://arduino.cc");

      if (ntagAdapter.write(message)) {
        Serial.println("Success. Try reading this tag with your phone.");
      } else {
        Serial.println("Write failed.");
        delay(500);

        
      }
  }
