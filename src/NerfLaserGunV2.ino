 /*
Additional sound effects for Nerf Laser Ops Pro gun. Existing gun functionality are preserved so that a normal laser tag game
can be played. But for sound effects to work, the play area must be within 2.4Ghz coverage (like wifi).

Wire an Arduino Pro Mini to the gun switches and an nRF24L01+ radio transceiver. The switches will:
 -trigger laser gun sound fx at a remote MP3 player.
 -cycle thru a variety weapons for selection.
When the fire button is triggered, a weapon number is sent to a remote DFPlayer Mini which plays the corresponding
mp3/wav file.

When the reload button is pressed and held, gun goes into weapon select mode. Repeat firing the weapon to cycle thru all sounds.
If reload button is pressed momentarily, weapon reload occurs. The gun permits 10 shots before requiring a reload. The software
keeps track of the shot count so that it plays the AMMO_EMPTY gun sound when required, except during weapon select mode.

*/


#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"
#include "Button.h"

#define WEAPON_SELECT 5
#define TRIGGER 7
#define CSPIN 9
#define CEPIN 10
#define MOSI 11
#define MISO 12
#define SCK 13

// Hardware configuration: Set up nRF24L01 radio on SPI bus
RF24 radio(CEPIN,CSPIN);

//INTERNAL_PULLUP:  pin----SW/ ---->0V  (no ext resistor needed)
//PULL_DOWN:        pin----+----SW/ ---->Vcc
//                         |----vvvv---->0V  (10K pulldown resistor)

ButtonCB btn2(TRIGGER, Button::PULL_DOWN);          //gun trigger
Button btn3(WEAPON_SELECT, Button::PULL_DOWN);      //weapon select mode

byte triggerCount;
byte weapon;
byte ammo;

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

// Topology
const uint64_t pipes[2] = { 0xABCDABCD71LL, 0x544d52687CLL };              // Radio pipe addresses for the 2 nodes to communicate.

void setup(){
  Serial.begin(57600);
  printf_begin();
  Serial.println("");
  Serial.println(F("Nerf Laser Gun - fires laser and send request to remote MP3 player."));
  // Setup and configure rf radio

  radio.begin();                          //init nRF24L01+ radio
  radio.setDataRate(RF24_2MBPS);
  radio.setPALevel(RF24_PA_MAX);
  radio.setAutoAck(1);                    // Ensure autoACK is enabled
  radio.setPayloadSize(1);                // Here we are sending 1-byte payloads to test the call-response speed
  radio.setRetries(0,15);                 // Smallest time between retries, max no. of retries
  radio.openWritingPipe(pipes[0]);
  radio.openReadingPipe(1,pipes[1]);

  if (radio.isChipConnected())
    Serial.println(F("Radio ready."));
  else
    Serial.println(F("Radio Chip not connected"));
  radio.printDetails();                   // Dump the configuration of the rf unit for debugging

  btn2.setPressHandler(onPressHoldFB);    //define button callback
  btn2.setHoldHandler(onPressHoldFB);
  btn2.setHoldRepeats(true);
  btn2.setHoldThreshold(500);

  btn3.setHoldThreshold(500);

  triggerCount=0;
  weapon=DEFAULT_WEAPON;          //default weapon sound
  ammo=0;                         //ammo count
  Serial.println("Powerup");
  sfxRequest(SFX_POWERUP);        //play sound on remote NP3 player

}

void sfxRequest(byte soundfileNumber) {
  radio.stopListening(); 
  byte soundfile=soundfileNumber;    //play 00n.mp3 by remote MP3 player
  if (!radio.write( &soundfile, sizeof(soundfile) )) {     //tell DFPlayer Mini to play this
    Serial.print(F("Radio write failed: "));
    Serial.println(soundfile);
  }
}

void onPressHoldFB(const Button&) {
  if (triggerCount == 0) {     //for 1st trigger pull only
    Serial.println("Gun ready");
    sfxRequest(SFX_READY);     //play sound on remote NP3 player
    triggerCount++;
  }
  else {
    Serial.println("Fire button");
    if (ammo == MAX_AMMO && !btn3.isDown()) {   //if in weapon select mode, do not play the AMMO_EMPTY sound.
      sfxRequest(SFX_AMMO_EMPTY);           //send weapon request to remote MP3 player
      Serial.println("AMMO_EMPTY");
    }
    else {
      if (btn3.isDown()) {            //weapon select button held down?
        Serial.println("Weapon select mode");
        weapon++;                     //cycle thru all weapons from min...max
        if (weapon > MAX_WEAPON)
          weapon=MIN_WEAPON;          //start from min if > max
      }
      sfxRequest(weapon);             //send weapon request to remote MP3 player
      Serial.print("Weapon: ");
      Serial.println(weapon);
      if (ammo < MAX_AMMO) {
        ammo++;                       //keep track of ammo count even during weapon seelct mode.
        Serial.print("Ammo used=");
        Serial.println(ammo);
      }
    }
  }
}

void loop(void) {
  btn2.process();
  btn3.process();
  if (btn3.press()) {
    sfxRequest(SFX_AMMO_RELOADED);           //send weapon request to remote MP3 player
    ammo=0;
    Serial.print("Ammo reloaded, available=");
    Serial.println(MAX_AMMO);
  }
}
