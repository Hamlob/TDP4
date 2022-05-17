#include <stdio.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define TIMER_INTERRUPT_DEBUG         2
#define _TIMERINTERRUPT_LOGLEVEL_     0
#define USE_TIMER_1     true
#include "TimerInterrupt.h"
#define TIMER1_INTERVAL_MS    5
#define TIMER1_FREQUENCY      (float) (1000.0f / TIMER1_INTERVAL_MS)

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

#define SENSE_BAT A0          //analog input to sense battery voltage

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

bool edit = 0;
uint8_t mode = 0;                                                   //variable keeping track of which mode the device is in; 0 for adjusting discpline, 1 for editing name
bool encPush_old, push1_old, push2_old, encA_old, encB_old;               // old values of the inputs, internally pulled up so = 1
bool encPush_new, push1_new, push2_new, encA_new, encB_new;
int buttonIncrement, encIncrement = 0;    //values storing direction of scrolling of letter/discipline given by inputs
const float t = 5;                         //time step of 10ms

unsigned long vib_start;
const uint8_t vib_duration = 100;
const uint8_t vib_intensity = 255;              //0-255 range
bool vibrating = false;

int count = 0;                              //counter for slower battery update
const float divider_ratio = 10/13.3;        //ratio between actual and measured value because of the voltage divider used 
uint8_t bat_new, bat_old;                   //variables holding the percentage of battery

#define CHARGE_AREA_START_X     20
#define CHARGE_AREA_START_Y     18
#define CHARGE_AREA_WIDTH       83
#define CHARGE_AREA_HEIGHT      28
#define BATTERY_FRAME_START_X   16
#define BATTERY_FRAME_START_Y   16
#define BATTERY_FRAME_WIDTH     88
#define BATTERY_FRAME_HEIGHT    32
#define BATTERY_TIP_START_X     105
#define BATTERY_TIP_START_Y     24
#define BATTERY_TIP_WIDTH       4
#define BATTERY_TIP_HEIGHT      16

//---------------SNAKE variables--------------------------------------------------

bool game_running = 0;

// define directions
#define DIRUP     1       // these values is what the "snake" looks at to decide-
#define DIRRIGHT  2
#define DIRDOWN   3       // the direction the snake will travel
#define DIRLEFT   4

// set button variables

// volitile cos we need it to update with the interupt so can be any bit of cycle value 
// is never higher than 4 so only need 8bit int to save resources
volatile uint8_t snake_dir = 0;     
 // snake ints
byte snakePosX[30]; // array to make body of snake
byte snakePosY[30];

int snakeX = 30;     // snake head position
int snakeY = 30;
int snakeSize = 1;   // snake size count limited to the size of the array

// world ints

uint8_t worldMinX = 0;        // these set the limits of the play area
uint8_t worldMaxX = SCREEN_WIDTH;
uint8_t worldMinY = 10;
uint8_t worldMaxY = SCREEN_HEIGHT -1;

// collect scran(food) and position of scran
bool scranAte = 0;
uint8_t scranPosX = 0;
uint8_t scranPosY = 0;

// scores variables
long playscore = 0;   
long highscore = 30;  // set high score to 3 collect as a starting point

//---------------SNAKE FUNCTIONS-----------------------------------------

//----------------------------read input----------------------------------------



//void TimerHandler()
//{
//  if (mode == 2){
//    encPush_new = digitalRead(ENC_PUSH);
//    encA_new = digitalRead(ENC_A);
//    
//    if (fallingEdge(encPush_old, encPush_new)){
//        mode = 0;   
//    }
//    
//    snake_dir += encRead();
//    dir_check();
//    encA_old = encA_new;
//    encPush_old = encPush_new;
//  }
//  else{
//    return;
//  }
//}

//-----------------------direction check-----------------------------

void dir_check(){
  if (snake_dir > 4){
      snake_dir = 1;
    }
    else if (snake_dir < 1){
      snake_dir = 4;
    }
}

//-------------------------- draw the display  routines-----------------------------------

void updateDisplay()  // draw scores and outlines  
    {
           // Serial.println("Update Display");

            display.fillRect(0,0, display.width()-1,8,BLACK);
            display.setTextSize(0);
            display.setTextColor(WHITE);
           
            // draw scores
            display.setCursor(2,1);
            display.print("Score:");
            display.print(String(playscore, DEC));
            display.setCursor(66,1);
            display.print("High:");
            display.print(String(highscore ,DEC));
            // draw play area
            //        pos  1x,1y, 2x,2y,colour
            display.drawLine(0,0,127,0,WHITE); // very top border
            display.drawLine(63,0,63,9,WHITE); // score seperator
            display.drawLine(0,9, 127,9,WHITE); // below text border
            display.drawLine(0,63,127,63,WHITE); // bottom border
            display.drawLine(0,0,0,63,WHITE); // left border
            display.drawLine(127,0,127,63,WHITE); //right border

        

    }
  
//----------------------------------- update play area ------------------------------



void updateGame()     // this updates the game area display
    {
        display.clearDisplay();

          display.drawPixel(scranPosX,scranPosY, WHITE);
          scranAte = scranFood();   

            // check snake routines

              if (outOfArea()||selfCollision())
                   {
                      gameOver();
                   }

            // display snake
              for(int i=0;i<snakeSize;i++)
                  {
                    display.drawPixel(snakePosX[i],snakePosY[i],WHITE);
                  }
              
               // remove end pixel as movement occurs
              for(int i=snakeSize;i>0;i--)
                    {
                      snakePosX[i] = snakePosX[i-1];
                      snakePosY[i] = snakePosY[i-1];
                    }
              // add a extra pixel to the snake
              if(scranAte)
                  {
                    snakeSize+=1;
                    snakePosX[snakeSize-1]=snakeX;
                    snakePosY[snakeSize-1]=snakeY;
                  }


            switch(snake_dir) // was snakeDirection
                  {
                    case DIRUP:
                        snakeY-=1;
                        break;
                    case DIRDOWN:
                        snakeY+=1;
                        break;
                    case DIRLEFT:
                        snakeX-=1;
                        break;
                    case DIRRIGHT:
                        snakeX+=1;
                        break;
                  }

        snakePosX[0] = snakeX;
        snakePosY[0] = snakeY;
        

          updateDisplay();
          display.display();
    }

  // --------------------- place the scran -------------------

void placeScran()
    {
      scranPosX=random(worldMinX+1,worldMaxX-1);
      scranPosY=random(worldMinY+1,worldMaxY-1);

       }
//------------------------ SCRAN ATE POINT UP ----------------
bool scranFood()
    {
          if(snakeX==scranPosX&&snakeY==scranPosY)
              {
                  playscore=playscore+10;

                   tone(HAP,2000,10);
                      updateDisplay();
                      placeScran();
                    return 1;
              }
            else
              {
                  return 0;
              }
    }
//--------------------- out of area----------------------

bool outOfArea()
    {
      return snakeX < worldMinX||snakeX >worldMaxX ||snakeY<worldMinY|| snakeY>worldMaxY;
    }
//---------------------- game over--------------------------

void gameOver()
   {
      uint8_t rectX1,rectY1,rectX2,rectY2;
     
      rectX1=38;
      rectY1=28;
      rectX2=58;
      rectY2=12;
      display.clearDisplay();
      display.setCursor(40,30);
      display.setTextSize(1);
      tone(HAP,2000,50);
      display.print("GAME ");
      tone(HAP,1000,50);
      display.print("OVER");

      if(playscore>=highscore)   //check to see if score higher than high score
          {  
            highscore=playscore;  //single if statment to update high score
          }

         
      for(int i=0;i<=16;i++)   // this is to draw rectanlges around game over
            {
              display.drawRect(rectX1,rectY1,rectX2,rectY2,WHITE);
              Serial.println("if loop");
              display.display();
                rectX1-=2;      // shift over by 2 pixels
                rectY1-=2;
                rectX2+=4;      // shift over 2 pixels from last point
                rectY2+=4;
                tone(HAP,i*200,3);
            }
       display.display();

      
                //Screen Wipe after fame over
                rectX1=0;   // set start position of line
                rectY1=0;
                rectX2=0;
                rectY2=63;

            for (int i =0;i<=127;i++)
                  {
                       uint8_t cnt=0;
                       display.drawLine(rectX1,rectY1,rectX2,rectY2,BLACK); 
                        rectX1++;
                        rectX2++;
                        display.display();                  
                     
                  }
        display.clearDisplay();
      playscore=0;        // reset snake and player details
      snakeSize=1;
      snakeX=display.width()/2;
      snakeY=display.height()/2;

       waitForPress();        // wait for player to start game

      }
//-------------------------wait for presss loop -------------------------
void waitForPress()
      {
        bool waiting=1;  // loop ends whjen this is true
        display.clearDisplay();
        while(waiting)
             
            {
                      encPush_new = digitalRead(ENC_PUSH);

                      drawALineForMe(WHITE); // draw a random white line
                      drawALineForMe(BLACK); // draw a random black line so that the screen not completely fill white
                      display.fillRect(19,20,90,32,BLACK);        // blank background for text
                      display.setTextColor(WHITE);
                      display.setCursor(35,25);
                      display.setTextSize(2); // bigger font
                      display.println("SNAKE");
                                      //    x  y   w  h r  col
                      display.drawRoundRect(33,22,62,20,4,WHITE);   // border Snake
                      display.drawRect(19,20,90,32,WHITE);          // border box  - 3
                      display.setCursor(25,42);
                      display.setTextSize(0);                       // font back to normal
                      display.println("press any key");
                      display.fillRect(0,0,127,8,BLACK);
                      display.setCursor(10,0);
                      display.print("High Score :");                // display the high score
                      display.print(highscore);
                      display.display();
                      waiting = digitalRead(PUSHB_1) && digitalRead(PUSHB_2);  // if any of the keys is pressed, its value goes to 0 and PUSHB1&&PUSHB2 will be 0 too
                      if (fallingEdge(encPush_old, encPush_new)){
                        mode = 0;
                        displayNameDisc();
                        waiting = 0;
                      }
                      snake_dir=0;        // reset button press to no direction

                      encPush_old = encPush_new;
                
            }
      }
//--------------------DRAW a random line input colour uint8_t-------------------
void drawALineForMe(uint8_t clr)
        {
                 uint8_t line1X,line1Y,line2X,line2Y=0;
                      // set random co-ordinates for a line then draw it     
                      //  variable         no less      no more
                          line1X = random(worldMinX+1,worldMaxX-1);
                          line1Y = random(worldMinY+1,worldMaxY-1);
                          line2X = random(worldMinX+1,worldMaxX-1);
                          line2Y = random(worldMinY+1,worldMaxY-1);
                       
                          display.drawLine(line1X,line1Y,line2X,line2Y,clr);
        }

//-------------------------------------  collision detecion -------------------------------
bool selfCollision()
    {
      for(byte i=4;i<snakeSize;i++)
          {   // see if snake X and Y match == snakePos X and Y return true 1 if so
              if (snakeX==snakePosX[i]&&snakeY==snakePosY[i])  
                    {
                      tone(HAP,2000,20);
                      tone(HAP,1000,20);
                      
                      return 1; 
                    }
          }
        return 0;
       }

//---------------FUNCTIONS---------------------------------------------------

void ledOut(uint8_t r, uint8_t g, uint8_t b){
  //input: value of how prominent each color should be (0-255)
  //output: PWM driving RGB leds; each color will be driven separately by PWM with duty cycle 0-100% depending on the char value (0->0%, 255->100%)

  analogWrite(LED_R, r);
  analogWrite(LED_G, g);
  analogWrite(LED_B, b);
  return;
}


void updateData(int discIndex){
  //according to the discIndex, this function generates values for R, G, B components of the color stores then in global color variable
  //it also updates the global disc variable which holds the discipline string

  switch(discIndex)
  {
    case 1:
        disc = &MECH[0];    //change the pointer to point at the first character of MECH array
        color[0] = 0;
        color[1] = 0;
        color[2] = 255;
        break;
    case 2:
        disc = &EEE[0];
        color[0] = 255;
        color[1] = 0;
        color[2] = 0;
        break;
    case 3:
        disc = &AER[0];
        color[0] = 0;
        color[1] = 255;
        color[2] = 0;
        break;
    case 4:
        disc = &CIV[0];
        color[0] = 0;
        color[1] = 255;
        color[2] = 255;
        break;
    case 5:
        disc = &PDE[0];
        color[0] = 255;
        color[1] = 0;
        color[2] = 255;
        break;
    default:
        color[0] = 255;
        color[1] = 255;
        color[2] = 255;
  }

  return;
}

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

void writeString(const char *text, uint8_t size =1){
  //displays name and discipline on the display, starting at the x, y cursor position
  //Characters that won't fit onto the display will be clipped.

  display.setTextSize(size);               // Normal 1:1 pixel scale
  
  for(int i=0; text[i]!='\0'; i++){
    display.write(text[i]);
  }

  return;
}

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

void vibrate(bool state){
  if (state){
    analogWrite(HAP, vib_intensity);
    vib_start = millis();
    vibrating = true;
  }
  else{
    analogWrite(HAP, 0);
    vibrating = false;
  }
}

void vibrate_twice(){
  analogWrite(HAP, vib_intensity);
  delay(vib_duration);
  analogWrite(HAP, 0);
  delay(vib_duration);
  analogWrite(HAP, vib_intensity);
  delay(vib_duration);
  analogWrite(HAP, 0);
}

void intensityAdjust(){
  if ((color[0] - 25)<0){
    color[0] = 0;
  }
  else if((color[0]+25)>255){
    color[0] = 255;
  }
  else{
    color[0] = color[0] + encIncrement * 25;
  }
  if ((color[1] - 25)<0){
    color[1] = 0;
  }
  else if((color[1]+25)>255){
    color[1] = 255;
  }
  else{
    color[1] = color[1] + encIncrement * 25;
  }
  if ((color[2] - 25)<0){
    color[2] = 0;
  }
  else if((color[2]+25)>255){
    color[2] = 255;
  }
  else{
  color[2] = color[2] + encIncrement * 25;
  }
}

bool fallingEdge(bool old_value, bool new_value){
  if(old_value == 1 && new_value == 0){
    return true;
  }
  return false;
}

int encRead(){
  if ((encA_old == 0 && encA_new == 1 && digitalRead(ENC_B) == 0) or (encA_old == 1 && encA_new == 0 && digitalRead(ENC_B) == 1)){
    return 1;
  }
  else if ((encA_old == 1 && encA_new == 0 && digitalRead(ENC_B) == 0) or (encA_old == 0 && encA_new == 1 && digitalRead(ENC_B) == 1)){
    return -1;
  }
  return 0;           //return 0 if no condition is met
}

uint8_t batPercent(uint16_t value){
  float analog_value = value*(3.3/1023)*(1/divider_ratio);                     //convert to original analog range
  uint8_t bat_percent;
  if (analog_value < 3.1){
    bat_percent = 1;
  }
  else if (analog_value < 3.6){                                          // 3.0-3.6 represents 0-10% range and is mostly linear
    bat_percent = 0.5 + (analog_value - 3.0)/(3.6-3.0) * 10;
  }
  else if (analog_value < 4.2){
    bat_percent = 0.5 + 4497.7*pow(analog_value,4) - 70268.1*pow(analog_value,3) + 411049.15*pow(analog_value,2) -1066885.1*(analog_value) + 1036586;   //interpolating function using python
  }
  else{
    bat_percent = 100;
  }
  return bat_percent;
}

void displayBat(uint8_t percentage){
  display.clearDisplay();
  display.drawRect(BATTERY_FRAME_START_X, BATTERY_FRAME_START_Y, BATTERY_FRAME_WIDTH, BATTERY_FRAME_HEIGHT, SSD1306_WHITE);
  display.fillRect(BATTERY_TIP_START_X, BATTERY_TIP_START_Y, BATTERY_TIP_WIDTH, BATTERY_TIP_HEIGHT, SSD1306_WHITE);
  uint8_t width;
  width = (percentage * CHARGE_AREA_WIDTH) / 100;
  display.fillRect(CHARGE_AREA_START_X, CHARGE_AREA_START_Y, width, CHARGE_AREA_HEIGHT, SSD1306_WHITE);
  display.setCursor(0,0);
  
  display.display();
}


//----------MAIN------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------
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
    pinMode(HAP, OUTPUT);


    pinMode(PUSHB_1, INPUT_PULLUP);        
    pinMode(PUSHB_2, INPUT_PULLUP);
    pinMode(ENC_PUSH, INPUT_PULLUP);
    pinMode(ENC_A, INPUT_PULLUP);
    pinMode(ENC_B, INPUT_PULLUP);
//  pinMode(SPI_MISO, INPUT);
//  pinMode(NFC_FD, INPUT);
    pinMode(SENSE_BAT, INPUT);
    
    Serial.begin(9600);
//
//    // Init timer ITimer1
//    ITimer1.init();
//
//    // Interval in unsigned long millisecs
//    if (ITimer1.attachInterruptInterval(TIMER1_INTERVAL_MS, TimerHandler))
//      Serial.println("Starting  ITimer OK, millis() = " + String(millis()));
//    else
//      Serial.println("Can't set ITimer. Select another freq. or timer");

    encPush_old = push1_old = push2_old = encA_old = 1;
    encPush_new = push1_new = push2_new = encA_new = 1;

    displaySetup();         //setups the display
    ledOut(color[0], color[1], color[2]);
    disc = &EEE[0];
    displayNameDisc();

    
}


void loop(){

    buttonIncrement = 0;
    
    //check for inputs
    encPush_new = digitalRead(ENC_PUSH);
    push1_new = digitalRead(PUSHB_1);
    push2_new = digitalRead(PUSHB_2);
    encA_new = digitalRead(ENC_A);

    
    if (fallingEdge(encPush_old, encPush_new)){
      mode += 1;
      vibrate_twice();
      if(mode==0){
        displayNameDisc();
      }
      else if (mode == 1){
        bat_new = batPercent(analogRead(SENSE_BAT));
        displayBat(bat_new);
      }
      else if (mode == 2){
        game_running = 0;
      }
      else {
        mode = 0;
        displayNameDisc();
      }
    }
    if (fallingEdge(push1_old, push1_new)){
      buttonIncrement += 1;
    }
    if (fallingEdge(push2_old, push2_new)){
      buttonIncrement -= 1;
    }
    
    encIncrement = encRead();

    if (vibrating and (millis()-vib_start) >= vib_duration){               //if its vibrating and it has been vibrating for at least 100ms
      vibrate(0);
    }

    if (encPush_new == 0 && push1_new == 0 && push2_new == 0){        // edit mode engaged by pressing all three buttons simultaneously
      edit = !edit;
      mode = 0;
      vibrate(1);
      delay(400);
      vibrate(0);
      if(edit){
        ledOut(0,0,0);
      }
      else{
        ledOut((intensity*color[0])/10, (intensity*color[1])/10, (intensity*color[2])/10);   //display the color on LED
      }
    }
      
      // mode for scrolling through the disciplines and colors
    if(mode == 0){
      if (buttonIncrement!=0 || encIncrement!=0){         //if there is an input

        //edit mode; LED OFF
        if (edit){                                        
          letterIndex += buttonIncrement;
          checkLetterIndex();
          row = letterIndex/maxlength;
          column = letterIndex%maxlength;
          username[row][column] += encIncrement;         
        }

        //display only mode; LED ON
        else{
          vibrate(1);                          
          discIndex +=buttonIncrement;
          checkIndex();                           //make sure the index is within the range of disciplines
          updateData(discIndex);                  //update the discipline and color accordingly
          intensity += encIncrement;
          checkIntensity();
          ledOut((intensity*color[0])/10, (intensity*color[1])/10, (intensity*color[2])/10);   //display the color on LED
        }
        displayNameDisc();                      //comment out for debugging
        //button_values();                      //used for debugging
      }
    }
  
    //mode for displaying battery
    else if (mode == 1){
      if (count >2000){
        bat_new = batPercent(analogRead(SENSE_BAT));
        if (bat_new != bat_old){
          displayBat(bat_new); 
        }
        else{
          bat_old = bat_new;
        }
      }
    }

    //snake mode
    else{
      if (game_running){
        updateGame();
        snake_dir += buttonIncrement;
        dir_check();
      }
      else{
        waitForPress();    // display the snake start up screen
        placeScran();  // place first bit of food
        game_running = 1;
      }
    }
  
    //update input values
    encPush_old = encPush_new;
    push1_old = push1_new;
    push2_old = push2_new;
    encA_old = encA_new;

    delay(t);
}

//only for debugging

//void button_values(){
//  display.clearDisplay();
//  char red[4], blue[4], green[4];
//  red[0] = color[0]/100 + '0';
//  red[1] = (color[0]%100)/10 +'0';
//  red[2] = color[0]%10 + '0';
//  green[0] = color[1]/100 + '0';
//  green[1] = (color[1]%100)/10 +'0';
//  green[2] = color[1]%10 + '0';
//  blue[0] = color[2]/100 + '0';
//  blue[1] = (color[2]%100)/10 +'0';
//  blue[2] = color[2]%10 + '0';
//  red[3]=green[3]=blue[3]='\0';
//  
//  
//  display.setCursor(0, 0);             
//  writeString(red,2);
//
//  display.setCursor(0,20);
//  writeString(green,2);
//  
//  display.setCursor(0,40);            
//  writeString(blue,2);
//  
//  display.display();  
//}