#include <LiquidCrystal.h>
#define MAXVOICENUM 4
#define VOICEPARAMNUM 7
#define NOTENUM 8 

#define LED 13

#define LED_DATA 2
#define LED_ST_CLK 3
#define LED_SH_CLK 4

#define SEQ_BUTT_EN 9
#define SEQ_BUTT_DATA 10
#define SEQ_BUTT_LOAD 11
#define SEQ_BUTT_CLK 12

#define MUTE_BUTT 7
#define PLAYSTOP_BUTT 8
#define MUTE_LED 5
#define PLAYSTOP_LED 6

#define LCDRS A0
#define LCDEN A1
#define LCDD4 A2
#define LCDD5 A3
#define LCDD6 A4
#define LCDD7 A5

#define MIDIBAUD 31250
#define MIDICC B10110001
#define MSGVOICE 16
#define MSGPARAMS 32
#define MSGSEQ 48
#define MSGTEMPO 64
#define MSGSTARTSTOP 80

typedef enum {INIT, WAIT, RUN} states;

// buttons and leds
bool playstop_curr, playstop_last;
bool mute_curr, mute_last;
byte seq_butt_curr, seq_butt_last = 0;
byte ctrl_butt_curr, ctrl_butt_last = 0;
byte tempoleds[NOTENUM] = {B10000000, B01000000, B00100000, B00010000, B00001000, B00000100, B00000010, B00000001};
byte sevensegleds[10] = {B10111110, B00001010, B01110110, B01011110, B11001010, B11011100, B11111100, B00001110, B11111110, B11011110};
byte j;    

// controller's state
states state = INIT;
byte voicesCnt = 0;
byte voiceN = -1;
byte voiceParamN = 0;
byte sequencers[MAXVOICENUM];
byte voiceParams[MAXVOICENUM][VOICEPARAMNUM];
word tempo = 0;
bool playing = false;
bool muted = false;

LiquidCrystal lcd(LCDRS, LCDEN, LCDD4, LCDD5, LCDD6, LCDD7);

void midiRead(byte * msg) {    
    if (Serial.available() > 2) {
      msg[0] = Serial.read(); // command
      msg[1] = Serial.read(); // data1
      msg[2] = Serial.read(); // data2
    }
  }

void midiWrite(byte s, byte d1, byte d2) {
  byte status = (s & B11111111); // status byte: msb = 1
  byte data1 = (d1 & B01111111); // data1 byte: msb = 0
  byte data2 = (d2 & B01111111); // data2 byte: msb = 0   
      
  Serial.write(status);
  Serial.write(data1);
  Serial.write(data2);
  }   


void sendPlay() {
  midiWrite(MIDICC, MSGSTARTSTOP + 1, 0);
  }

void sendStop() {
  midiWrite(MIDICC, MSGSTARTSTOP + 0, 0);
  }

void sendMute() {
  for (byte i = 0; i < voicesCnt; i++)
    midiWrite(MIDICC, MSGSTARTSTOP + 2, i);
  }

void sendUnMute() {
  for (byte i = 0; i < voicesCnt; i++)  
    midiWrite(MIDICC, MSGSTARTSTOP + 3, i);
  }

void setup()
{
  lcd.begin(16, 2);
    
  // MIDI  
  Serial.begin(MIDIBAUD);

  // LEDs
  pinMode(LED, OUTPUT);    
  pinMode(LED_DATA, OUTPUT);
  pinMode(LED_ST_CLK, OUTPUT); 
  pinMode(LED_SH_CLK, OUTPUT);
  // initial switching off
  digitalWrite(LED_ST_CLK, LOW);
  shiftOut(LED_DATA, LED_SH_CLK, LSBFIRST, 0);
  shiftOut(LED_DATA, LED_SH_CLK, LSBFIRST, 0);
  shiftOut(LED_DATA, LED_SH_CLK, LSBFIRST, 0);
  digitalWrite(LED_ST_CLK, HIGH);    
  pinMode(MUTE_LED, OUTPUT);
  pinMode(PLAYSTOP_LED, OUTPUT);
    
  // SEQ buttons
  pinMode(MUTE_BUTT, INPUT);
  pinMode(PLAYSTOP_BUTT, INPUT);   
  mute_last = digitalRead(MUTE_BUTT);
  playstop_last = digitalRead(PLAYSTOP_BUTT);    
  pinMode(SEQ_BUTT_EN, OUTPUT);
  pinMode(SEQ_BUTT_DATA, INPUT); 
  pinMode(SEQ_BUTT_LOAD, OUTPUT);
  pinMode(SEQ_BUTT_CLK, OUTPUT);  
  digitalWrite(SEQ_BUTT_EN, HIGH);
  digitalWrite(SEQ_BUTT_LOAD, HIGH);  
  // initial buttons sampling  
  digitalWrite(SEQ_BUTT_LOAD, LOW);
  delayMicroseconds(5);
  digitalWrite(SEQ_BUTT_LOAD, HIGH);
  delayMicroseconds(5);
  digitalWrite(SEQ_BUTT_CLK, HIGH);
  digitalWrite(SEQ_BUTT_EN, LOW);
  seq_butt_last = shiftIn(SEQ_BUTT_DATA, SEQ_BUTT_CLK, LSBFIRST);
  ctrl_butt_last = shiftIn(SEQ_BUTT_DATA, SEQ_BUTT_CLK, LSBFIRST);
  digitalWrite(SEQ_BUTT_EN, HIGH);   
  digitalWrite(SEQ_BUTT_LOAD, HIGH); 
}

void loop()
{   
  byte msg[3] = {0, 0, 0};
  midiRead(msg);
  byte msgtype = (msg[1] & B11110000);
  byte msgsubtype = (msg[1] & B00001111);
  byte msb = msg[1] << 7;  
  
  switch(state) {
    case INIT: {
      if (msgtype == MSGVOICE) {                      // voices count           
        voicesCnt = msg[2];
        }
      else if (msgtype == MSGPARAMS) {                 // init voice params        
        voiceParams[voiceN][msgsubtype] = msg[2];
        }        
      else if (msgtype == MSGSEQ) {                      // init sequencer state          
        voiceN++;                                     
        sequencers[voiceN] = msb + msg[2];
        }
      else if (msgtype == MSGTEMPO) {                 // init tempo                        
        j = (NOTENUM - 1) - msgsubtype;        
        tempo = msg[2];  
        }                
      else if (msgtype == MSGSTARTSTOP && msgsubtype == 1) {  // play message                
        voiceN = 0;
        voiceParamN = 0;              
        lcd.clear();                            // print voice parameters
        lcd.print("V: ");
        lcd.print(voiceN);
        lcd.setCursor(5, 0);
        lcd.print("P: ");
        lcd.setCursor(8, 0);         
        switch (voiceParamN) {
          case 0:
            lcd.print("wav freq");                  
            break;
          case 1:
            lcd.print("wav type");                  
            break;                         
          case 2:
            lcd.print("att time");                  
            break;
          case 3:
            lcd.print("dec time");                  
            break;
          case 4:
            lcd.print("sus time");                  
            break;
          case 5:
            lcd.print("sus lev ");                  
            break;
          case 6:
            lcd.print("rel time");                  
            break;                                                                                                         
          }
        lcd.setCursor(0, 1); 
        lcd.print("Val: ");
        lcd.setCursor(5, 1);     
        if (voiceParams[voiceN][voiceParamN] < 10)
            lcd.print("  ");                                                                    
        else if (voiceParams[voiceN][voiceParamN] < 100)
            lcd.print(" ");                                                   
        lcd.print(voiceParams[voiceN][voiceParamN]);  
        lcd.setCursor(9, 1);
        lcd.print("tmp: ");
        if (tempo < 10)
          lcd.print(" ");
        lcd.print(tempo);
        state = WAIT;               
        }                       
      break;
      }
    case WAIT: {
      playstop_curr = digitalRead(PLAYSTOP_BUTT);           // play button pressed
      if (playstop_curr != playstop_last) {
        sendPlay();        
        playstop_last = playstop_curr;
        playing = true;
        state = RUN;
        }
      break;
      }
    case RUN: {             
      // LEDS
      digitalWrite(LED_ST_CLK, LOW);
      shiftOut(LED_DATA, LED_SH_CLK, LSBFIRST, tempoleds[j]);                           // blue  (tempo)    
      shiftOut(LED_DATA, LED_SH_CLK, LSBFIRST, sequencers[voiceN] & (~tempoleds[j]));   // green  (pattern)
      shiftOut(LED_DATA, LED_SH_CLK, LSBFIRST, sevensegleds[voiceN]);                  // seven segment display (voice number)
      digitalWrite(LED_ST_CLK, HIGH);      
      digitalWrite(PLAYSTOP_LED, playing);
      digitalWrite(MUTE_LED, muted);   

      // PLAYSTOP & MUTE BUTTONS
      playstop_curr = digitalRead(PLAYSTOP_BUTT); 
      if (playstop_curr != playstop_last) {
        sendStop();
        playstop_last = playstop_curr;
        playing = false;
        digitalWrite(PLAYSTOP_LED, playing);
        voicesCnt = 0;
        voiceN = -1;
        voiceParamN = 0;      
        tempo = 0;        
        for (byte i = 0; i < MAXVOICENUM; i++) {
          for (byte j = 0; j < NOTENUM; j++) {
            voiceParams[i][j] = 0;
            sequencers[i] = 0;
            }
          }

        lcd.clear();
        digitalWrite(LED_ST_CLK, LOW);
        shiftOut(LED_DATA, LED_SH_CLK, LSBFIRST, 0);  // blue  (tempo)    
        shiftOut(LED_DATA, LED_SH_CLK, LSBFIRST, 0);  // green  (pattern)
        shiftOut(LED_DATA, LED_SH_CLK, LSBFIRST, 0);  // sevenseg
        digitalWrite(LED_ST_CLK, HIGH);         
        state = INIT;          
        }

      mute_curr = digitalRead(MUTE_BUTT);         
      if (mute_curr != mute_last) {
        if (muted) {
          sendUnMute();
          muted = false;
        }
        else {
          sendMute();
          muted = true;
        }
        mute_last = mute_curr;
        }
                  
      // SEQ & CTRL buttons
      digitalWrite(SEQ_BUTT_LOAD, LOW);
      delayMicroseconds(5);
      digitalWrite(SEQ_BUTT_LOAD, HIGH);
      delayMicroseconds(5);
      digitalWrite(SEQ_BUTT_CLK, HIGH);
      digitalWrite(SEQ_BUTT_EN, LOW);
      seq_butt_curr = shiftIn(SEQ_BUTT_DATA, SEQ_BUTT_CLK, LSBFIRST);
      ctrl_butt_curr = shiftIn(SEQ_BUTT_DATA, SEQ_BUTT_CLK, LSBFIRST);
      digitalWrite(SEQ_BUTT_EN, HIGH); 
    
      if (seq_butt_curr != seq_butt_last) {    
        byte seq_butt_change = seq_butt_last ^ seq_butt_curr; // XOR reflects only changes                       
        sequencers[voiceN] ^= seq_butt_change;        
        seq_butt_last = seq_butt_curr;      
        byte data1 = MSGSEQ + (sequencers[voiceN] >> 7);         // MSB = 1st seq button; MSGPARAMS = msg type; at synth side both parts are unmasked and then processed
        byte data2 = sequencers[voiceN];          // remaining 7bits (bcause only 0-127 allowed in MIDI data) = buttons 2 - 8
        midiWrite(MIDICC, data1, data2); // CC + channel 1          
        }
    
      if (ctrl_butt_curr != ctrl_butt_last) {
        byte ctrl_butt_change = ctrl_butt_last ^ ctrl_butt_curr; // XOR reflects only changes          
        for (byte i = 0; i < NOTENUM; i++) {
          if (ctrl_butt_change & (1 << i)) {
            switch (i) {
              case 0:
                if ((tempo - 1) < 1)
                  tempo = 1;
                else
                  tempo--;
                midiWrite(MIDICC, MSGTEMPO, tempo);                  
                break;
              case 1:
                if (voiceParamN == 1) {                             // wave type
                  if ((voiceParams[voiceN][voiceParamN] + 1) > 3)
                    voiceParams[voiceN][voiceParamN] = 3;
                  else
                    voiceParams[voiceN][voiceParamN]++;                                          
                }
                else {                                              // other params
                  if ((voiceParams[voiceN][voiceParamN] + 1) > 127)
                    voiceParams[voiceN][voiceParamN] = 127;
                  else
                    voiceParams[voiceN][voiceParamN]++;                        
                }
                midiWrite(MIDICC, MSGPARAMS + voiceParamN, voiceParams[voiceN][voiceParamN]);                                               
                break;              
              case 2:
                if ((voiceParamN + 1) == 7)
                  voiceParamN = 0;
                else
                  voiceParamN++;                                         
                midiWrite(MIDICC, MSGPARAMS + voiceParamN, voiceParams[voiceN][voiceParamN]);                                              
                break;              
              case 3:
                if ((voiceN + 1) >= voicesCnt)
                  voiceN = 0;
                else
                  voiceN++;            
                midiWrite(MIDICC, MSGVOICE, voiceN);                   
                break;              
              case 4:
                if ((tempo + 1) > 99)
                  tempo = 99;
                else
                  tempo++;
                midiWrite(MIDICC, MSGTEMPO, tempo);                  
                break;              
              case 5:
                if (voiceParamN == 1) {                             // wave type
                  if ((voiceParams[voiceN][voiceParamN] - 1) < 0)
                    voiceParams[voiceN][voiceParamN] = 0;
                  else
                    voiceParams[voiceN][voiceParamN]--;                                  
                }
                else {                                              // other params
                  if ((voiceParams[voiceN][voiceParamN] - 1) < 1)
                    voiceParams[voiceN][voiceParamN] = 1;
                  else
                    voiceParams[voiceN][voiceParamN]--;                                                    
                }
                midiWrite(MIDICC, MSGPARAMS + voiceParamN, voiceParams[voiceN][voiceParamN]);                                     
                break;              
              case 6:
                if ((voiceParamN - 1) == -1)
                  voiceParamN = 6;
                else
                  voiceParamN--;        
                midiWrite(MIDICC, MSGPARAMS + voiceParamN, voiceParams[voiceN][voiceParamN]);                                               
                break;              
              case 7:
                if ((voiceN - 1) < 0)
                  voiceN = voicesCnt - 1;
                else
                  voiceN--;                             
                midiWrite(MIDICC, MSGVOICE, voiceN);                                     
                break;              
              }
                                
              lcd.setCursor(3, 0); 
              lcd.print(voiceN);     
              lcd.setCursor(8, 0); 
              switch (voiceParamN) {
                case 0:
                  lcd.print("wav freq");                  
                  break;                            
                case 1:
                  lcd.print("wav type");                  
                  break;                
                case 2:
                  lcd.print("att time");                  
                  break;
                case 3:
                  lcd.print("dec time");                  
                  break;
                case 4:
                  lcd.print("sus time");                  
                  break;
                case 5:
                  lcd.print("sus lev ");                  
                  break;
                case 6:
                  lcd.print("rel time");                  
                  break;                                                                                                            
                }
              lcd.setCursor(5, 1);     
              if (voiceParams[voiceN][voiceParamN] < 10)
                  lcd.print("  ");                                                                    
              else if (voiceParams[voiceN][voiceParamN] < 100)
                  lcd.print(" ");                                
              lcd.print(voiceParams[voiceN][voiceParamN]); 
              lcd.setCursor(9, 1);
              lcd.print("tmp: ");
              if (tempo < 10)
                lcd.print(" ");
              lcd.print(tempo);
            }
          }
        ctrl_butt_last = ctrl_butt_curr;    
        }

      // MESSAGES
      if (msgtype == MSGTEMPO) {        
        j = (NOTENUM - 1) - msgsubtype; // num sent from synth is bit position in byte; here it is index to array -> 7 (from seq) == 0th index of tempo[]
        if (j == 0)                   // only at the begining of sequence
          tempo = msg[2];             // update tempo (for better usability)
        }  
      else if (msgtype == MSGSTARTSTOP && msgsubtype == 0) {
        playing = false;
        voicesCnt = 0;
        voiceN = -1;
        voiceParamN = 0;      
        tempo = 0;        
        for (byte i = 0; i < MAXVOICENUM; i++) {
          for (byte j = 0; j < NOTENUM; j++) {
            voiceParams[i][j] = 0;
            sequencers[i] = 0;
            }
          }
        lcd.clear();        
        digitalWrite(LED_ST_CLK, LOW);
        shiftOut(LED_DATA, LED_SH_CLK, LSBFIRST, 0);  // blue  (tempo)    
        shiftOut(LED_DATA, LED_SH_CLK, LSBFIRST, 0);  // green  (pattern)
        shiftOut(LED_DATA, LED_SH_CLK, LSBFIRST, 0);  // sevenseg
        digitalWrite(LED_ST_CLK, HIGH);                   
        state = INIT;        
      }    
        delay(20);
      break;
      }
    }
}

// just for debug purpose
void blinkLed() {
  digitalWrite(LED,HIGH);  //Turn LED on
  delay(10);
  digitalWrite(LED,LOW);  //Turn LED off 
}

