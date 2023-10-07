/*
 * ACPCPE v2.0 - Amstrad CPC Printer Emulator
 * 
 * v1.0 - debounce button using an NE555 in bistable mode.
 * v2.0 - software debounce button. NE555 removed
 * 
 * Having an Arduino connected to the Amstrad CPC's printer port,
 * this program simply converts the parallel data coming from the 
 * printer port to hexadecimal values and sends them through the 
 * serial port of the Arduino to a listening PC.
 * 
 * This program also checks the status of a push-button used as 
 * Online/Offline button, and turns on/off an LED to indicate 
 * Online/Offline status.
 * 
 * Instead of an Arduino, this program uses Teensy 2.0, but any
 * Arduino can be used. It's just a matter of redefining the pins.
 * 
 * CPC's Printer Port (Parallel) => Teensy USB (Serial Hex data) => PC USB Serial Port
 * 
 * A provided Pyhton program will read the hexadecimal bytes received 
 * on the USB Serial port and translate them to generate different text files.
 * 
 */

/* ---------------------------LICENSE NOTICE-------------------------------- 
 *  MIT License
 *  
 *  Copyright (c) 2019 David Asta
 *  
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *  
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */


 /*
  * Amstrad CPC Printer Port
  * 
  * The CPC's printer port has only 7 bits of data, therefore can only print 128 
  * different characters instead of the 256 allowed by a 8-bit data port. 7-bit 
  * data port is enough for printing English 7-bit ASCII character sets, but 
  * doesn't allow to print bitmap graphics.
  * 
  * http://www.cpcwiki.eu/index.php/Connector:Printer_port
  * PIN 1 = /STROBE     PIN 2 = D0
  * PIN 3 = D1          PIN 4 = D2
  * PIN 5 = D3          PIN 6 = D4
  * PIN 7 = D5          PIN 8 = D6
  * PIN 11 = BUSY
  * PINS 9, 14, 16, 19 to 26, 28 and 33 = GND
  * PINS 10, 12, 13, 15, 17, 18, 27, 29, 30, 31, 32, 34, 35, 36 = Not Connected
  * 
  * /Strobe goes from high to low for 0.5 ms to indicate that the Data pins 
  * (D0..D6) are holding valid data.
  * 
  * The BUSY signal is set by the printer and instructs the Amstrad CPC to
  * wait until the printer can receive more data. Low when printer is ready 
  * to accept data
  * 
  * The communication with the Pyhton program works as follows:
  *   - At the press of the push-button:
  *       - the LED lights up to indicate that the "printer is Online
  *       - the Arduino sends the ESC/P command 17 (Select Printer) 
  *       - and starts listening from the Amstrad CPC Printer port.
  *   - At the second press of the push-button:
  *       - the LED turns off to indicate that the "printer is Offline
  *       - the Arduino sends the ESC/P command 19 (Deselect Printer) 
  *       - and stops to listen from the Amstrad CPC Printer port.
  *   - The Python program uses these two commands to open and close the files.
  *   
  *   - In other words (or How to print setep-by-step:
  *     - When ready to print press the push-button and ensure that the Online LED is ON.
  *     - Print from the Amstrad CPC (from BASIC, word processor, etc.).
  *     - Once printing is finished, press the push-button again. LED should go off.
  *     - The Python program running on the PC will dump all received bytes into file(s).
  *     
  */

/////////////////////////////////////////////////////////////////////////
// Teensy 2.0's digital pins usable for interrupts (5, 6, 7, 8)
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

#define COM_SPEED 115200 // Speed of the COM port between Arduino and PC

/////////////////////////////////////////////////////////////////////////
byte data = 0;          // Variable for converting parallel data (D0..D6) as a single byte
char buf[2];            // Variable for converting byte to hex using sprintf
bool wasOnline = false; // Variable for storing Online/Offline push-button state
unsigned long debounceDelay = 150; // delay time to avoid bounce from the push-button
unsigned long lastDebounce = 0;   // time since push-button was last pressed

/////////////////////////////////////////////////////////////////////////
void setup() {
  Serial.begin(COM_SPEED);

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
  attachInterrupt(digitalPinToInterrupt(PRN_STRB), readCPCbyte, LOW);
  pinMode(ONOFF_BTN, INPUT_PULLUP);
  //if the BTN status changes call btnPressed function
  attachInterrupt(digitalPinToInterrupt(ONOFF_BTN), btnPressed, CHANGE);
    
  //on this Teensy it is pin 13
//  digitalWrite(13, HIGH);   // turn the Teensy's internal LED on

//  Serial.println("Deselecting printer");
  Serial.print(25, HEX); // ESC 19 = Deselect printer
}

/////////////////////////////////////////////////////////////////////////
void readCPCbyte(){
  // This function is called when /Strobe is Low (detected by Arduino ISR)
//  Serial.println("\nStrobe is LOW so were sending data to USB port");

  // Receive Byte
  bitWrite(data, 0, digitalRead(PRN_D0));
  bitWrite(data, 1, digitalRead(PRN_D1));
  bitWrite(data, 2, digitalRead(PRN_D2));
  bitWrite(data, 3, digitalRead(PRN_D3));
  bitWrite(data, 4, digitalRead(PRN_D4));
  bitWrite(data, 5, digitalRead(PRN_D5));
  bitWrite(data, 6, digitalRead(PRN_D6));
  // print byte as hexadecimal value to the USB Serial port
  sprintf(buf, "%02x", data);
//  Serial.println("and here is what we sent");
  Serial.print(buf);
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
      }else{
        // Switch from Offline to Online
        wasOnline = true;
        Serial.print(23, HEX); // ESC 17 = Select printer
//        Serial.println("\nprinter online");
        digitalWrite(PRN_BUSY, LOW); // Printer is Online, tell the CPC that we're ready for data
        //in theory setting this should invoke the call to readCPCbyte?
      }
    }
    
    lastDebounce = millis(); // reset debounce timer
  }

  digitalWrite(ONOFF_LED, wasOnline);
//  digitalWrite(PRN_BUSY, !wasOnline);
}

/////////////////////////////////////////////////////////////////////////
void loop() {}
