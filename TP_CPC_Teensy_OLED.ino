#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <string.h>

#define i2c_Address 0x3c //initialize with the I2C addr 0x3C Typically eBay OLED's
//#define i2c_Address 0x3d //initialize with the I2C addr 0x3D Typically Adafruit OLED's

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1   //   QT-PY / XIAO
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define SCK      19  //A5
#define SDA      18  //A4

/////////////////////////////////////////////////////////////////////////
// Teensy 2.0's digital pins usable for interrupts (5, 6, 7, 8)
// Teensy 3.0 & 4.0 - all pins can be used for interrupts
#define PRN_STRB  5   // /Strobe
#define ONOFF_BTN 6   // Online/Offline signal through push-button

#define PRN_D0    0   // Data 0
#define PRN_D1    1   // Data 1
#define PRN_D2    2   // Data 2
#define PRN_D3    3   // Data 3
#define PRN_D4    4   // Data 4
#define PRN_D5    7   // Data 5
#define PRN_D6    8   // Data 6

#define PRN_BUSY  9   // Busy = HIGH / Ready = LOW
#define ONOFF_LED 10  // Indicator of "printer" Online/Offline

#define COM_SPEED 9600 // Speed of the COM port between Arduino and PC

/////////////////////////////////////////////////////////////////////////
byte data = 0;          // Variable for converting parallel data (D0..D6) as a single byte
char buf[2];            // Variable for converting byte to hex using sprintf
char buff[1200];        // used to output to OLED
bool wasOnline = false; // Variable for storing Online/Offline push-button state
unsigned long debounceDelay = 150; // delay time to avoid bounce from the push-button
unsigned long lastDebounce = 0;   // time since push-button was last pressed

/////////////////////////////////////////////////////////////////////////
void setup() {
//  Serial.begin(COM_SPEED);//having this early may have caused the /strobe issue?

//  Serial.println("Setting up printer comms");
//  Serial.println("telling PIN 9 CPC were busy");
  pinMode(PRN_BUSY, OUTPUT);
  digitalWrite(PRN_BUSY, 1); // Tell CPC that we're busy setting up everything
  
//  Serial.println("Turn LED OFF");
  pinMode(ONOFF_LED, OUTPUT);
  digitalWrite(ONOFF_LED, LOW);
  
//  Serial.println("set all pins to INPUT");
  pinMode(PRN_D0, INPUT);
  pinMode(PRN_D1, INPUT);
  pinMode(PRN_D2, INPUT);
  pinMode(PRN_D3, INPUT);
  pinMode(PRN_D4, INPUT);
  pinMode(PRN_D5, INPUT);
  pinMode(PRN_D6, INPUT);

//  Serial.println("Pulling Strobe and Button up");
  pinMode(PRN_STRB, INPUT_PULLUP);
  //if Strobe goes LOW then call the readCPCbyte function
  attachInterrupt(digitalPinToInterrupt(PRN_STRB), readCPCbyte, CHANGE); //CHANGE); //LOW);
  pinMode(ONOFF_BTN, INPUT_PULLUP);
  //if the BTN status changes call btnPressed function
  attachInterrupt(digitalPinToInterrupt(ONOFF_BTN), btnPressed, CHANGE);
    
  //on this Teensy it is pin 13
  digitalWrite(13, HIGH);   // turn the Teensy's internal LED on

  Serial.begin(COM_SPEED);
//  Serial.println("Deselecting printer");
  Serial.print(25, HEX); // ESC 19 = Deselect printer

  pinMode(SDA, OUTPUT);
  pinMode(SCK, OUTPUT);

  delay(250); // wait for the OLED to power up
  display.begin(i2c_Address, true); // Address 0x3C default
 //display.setContrast (0); // dim display
 //comment the following line doesn't show the adafruit splash screen
  display.display();
  delay(1000);
//  Wire.setClock(800000L); //set the I2C bus frequency to 800Mhz - faster refresh!
  display.clearDisplay(); // Clear the display buffer
}

/////////////////////////////////////////////////////////////////////////
void readCPCbyte(){
  // This function is called when /Strobe is Low (detected by Arduino ISR)
//  Serial.println("\nStrobe is LOW so were sending data to USB port");

  if(digitalRead(PRN_STRB) == LOW)
  {
//  Serial.print("\ninvoked readCPCbyte=");
    data = 0;
    // Receive Byte
    bitWrite(data, 0, digitalRead(PRN_D0));
    bitWrite(data, 1, digitalRead(PRN_D1));
    bitWrite(data, 2, digitalRead(PRN_D2));
    bitWrite(data, 3, digitalRead(PRN_D3));
    bitWrite(data, 4, digitalRead(PRN_D4));
    bitWrite(data, 5, digitalRead(PRN_D5));
    bitWrite(data, 6, digitalRead(PRN_D6));
    // print byte as hexadecimal value to the USB Serial port
    sprintf(buf, "%02x", data);  //this copies value into buf
    Serial.print(buf);           //this outputs the buf value
    //also output to the OLED screen
    //need to put the code in place to cater for only outputting 21 chars per line
    char result[5];
    hextostr(buf,result);
    //Serial.println(result);
    strcat(buff, result);
    //Serial.write(char(data));

  }
}

void hextostr(char *hex, char *result)
{
  char temp[3];
  int index = 0;
  temp[2] = '\0';
  while (hex[index]) {
        strncpy(temp, &hex[index], 2);
        //*result = (char) atoi(temp);
        *result = (char)strtol(temp, NULL, 16);
        result++;
        index += 2;
  }
  *result = '\0';
}

void displayoutput(){
//contains the LAST hex character, so it does have something
//just need to find a way to copy into a char that is longer than 2 HEX chars, but it does work!
//Serial.print(buff);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.setTextColor(SH110X_WHITE);
  display.print(buff);//68656c6c6f0d0a  
  //probably should convert from HEX to DEC and output?!
  display.display();
}

/////////////////////////////////////////////////////////////////////////
void btnPressed(){
//Serial.print("\nBUSY= "); Serial.print(digitalRead(PRN_BUSY));
//Serial.print("\nSTRB= "); Serial.print(digitalRead(PRN_STRB));
//Serial.print("\nBTN = "); Serial.print(digitalRead(ONOFF_BTN));
//Serial.print("\nDATA= ");
//Serial.print(digitalRead(PRN_D0));
//Serial.print(digitalRead(PRN_D1));
//Serial.print(digitalRead(PRN_D2));
//Serial.print(digitalRead(PRN_D3));
//Serial.print(digitalRead(PRN_D4));
//Serial.print(digitalRead(PRN_D5));
//Serial.println(digitalRead(PRN_D6));

//  Serial.println("\nbutton pressed");
  // This function is called each time Online/Offline is Low (detected by Arduino ISR)
  digitalWrite(PRN_BUSY, HIGH); // Printer is Offline, tell the CPC that can't send data

  if((millis() - lastDebounce) > debounceDelay){
    // timer is been stable for long enough time to consider it not a bounce

    if(digitalRead(ONOFF_BTN) == LOW){
      if(wasOnline){
        // Switch from Online to Offline
        wasOnline = false;
        Serial.print(25, HEX); // ESC 19 = Deselect printer
//        Serial.println("\nprinter offline");
        digitalWrite(PRN_BUSY, HIGH);
        digitalWrite(13, HIGH);   // turn the Teensy's internal LED on
        //output text to display
        displayoutput();
      }else{
        // Switch from Offline to Online
        wasOnline = true;
        Serial.print(23, HEX); // ESC 17 = Select printer
        digitalWrite(13, LOW);   // turn the Teensy's internal LED on
        display.clearDisplay(); //clear buffer
        display.display();      //clear splash screen
//        Serial.println("\nprinter online");
//        digitalWrite(PRN_BUSY, LOW); // Printer is Online, tell the CPC that we're ready for data
      }
    }
    
    lastDebounce = millis(); // reset debounce timer
  }

  digitalWrite(ONOFF_LED, wasOnline);
  digitalWrite(PRN_BUSY, !wasOnline);
}

/////////////////////////////////////////////////////////////////////////
void loop() {}
//all shall work as expected now
//changed from LOW to CHANGE and detect == LOW
//17
//print#8,"h"
//680d0a
//print#8,"hello"
//68656c6c6f0d0a
//19
