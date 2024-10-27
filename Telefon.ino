/*
  Telefon für Reparaturcafe/Stadtmuseum Leinfelden

  Wählescheibentelefon wird über TAE-Buchse an Analog A0 angeschlossen (über einen 470Ohm Widerstand).
  Arduino Nano zählt die Impulse und spielt je nach gewählter Nummer einen Text über den Ohrhörer des Telefons.
  Digital-Pins 1 bis 5 zeigen den Zustand der Zustandsmaschine an.
  Bleibt hängen, bis der MP3-Spieler funktioniert (SD-Karte muss gesteckt sein)

  (C) 2024 Achim Haag

  Arduino Nano, ATmega 328p (old bootloader)
  Sketch uses 6468 bytes (21%) of program storage space. Maximum is 30720 bytes.
  Global variables use 395 bytes (19%) of dynamic memory, leaving 1653 bytes for local variables. Maximum is 2048 bytes.  

  Enable USE_DISPLAY in display.ino for Adafruit display --> there is not enough memory available and programm will CRASH :-{
*/


#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"


SoftwareSerial playerserial(8, 9); // RX, TX
DFRobotDFPlayerMini player;

void printDetail(uint8_t type, int value);


#define PIN_HUNGUP 2
#define PIN_LIFTED 3
#define PIN_WINDINGUP 4
#define PIN_PULSING 5
#define PIN_PLAYING 6

void clear_pins(void) {
  digitalWrite(PIN_HUNGUP, LOW);
  digitalWrite(PIN_LIFTED, LOW);
  digitalWrite(PIN_WINDINGUP, LOW);
  digitalWrite(PIN_PULSING, LOW);
  digitalWrite(PIN_PLAYING, LOW);
}

void setup() {
  // Debug and MP3-player serial interfaces:
  Serial.begin(115200);
  playerserial.begin(9600);

  // debug pins for state machine:
  pinMode(PIN_HUNGUP, OUTPUT);
  pinMode(PIN_LIFTED, OUTPUT);
  pinMode(PIN_WINDINGUP, OUTPUT);
  pinMode(PIN_PULSING, OUTPUT);
  pinMode(PIN_PLAYING, OUTPUT);
  clear_pins();

  // initialize MP3 player. STALLS IF NOT SUCCESSFUL:
  uint8_t response = player.begin(playerserial, true, false);
  do {
    player.reset();
    player.waitAvailable(2000);
    delay(200);
    response = player.readType();
    printDetail(response, player.read());
  }
  while (response != DFPlayerCardOnline);

  Serial.println(F("DFPlayer Mini online."));
  player.setTimeOut(200); // timeout in ms
  player.volume(25);  //Set volume value (0~30).
  printDetail(player.readType(), player.read()); // Print error messages from DFPlayer

//  init_display();
}


enum STATES_PHONE {
  STATE_START=0, STATE_HUNGUP=100, STATE_LIFTED=200,  STATE_WINDINGUP=300, STATE_PULSE_HIGH=400, STATE_PULSE_LOW=450, STATE_PLAY=500};
enum STATES_PHONE state = STATE_START;
enum STATES_PHONE nextstate = STATE_START;
bool initstate = false;

#define DIALTONE 1000

int line_raw;   // ADC value of phone line
int line;       // filtered value of phone line

#define HUNGUP_DELAY 10 // wait in ms
#define HUNGUP_WAIT 10  // wait 10*10 ms before leaving
#define LIFTED_DELAY 10 
#define LIFTED_WAIT 10  // 100ms
#define WINDINGUP_DELAY 5
#define WINDINGUP_WAIT 5  // 25ms
#define PULSE_HIGH_DELAY 4
#define PULSE_HIGH_WAIT 5  // 20ms
#define PULSE_HIGH_TIMEOUT 50 // 200ms timeout for hanging up while pulsing
#define PULSE_LOW_DELAY 4
#define PULSE_LOW_WAIT_HIGH 6  // 24ms
#define PULSE_LOW_WAIT_MIDDLE 5  // 20ms
#define PLAY_WAIT 5
#define PLAY_DELAY 6    // 30ms

#define LEVEL_HIGH 800      // distinguish between hung up (ca. 1020 - 1023) and lifted off (ca. 450 - 630)
#define LEVEL_LOW 300       // distinguish between lifted off (see above) and dial-impulse (ca. 114 -116)

int counter_change; // general purpose for counting, if a change should be changed
int counter_midlevel; // for detecting when dialing is finished
int counter_highlevel;  // for detecting timeout, when user hung up during pulsing
int number;     // the number that is beeing dialed

void loop() {

/*
  if (player.available()) {
    printDetail(player.readType(), player.read()); //Print the detail message from DFPlayer to handle different errors and states.
  }
*/
  line = analogRead(A0);

  // debug output for plotting curve with analog input and states:
//  Serial.print("A0:");
  Serial.print(line);
  Serial.print(" ");
//  Serial.print(",State:");
  Serial.print(state);
  Serial.print(" ");
//  Serial.print(",Number:");
  Serial.println(number*100);

  // in case of imminent state change, set 'initstate' 
  // variable --> next state will start with its initalization:
  initstate = (state != nextstate);
  if (initstate) state = nextstate;

  // rotary phone state machine:
  switch (state) {
    case STATE_START:
      // initialize system
      counter_change = 0;
      state = nextstate = STATE_HUNGUP;
      number = -1;        // -1 means: reciever is on the hook
      clear_pins();
      digitalWrite(PIN_HUNGUP, HIGH);
      break;

    case STATE_HUNGUP:
      // wait until we received a considerable number of levels lower than the HUNGUP-level:
      counter_change += (line < LEVEL_HIGH ? 1 : -(counter_change > 0));
      if (counter_change > HUNGUP_WAIT) {
        nextstate = STATE_LIFTED;   // proceed to next state
      }
      else
        delay(HUNGUP_DELAY);
      break;

    case STATE_LIFTED:
      if (initstate) {
        // entry into this state, do initialization:
        counter_change = 0;
        number = 0;                 // 0 means: reciever is off the hook
        clear_pins();
        digitalWrite(PIN_LIFTED, HIGH);
        player.playMp3Folder(DIALTONE);
      }

      if (line > LEVEL_HIGH)    // if receiver was put back on hook, abort
        nextstate = STATE_START;

      // wait until we received a considerable number of levels lower than the dialling-level:
      counter_change += (line < LEVEL_LOW ? 1 : -(counter_change > 0));
      if (counter_change > LIFTED_WAIT) {
        nextstate = STATE_WINDINGUP;  // proceed to next state
      }
      else
        delay(LIFTED_DELAY); 
      break;
      
    case STATE_WINDINGUP:   // 300
      if (initstate) {
        // new state: WINDINGUP
        counter_change = 0;
        clear_pins();
        digitalWrite(PIN_WINDINGUP, HIGH);
        player.pause();        
      }
      // if we received a considerable number of levels in the range of the high-level:
      counter_change += (line > LEVEL_HIGH ? 1 : -(counter_change > 0));
      if (counter_change > WINDINGUP_WAIT) {
        // successfully detected the first high impulse, go to PULSE_HIGH state:
        // new state: PULSE_HIGH
        number = 0;                 // we got our first number! (is incremented to 1 in the entry-function of the next state)
        nextstate = STATE_PULSE_HIGH;
      }
      else
        delay(WINDINGUP_DELAY); 
      break;

    case STATE_PULSE_HIGH:     // 400
      if (initstate) {
        // successfully detected the first high impulse, go to PULSE_HIGH state:
        // new state: PULSE_HIGH
        counter_change = 0;
        counter_highlevel = 0;
        number += 1;                 // we got the next number
        clear_pins();
        digitalWrite(PIN_PULSING, HIGH);
      }
      // if this state takes too long, it does not indicate a rotary HIGH, but the user put the
      // receiver back onto the cradle --> abort and go to start state:
      counter_highlevel += (line > LEVEL_HIGH ? 1 : -(counter_highlevel > 0));
      if (counter_highlevel > PULSE_HIGH_TIMEOUT) {
        nextstate = STATE_START;
      }

      // if we received enough levels below the high level, i. e. low level or playing level:
      counter_change += (line < LEVEL_HIGH ? 1 : -(counter_change > 0));
      if (counter_change > PULSE_HIGH_WAIT) {
        nextstate = STATE_PULSE_LOW;  // proceed to next state
      }
      else
        delay(PULSE_HIGH_DELAY); 
      break;

    case STATE_PULSE_LOW:     // 450
      if (initstate) {
        counter_change = 0;
        counter_midlevel = 0;
      }
      // if we received a considerable number of levels in the range of the high-level:
      counter_change += (line > LEVEL_LOW ? 1 : -(counter_change > 0));
      counter_midlevel += ((line > LEVEL_LOW) && (line < LEVEL_HIGH) ? 1 : -(counter_midlevel > 0));
      if (counter_change > PULSE_LOW_WAIT_HIGH) {
        nextstate = STATE_PULSE_HIGH;
      }
      else if (counter_midlevel > PULSE_LOW_WAIT_MIDDLE) {
        nextstate = STATE_PLAY;
      }
      else
        delay(PULSE_LOW_DELAY); 
      break;

    case STATE_PLAY:      // 500
      if (initstate) {
        counter_change = 0;
        display_number(number);
        clear_pins();
        digitalWrite(PIN_PLAYING, HIGH);
        player.playMp3Folder(number);
      }
      if (line > LEVEL_HIGH)    // if receiver was put back on hook, abort
        nextstate = STATE_START;

      counter_change += (line < LEVEL_LOW ? 1 : -(counter_change > 0));
      if (counter_change > PLAY_WAIT) {
        nextstate = STATE_WINDINGUP;
      }
      else
        delay(PLAY_DELAY); 
      break;

  }
}

// printDetail
// Write error message of MP3 player to serial line
//
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
