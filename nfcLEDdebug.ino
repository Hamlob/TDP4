#include <stdio.h>
#include <Wire.h>

#include <Encoder.h>

#include <ntag.h>
#include <ntagadapter.h>
#include <ntageepromadapter.h>
#include <ntagsramadapter.h>

#include "ntageepromadapter.h"
#define HARDI2C

//Ntag ntag(Ntag::NTAG_I2C_1K,2,5);

Ntag ntag(Ntag::NTAG_I2C_1K,7,9);
NtagEepromAdapter ntagAdapter(&ntag);

//#define I2C_SDA A4            //NFC header
//#define I2C_SCL A5
//#define NFC_FD 2

#define LED_R 5                 //LED PWM
#define LED_B 6
#define LED_G 9       
#define HAP 3                 //haptic motor PWM


#define PUSHB_1 4             //button inputs
#define PUSHB_2 7
#define ENC_PUSH A3

#define ENC_A A2                //encoder inputs (dig)
#define ENC_B A1

//uint8_t color[3] = {55, 55, 55};         //char array holding {R,G,B} components of the color respectively
//uint8_t intensity = 10;                  //intensity ranging from 1-10
int inByte = 0;
bool encPush_old, push1_old, push2_old, encA_old, encB_old;               // old values of the inputs, internally pulled up so = 1
bool encPush_new, push1_new, push2_new, encA_new, encB_new;
int buttonIncrement, encIncrement = 0;    //values storing direction of scrolling of letter/discipline given by inputs
const float t = 5;                         //time step of 10ms

int encRead(){
  if ((encA_old == 0 && encA_new == 1 && digitalRead(ENC_B) == 0) or (encA_old == 1 && encA_new == 0 && digitalRead(ENC_B) == 1)){
    return 1;
  }
  else if ((encA_old == 1 && encA_new == 0 && digitalRead(ENC_B) == 0) or (encA_old == 0 && encA_new == 1 && digitalRead(ENC_B) == 1)){
    return -1;
  }
  return 0;           //return 0 if no condition is met
}

bool fallingEdge(bool old_value, bool new_value){
  if(old_value == 1 && new_value == 0){
    return true;
  }
  return false;
}


void ledOut(uint8_t r, uint8_t g, uint8_t b){
  //input: value of how prominent each color should be (0-255)
  //output: PWM driving RGB leds; each color will be driven separately by PWM with duty cycle 0-100% depending on the char value (0->0%, 255->100%)

  analogWrite(LED_R, r);
  analogWrite(LED_G, g);
  analogWrite(LED_B, b);
  return;
}
void setup(){
  Serial.begin(9600);
   while (!Serial) {
  }

// initializes the pins

//  pinMode(UART_CTS, OUTPUT);
//  pinMode(UART_RX, OUTPUT);
//  pinMode(UART_TX, OUTPUT);
//  pinMode(SPI_SS, OUTPUT);
//  pinMode(SPI_MOSI, OUTPUT);
//  pinMode(SPI_SCK, OUTPUT);
//  pinMode(OLED_RST, OUTPUT);
//  pinMode(I2C_SDA, OUTPUT);
//  pinMode(I2C_SCL, OUTPUT);
    pinMode(LED_R, OUTPUT);
    pinMode(LED_B, OUTPUT);
    pinMode(LED_G, OUTPUT);
    //pinMode(HAP, OUTPUT);


    pinMode(PUSHB_1, INPUT_PULLUP);        
    pinMode(PUSHB_2, INPUT_PULLUP);
    pinMode(ENC_PUSH, INPUT_PULLUP);
    pinMode(ENC_A, INPUT_PULLUP);
    pinMode(ENC_B, INPUT_PULLUP);
//  pinMode(SPI_MISO, INPUT);
//  pinMode(NFC_FD, INPUT);
//  pinMode(SENSE_BAT, INPUT);
   
    encPush_old = push1_old = push2_old = encA_old = 1;
    encPush_new = push1_new = push2_new = encA_new = 1;

ledOut(255,255,255);

  //ntagAdapter.begin();

}

void loop(){
    /*if (Serial.available() > 0) {
      inByte = Serial.read();
      Serial.print(char(inByte));
      //Serial.println("responding");
    }*/

    buttonIncrement = 0;
    encIncrement = 0;
    //check for inputs
    encPush_new = digitalRead(ENC_PUSH);
    push1_new = digitalRead(PUSHB_1);
    push2_new = digitalRead(PUSHB_2);
    encA_new = digitalRead(ENC_A);

    
    if (fallingEdge(encPush_old, encPush_new)){
      encIncrement = 1;
    }
    if (fallingEdge(push1_old, push1_new)){
      buttonIncrement = 1;
    }
    if (fallingEdge(push2_old, push2_new)){
      buttonIncrement = 1;
    }
    
    //encIncrement = encRead();


    if (buttonIncrement!=0 || encIncrement!=0){         //if there is an input
      ntagAdapter.begin();
      ledOut(0, 255, 0);
      Serial.println("initiating write");
      delay(2000);
      
      NdefMessage message = NdefMessage();
      message.addUriRecord("http://arduino.cc");
      
      if (ntagAdapter.write(message)) {
        ledOut(0,0,255);
        Serial.println("success");
      } else {
        ledOut(255, 0, 0);
        Serial.println("failed");
      }
      delay(2000);
      ledOut(255,255,255);
      
      /*
      // mode for scrolling through the disciplines and colors
      if(mode == 0){                          
        discIndex +=buttonIncrement;
        checkIndex();                           //make sure the index is within the range of disciplines
        updateData(discIndex);                  //update the discipline and color accordingly
        //ledOut(color[0], color[1], color[2]);
        intensity += encIncrement;
        checkIntensity();
        ledOut((intensity*color[0])/10, (intensity*color[1])/10, (intensity*color[2])/10);   //display the color on LED
      }*/
  
      /*//mode for editing the name
      else{
        letterIndex += buttonIncrement;
        checkLetterIndex();
        row = letterIndex/maxlength;
        column = letterIndex%maxlength;
        username[row][column] += encIncrement; 
      }*/
    }

    //update input values
    encPush_old = encPush_new;
    push1_old = push1_new;
    push2_old = push2_new;
    encA_old = encA_new;
    encB_old = encB_new;

    delay(t);
}
