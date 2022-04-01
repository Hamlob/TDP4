#include <stdio.h>
#include <SPI.h>
#include <Wire.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Encoder.h>

#include <ntag.h>
#include <ntagadapter.h>
#include <ntageepromadapter.h>
#include <ntagsramadapter.h>

#include "ntageepromadapter.h"
#define HARDI2C



// defining pins and creating display + encoder objects
#define SCREEN_WIDTH 128        // OLED display width, in pixels
#define SCREEN_HEIGHT 64        // OLED display height, in pixels

#define OLED_RESET  8
//#define OLED_DC     6           //SPI interface pins + display object creation with default MISO, MOSI, CLK pins (see docs for bit-bang SPI)
//#define OLED_CS     10
//Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
//  &SPI, OLED_DC, OLED_RESET, OLED_CS);

#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32(0x3D doesnt work)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Ntag ntag(Ntag::NTAG_I2C_1K,2,5);
NtagEepromAdapter ntagAdapter(&ntag);

//#define UART_CTS RESET        //uart header
//#define UART_RX 0
//#define UART_TX 1

//#define I2C_SDA A4            //NFC header
//#define I2C_SCL A5
//#define NFC_FD 2

#define LED_R 5                 //LED PWM
#define LED_B 6
#define LED_G 9       
#define HAP 3                 //haptic motor PWM

//#define SENSE_BAT A0          //analog input to sense battery voltage

#define PUSHB_1 4             //button inputs
#define PUSHB_2 7
#define ENC_PUSH A3

#define ENC_A A2                //encoder inputs (dig)
#define ENC_B A1
//Encoder myEnc(ENC_A, ENC_B);    //creating encoder object, this also initializes pins ENC_A, ENC_B as digital inputs (exernal interrupts)


//constants and variables

const char MECH[] = "Mechanical";
const char EEE[] = "Electronics and Electrical";
const char AER[] = "Aeronautical";
const char CIV[] = "Civil";
const char PDE[] = "Product Design";
const char ENG[] = " Engineering";          //ENG will be appended at the eng of each discipline to save memory


const char *disc= &ENG[0];                  // not-constant pointer to a constant discipline string (to its first character)
const uint8_t nDisc = 5;                    //total number of disciplines
int discIndex = 1;                          //discipline ID 
uint8_t color[3] = {55, 55, 55};         //char array holding {R,G,B} components of the color respectively
uint8_t intensity = 10;                  //intensity ranging from 1-10

int letterIndex = 0;                                                  //index keeping track of which letter is currently being edited
uint8_t row, column = 0;
const uint8_t maxlength = 10;                                             //maximum length of the firstname or lastname
char username[2][maxlength + 1] = {"__________","__________"};       //column length must account for string end character

bool mode = 0;                                                   //variable keeping track of which mode the device is in; 0 for adjusting discpline, 1 for editing name
bool encPush_old, push1_old, push2_old, encA_old, encB_old;               // old values of the inputs, internally pulled up so = 1
bool encPush_new, push1_new, push2_new, encA_new, encB_new;
int buttonIncrement, encIncrement = 0;    //values storing direction of scrolling of letter/discipline given by inputs
const float t = 5;                         //time step of 10ms

/*


void checkIndex(){
  //updates discIndex value and makes sure the value is in range [1,nDisc] by connecting the first and last term in circular pattern
  //in case the discIndex value is about to go out of bounds of the range, it will simply start from the other side of the range

  if (discIndex > nDisc)                        //check if discIndex exceeded the nDisc range, if it did count from the start using % operator
  {
    discIndex = discIndex % nDisc;          
  }
  if (discIndex < 1)                            //check if discIndex is lower than the 1, if it is, count from the end of the nDisc range; see below
  {
    discIndex = nDisc + (discIndex % nDisc);    //plus sign because (discIndex % nDisc) is negative
  }
    
}

void checkLetterIndex(){
  //updates LetterIndex value so that it is in range defined by maxlength of the name

  if (letterIndex > 2*maxlength-1){
    letterIndex = 0;
  }
  else if (letterIndex < 0){
    letterIndex = 2*maxlength-1;
  }
}

void checkIntensity(){
  if (intensity>10){
    intensity = 10;
  }
  else if (intensity <1){
    intensity = 1;
  }
}
*/
void writeString(const char *text, uint8_t size =1){
  //displays name and discipline on the display, starting at the x, y cursor position
  //Characters that won't fit onto the display will be clipped.

  display.setTextSize(size);               // Normal 1:1 pixel scale
  
  for(int i=0; text[i]!='\0'; i++){
    display.write(text[i]);
  }

  return;
}
/*
void displayNameDisc(){ 
  display.clearDisplay();
  
  display.setCursor(0, 0);              
  writeString(username[0],2);
  display.setCursor(0,20);
  writeString(username[1],2);
  
  display.setCursor(0,40);
  writeString(disc);
  writeString(ENG);
  
  display.display();  
}
*/

void displaySetup(){
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.clearDisplay();  // Clear the buffer
  display.cp437(true);                  // Use full 256 char 'Code Page 437' font
  
  display.drawPixel(10, 10, SSD1306_WHITE); // Draw a single pixel in white
  display.setTextColor(SSD1306_WHITE);  // Draw white text
  display.setTextSize(1);
  display.display();      //updates the screen
  delay(2000);

  display.clearDisplay();
}

/*
void vibrate(){
  analogWrite(HAP, 255);
  delay(100);
  analogWrite(HAP,0);
  delay(100);
}*/



int encRead(){
  if ((encA_old == 0 && encA_new == 1 && digitalRead(ENC_B) == 0) or (encA_old == 1 && encA_new == 0 && digitalRead(ENC_B) == 1)){
    return 1;
  }
  else if ((encA_old == 1 && encA_new == 0 && digitalRead(ENC_B) == 0) or (encA_old == 0 && encA_new == 1 && digitalRead(ENC_B) == 1)){
    return -1;
  }
  return 0;           //return 0 if no condition is met
}


void setup(){
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
    
    Serial.begin(9600);

    displaySetup();         //setups the display
    ledOut(color[0], color[1], color[2]);
    disc = &EEE[0];
    displayNameDisc();
ntagAdapter.begin();
}


void loop(){

    buttonIncrement = 0;
    
    //check for inputs
    encPush_new = digitalRead(ENC_PUSH);
    push1_new = digitalRead(PUSHB_1);
    push2_new = digitalRead(PUSHB_2);
    encA_new = digitalRead(ENC_A);

    
    if (fallingEdge(encPush_old, encPush_new)){
      
     NdefMessage message = NdefMessage();
      message.addUriRecord("http://arduino.cc");
    display.clearDisplay();
    display.setCursor(0, 0);              
    
   
      if (ntagAdapter.write(message)) {
        writeString("success",2);
      } else {
        writeString("fail",2);
      }
      display.display();
      
      /*mode = !mode;
      if(mode){
        ledOut(0,0,0);
      }
      else{
        ledOut((intensity*color[0])/10, (intensity*color[1])/10, (intensity*color[2])/10);   //display the color on LED
      }*/
    }
    if (fallingEdge(push1_old, push1_new)){
      buttonIncrement += 1;
    }
    if (fallingEdge(push2_old, push2_new)){
      buttonIncrement -= 1;
    }
    
    encIncrement = encRead();

    if (buttonIncrement!=0 || encIncrement!=0){         //if there is an input
      
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
  
      displayNameDisc();                      //comment out for debugging
      //button_values();                      //used for debugging
    }

    //update input values
    encPush_old = encPush_new;
    push1_old = push1_new;
    push2_old = push2_new;
    encA_old = encA_new;
    encB_old = encB_new;

    delay(t);
}

void button_values(){
  display.clearDisplay();
  char red[4], blue[4], green[4];
  red[0] = color[0]/100 + '0';
  red[1] = (color[0]%100)/10 +'0';
  red[2] = color[0]%10 + '0';
  green[0] = color[1]/100 + '0';
  green[1] = (color[1]%100)/10 +'0';
  green[2] = color[1]%10 + '0';
  blue[0] = color[2]/100 + '0';
  blue[1] = (color[2]%100)/10 +'0';
  blue[2] = color[2]%10 + '0';
  red[3]=green[3]=blue[3]='\0';
  
  
  display.setCursor(0, 0);             
  writeString(red,2);

  display.setCursor(0,20);
  writeString(green,2);
  
  display.setCursor(0,40);            
  writeString(blue,2);
  
  display.display();  
}
