/*
 * Remote MP3 sound player with an ESP32.
 * 
 * Receives weapon request for sound from nRF24L01+ radio and play selected sound on DFPlayer Mini MP3 Player module.
 * If the BUSY pin is LOW, the dfplayer is busy and vice versa.
 * Note:  after a song starts, BUSY pin takes 200ms to become active LOW.
 *
 * On a FAT32 SD card, under folder 01, files 001-009.mp3 are gun system sounds.
 * weapon sounds start from file 010.mp3...max.mp3. wav files can also be used.
 * audio format - stereo or mono 44.1Khz, 32bit float
 */

#include <Arduino.h>
#include "DFRobotDFPlayerMini.h"
#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"
#include "Button.h"

#define WEAPON_SELECT 22     //self-test weapon select button
#define CSPIN 5   //esp32 VSPI gpio# for
#define CEPIN 4   //use by nRF24L01+ radio
#define MOSI 23
#define MISO 19
#define SCK 18
#define TX 17     //Serial2 to dfplayer
#define RX 16
#define pinDfpBusy 36     //Arduino pin -> dfplayer BUSY pin 16 (low: playing / high: not playing)

RF24 radio(CEPIN,CSPIN);      //ce,cs
const uint64_t pipes[2] = { 0xABCDABCD71LL, 0x544d52687CLL };              // Radio pipe addresses for the 2 nodes to communicate.

//INTERNAL_PULLUP:  pin-----SW/ ----->0V  (no ext resistor needed)
ButtonCB btn1(WEAPON_SELECT, Button::INTERNAL_PULLUP);      //test button

DFRobotDFPlayerMini myDFPlayer1;       //general purpose sound module

byte weapon;
const byte SFX_RESERVED_0 = 0;           //reserved gun sounds
const byte SFX_POWERUP = 1;
const byte SFX_READY = 2;
const byte SFX_AMMO_EMPTY = 3;
const byte SFX_AMMO_RELOADED = 4;
const byte SFX_RESERVED_9 = 9;           //reserved gun sounds

const byte MAX_AMMO = 10;                //Nerf ammo limit
const byte MIN_WEAPON = 10;              //start of weapon sounds (01\010.mp3)
const byte MAX_WEAPON = 40;              //end of weapon sounds (01\038.mp3)
const byte DEFAULT_WEAPON = MIN_WEAPON;  //default weapon sound

void setup()
{
  Serial.begin(57600);     //serial monitor
  Serial2.begin(9600);     //dfplayer mini

  pinMode(pinDfpBusy, INPUT);   // init Arduino pin for checking DFPlayer BUSY
  printf_begin();
  Serial.println("");
  Serial.println(F("Remote MP3 Player - receives request from Nerf Laser Gun to play sound files."));

  if (!myDFPlayer1.begin(Serial2,true,false)) {  //Use softwareSerial to communicate with mp3. true ACK, false no RESET
    Serial.println(F("Unable to begin player 1:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));
    while(true){
      delay(0); // Code to compatible with ESP8266 watch dog.
    }
  }
  else {
    myDFPlayer1.volume(0x20);     //Set volume value. From 0 to 30
    myDFPlayer1.playFolder(1,SFX_AMMO_RELOADED);  //Play sound test on player1
  }
  
  Serial.println(F("DFPlayer ready."));
  
  radio.begin();
  radio.setDataRate(RF24_2MBPS);
  radio.setPALevel(RF24_PA_MIN);
  radio.setAutoAck(1);                    // Ensure autoACK is enabled
  radio.setPayloadSize(1);                // Here we are sending 1-byte payloads to test the call-response speed
  radio.setRetries(0,15);                 // Smallest time between retries, max no. of retries
  radio.openWritingPipe(pipes[1]);
  radio.openReadingPipe(1,pipes[0]);
  radio.startListening();
  if (radio.isChipConnected())
    Serial.println(F("Radio ready."));
  else
    Serial.println(F("Radio Chip not connected"));
  radio.printDetails();                   // Dump the configuration of the rf unit for debugging

  btn1.setClickHandler(onClick);    //define button callback
  weapon=MIN_WEAPON;              //default weapon
}

void onClick(const Button&) {
    if (weapon > MAX_WEAPON)
      weapon=MIN_WEAPON;          //start from min if > max
    myDFPlayer1.playFolder(1,weapon);  //Play all other sounds on player1
    Serial.print("weapon selected: ");
    Serial.println(weapon);
    weapon++;                     //cycle thru all weapons from min...max for testing
}

void loop()
{
   btn1.process();

  if (radio.available() ) {
    byte recvbuf;
    radio.read( &recvbuf, sizeof(recvbuf) );
    Serial.print("Received: ");
    Serial.println(recvbuf);
    myDFPlayer1.playFolder(1,recvbuf);     //Play sound on player1
    /*
    if (digitalRead(pinDfpBusy) == LOW) {    //LOW=sound playing (200ms delay after start)
      myDFPlayer1.stop();                    //stop it
    }
    else {
      myDFPlayer1.playFolder(1,recvbuf);     //Play sound on player1
    }
    */
    
    /* debug
    if (myDFPlayer1.available()) {
      printDetail(myDFPlayer1.readType(), myDFPlayer1.read()); //Print the detail message from DFPlayer to handle different errors and states.
    }
    */
  }

}

void printDetail(uint8_t type, int value){
  switch (type) {
    case TimeOut:
      Serial.println(F("Time Out!"));
      break;
    case WrongStack:
      Serial.println(F("Stack Wrong!"));
      break;
    case DFPlayerCardInserted:
      Serial.println(F("Card Inserted!"));
      break;
    case DFPlayerCardRemoved:
      Serial.println(F("Card Removed!"));
      break;
    case DFPlayerCardOnline:
      Serial.println(F("Card Online!"));
      break;
    case DFPlayerUSBInserted:
      Serial.println("USB Inserted!");
      break;
    case DFPlayerUSBRemoved:
      Serial.println("USB Removed!");
      break;
    case DFPlayerPlayFinished:
      Serial.print(F("Number:"));
      Serial.print(value);
      Serial.println(F(" Play Finished!"));
      break;
    case DFPlayerError:
      Serial.print(F("DFPlayerError:"));
      switch (value) {
        case Busy:
          Serial.println(F("Card not found"));
          break;
        case Sleeping:
          Serial.println(F("Sleeping"));
          break;
        case SerialWrongStack:
          Serial.println(F("Get Wrong Stack"));
          break;
        case CheckSumNotMatch:
          Serial.println(F("Check Sum Not Match"));
          break;
        case FileIndexOut:
          Serial.println(F("File Index Out of Bound"));
          break;
        case FileMismatch:
          Serial.println(F("Cannot Find File"));
          break;
        case Advertise:
          Serial.println(F("In Advertise"));
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }
  
}
