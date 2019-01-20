#include <RotaryEncoder.h>
#include <DueTimer.h>
#include <LiquidCrystal.h>
#include <LiquidMenu.h>
#include <DueFlashStorage.h>

//************* SYNTH
#define MAXVOICENUM 4
#define MAXSYNTHNUM 4
#define MAXOSCILLNUM 4
#define NOTENUM 8
#define VOICEPARAMNUM 7
#define SONGNUM 8
#define SUSTLEVELPARAMIDX 3

#define WAVE_SAMPLES 2048
#define SAMPLE_RATE 44100.0
#define SAMPLES_PER_CYCLE 2048
#define SAMPLES_PER_CYCLE_FIXEDPOINT (SAMPLES_PER_CYCLE<<20)
#define TICKS_PER_CYCLE (float)((float)SAMPLES_PER_CYCLE_FIXEDPOINT/(float)SAMPLE_RATE)
#define INCREMENT_ONE_FIXEDPOINT 1<<16 // 16384 32768
#define MAXSAMPLEVAL 4095
//************* MESSAGES
#define MIDIBAUD 31250
#define MIDICC B10110001
#define MSGVOICE 16
#define MSGPARAMS 32
#define MSGSEQ 48
#define MSGTEMPO 64
#define MSGSTARTSTOP 80

#define SENDDELAY 50

//************* GPIO
#define ROTARYA 52
#define ROTARYB 53
#define LEFTBUTT 22
#define RIGHTBUTT 23
#define LCDRS 40
#define LCDEN 41
#define LCDD4 42
#define LCDD5 43
#define LCDD6 44
#define LCDD7 45

//************* OTHERS
#define SONGSSTARTADDRESS 4

struct Song {
  char * name;
  byte voiceSeqParams[MAXVOICENUM][VOICEPARAMNUM + 1];
};

typedef enum {INIT, CREATESYNTHS, RUN, STOP} states;

bool playing = false;
word tempo = 5;
word tempo_buff = tempo;    // initially they are same
uint32_t last_tempo;
byte ctrlsCnt = 0;
char * currSongName = "";
signed char pos = 0;
signed char newPos = 0;
byte paramIndex = 0;
char * ctrlsCntParamVals[] = {"1", "2", "3", "4"};
char * songsParamVals[] = {"Song   1", "Song   2", "Song   3", "Song   4", "Song   5", "Song   6", "Song   7", "Song   8"};
char ** curParamVals = nullptr;
char * curParamVal = "";
byte paramLimits[2] = {sizeof(ctrlsCntParamVals) / sizeof(ctrlsCntParamVals[0]), sizeof(songsParamVals) / sizeof(songsParamVals[0])};
byte curParamIndexLimit = 0;
bool subMenu = false;
uint32_t songsStartAddresses[SONGNUM];
states state = INIT; 
byte masterVolume = 50;
byte leftButtState = false;
byte leftButtLastState = false;
byte rightButtState = false;
byte rightButtLastState = false;
uint16_t nSineTable[WAVE_SAMPLES];
uint16_t nSquareTable[WAVE_SAMPLES];
uint16_t nSawTable[WAVE_SAMPLES];
uint16_t nTriangleTable[WAVE_SAMPLES];
uint16_t * waveTypes[MAXVOICENUM] = {&nSineTable[0], &nSquareTable[0], &nSawTable[0], &nTriangleTable[0]};
                               //A     A#      B      C     C#      D     D#      E      F     F#      G     G#
uint16_t waveFreqs[127] = {     55,    58,    62,    65,    69,    73,    78,    82,    87,    92,    98,   104,   // 1
                               110,   117,   123,   131,   139,   147,   156,   165,   175,   185,   196,   208,   // 2
                               220,   233,   247,   262,   277,   294,   311,   330,   349,   370,   392,   415,   // 3
                               440,   466,   494,   523,   554,   587,   622,   659,   698,   740,   784,   832,   // 4
                               880,   932,   988,  1047,  1109,  1175,  1245,  1319,  1397,  1480,  1568,  1661,   // 5
                              1760,  1865,  1976,  2093,  2217,  2394,  2489,  2637,  2794,  2960,  3136,  3322,   // 6
                              3520,  3729,  3951,  4186,  4435,  4699,  4978,  5274,  5588,  5920,  6272,  6645,   // 7
                              7040,  7458,  7902,  8372,  8869,  9397,  9956, 10548, 11175, 11839, 12543, 13289,   // 8
                             14080, 14917, 15804, 15804, 15804, 15804, 15804, 15804, 15804, 15804, 15804, 15804,   // 9
                             15804, 15804, 15804, 15804, 15804, 15804, 15804, 15804, 15804, 15804, 15804, 15804,   // 10
                             15804, 15804, 15804, 15804, 15804, 15804, 15804
                          };

//************* FILTER
#define FX_SHIFT 8
#define SHIFTED_1 256
uint8_t q = 0;
uint8_t f = 50;

int buf0, buf1;
// multiply two fixed point numbers (returns fixed point)
inline
unsigned int ucfxmul(uint8_t a, uint8_t b)
{
  return (((unsigned int)a*b)>>FX_SHIFT);
}
  
// multiply two fixed point numbers (returns fixed point)
inline
int ifxmul(int a, uint8_t b)
{
  return ((a*b)>>FX_SHIFT);
}
  
// multiply two fixed point numbers (returns fixed point)
inline
long fxmul(long a, int b)
{
  return ((a*b)>>FX_SHIFT);
}

unsigned int fb = q + fxmul(q, (int)SHIFTED_1 - (f / 128));

inline
int filter(int in, uint8_t q, uint8_t f, unsigned int fb)
{
  buf0+=fxmul(((in - buf0) + fxmul(fb, buf0-buf1)), f);
  buf1+=ifxmul(buf0-buf1, f); // could overflow if input changes fast
  return buf1;
}

//************* OBJECTS
LiquidCrystal lcd(LCDRS, LCDEN, LCDD4, LCDD5, LCDD6, LCDD7);
RotaryEncoder encoder(ROTARYA, ROTARYB);

LiquidLine songs_line(0, 0, "Songs");
LiquidLine songs_names_line(0, 1, curParamVal);
LiquidScreen screen1(songs_line, songs_names_line);
LiquidLine midi_ctrls_line(0, 0, "MIDI ctrls count");
LiquidLine num_line(0, 1, curParamVal);
LiquidScreen screen2(midi_ctrls_line, num_line);
LiquidMenu menu(lcd, screen1, screen2);

DueFlashStorage store;

void playSound() {
  Timer3.attachInterrupt(audioHandler).setFrequency(SAMPLE_RATE).start(); // start the audio interrupt at 44.1kHz
}

void stopSound() {
  Timer3.stop();
}

//************* MY CLASSES
class Oscillator {
    byte waveFreq;  
    byte waveType;
    uint32_t ulPhaseAccumulator = 0;

  public:
    Oscillator(byte wvFreq, byte wvTp) {
      waveFreq = wvFreq;      
      waveType = wvTp;
    }

    uint32_t getSample() {
      // from http://groovesizer.com/tb2-resources/
      ulPhaseAccumulator += waveFreqs[waveFreq] * INCREMENT_ONE_FIXEDPOINT;

      if (ulPhaseAccumulator > INCREMENT_ONE_FIXEDPOINT)
      {
        ulPhaseAccumulator -= INCREMENT_ONE_FIXEDPOINT;
        //ulPhaseAccumulator = 0;
      }
      
      return *(waveTypes[waveType] + (ulPhaseAccumulator >> 20));
    }


    byte getWaveFreq() {
      return waveFreq;
    }

    byte getWaveType() {
      return waveType;
    }

    void setWaveFreq(byte val) {
      waveFreq = val;
    }
    
    void setWaveType(byte val) {
      waveType = val;
    }
};

class Voice {
    Oscillator * oscillators[MAXOSCILLNUM];
    byte params[5];  // attack time, decay time, sustain time, sustain level, release time
    uint16_t envelopeVolume;
    uint32_t attackStartTime;
    uint32_t decayStartTime;
    uint32_t sustainStartTime;
    uint32_t releaseStartTime;
    byte envelopeProgress;
    bool muted;

  public:
    Voice(byte wavFrq, byte wavTp, byte atTm, byte decTm, byte sustTm, byte sustLv, byte relTm) {
      envelopeProgress = 255;     // = initial silence
      params[0] = atTm;
      params[1] = decTm;
      params[2] = sustTm;
      params[3] = sustLv;
      params[4] = relTm;

      muted = false;

      for (byte i = 0; i < 1; i++) {
        oscillators[i] = new Oscillator(wavFrq, wavTp);
      }
    }

    void setParamValue(byte n, byte val) {
      switch (n) {
        case 0:
          oscillators[0]->setWaveFreq(val);
          break;
        case 1:
          oscillators[0]->setWaveType(val);
          break;
        default:
          params[n] = val;
          break;
      }
    }

    void trigger() {
      if (!muted) {
        attackStartTime = millis();
        envelopeProgress = 0;
      }
    }

    uint32_t getSample() {
      // from http://groovesizer.com/tb2-resources/
      switch (envelopeProgress) {
        case 0: // ATTACK
          if ((millis() - attackStartTime) > params[0]) {
            decayStartTime = millis();
            envelopeProgress = 1;
          }
          else {
            envelopeVolume = map(millis(), attackStartTime, attackStartTime + params[0], 0, 1270);
          }
          break;

        case 1: // DECAY
          if ((millis() - decayStartTime) > params[1]) {
            sustainStartTime = millis();
            envelopeProgress = 2;
          }
          else {
            envelopeVolume = map(millis(), decayStartTime, decayStartTime + params[1], 1270, params[3]);
          }
          break;

        case 2: // SUSTAIN
          if ((millis() - sustainStartTime) > params[2]) {
            releaseStartTime = millis();
            envelopeProgress = 3;
          }
          else {
            envelopeVolume = params[3];
          }
          break;

        case 3: // RELEASE
          if ((millis() - releaseStartTime) > params[4]) {
            envelopeProgress = 255;
          }
          else {
            envelopeVolume = map(millis(), releaseStartTime, releaseStartTime + params[4], params[3], 0);
          }
          break;

        case 255: // MUTE
          envelopeVolume = 0;
          break;
      }

      uint32_t out = 0;
      for (byte i = 0; i < 1; i++) {
        out += oscillators[i]->getSample();
      }
      return filter((out * envelopeVolume) >> 11 /*13*/, q, f, fb);
      //return (out * envelopeVolume) >> 10;
    }

    void setMuted(bool m) {
      muted = m;
    }

    void getVoiceParams(byte vp[]) {
      vp[0] = oscillators[0]->getWaveFreq();            
      vp[1] = oscillators[0]->getWaveType();
      vp[2] = params[0];
      vp[3] = params[1];
      vp[4] = params[2];
      vp[5] = params[3];
      vp[6] = params[4];
    }
};

class SubSynth {
    Voice * voices[MAXVOICENUM];
    byte voicesCnt;
    byte sequences[MAXVOICENUM];
    byte voiceN;
    byte voiceParamN;
    byte voicesParamsBuff[MAXVOICENUM][VOICEPARAMNUM];    
    bool voicesMutedBuff[MAXVOICENUM];
    byte st;
    byte myId;

  public:
    SubSynth(byte vCnt, byte id, byte startVoiceId, Song song) {
      voicesCnt = vCnt;
      myId = id;
      st = NOTENUM - 1;
      voiceN = 0;
      voiceParamN = 0;
      switch (myId) {
        case 0:
          Serial1.begin(MIDIBAUD);
          break;
        case 1:
          Serial2.begin(MIDIBAUD);
          break;
        case 2:
          Serial3.begin(MIDIBAUD);
          break;
        case 3:
          Serial.begin(MIDIBAUD);
          break;
      }

      for (byte i = 0; i < voicesCnt; i++) {
        voices[i] = new Voice(song.voiceSeqParams[startVoiceId][0],
                              song.voiceSeqParams[startVoiceId][1],
                              song.voiceSeqParams[startVoiceId][2],
                              song.voiceSeqParams[startVoiceId][3],
                              song.voiceSeqParams[startVoiceId][4],
                              song.voiceSeqParams[startVoiceId][5],
                              song.voiceSeqParams[startVoiceId][6]);
        
        sequences[i] = song.voiceSeqParams[startVoiceId][7];
        
        for (byte j = 0; j < VOICEPARAMNUM; j++)
          voicesParamsBuff[i][j] = song.voiceSeqParams[startVoiceId][j];

        voicesMutedBuff[i] = false;
        
        startVoiceId++;
      }
    }

    void midiRead(byte msg[3]) {
      switch (myId) {
        case 0:
          if (Serial1.available() > 2) {
            msg[0] = Serial1.read(); // command
            msg[1] = Serial1.read(); // data1
            msg[2] = Serial1.read(); // data2
          }
          break;
        case 1:
          if (Serial2.available() > 2) {
            msg[0] = Serial2.read(); // command
            msg[1] = Serial2.read(); // data1
            msg[2] = Serial2.read(); // data2
          }
          break;
        case 2:
          if (Serial3.available() > 2) {
            msg[0] = Serial3.read(); // command
            msg[1] = Serial3.read(); // data1
            msg[2] = Serial3.read(); // data2
          }
          break;
        case 3:
          if (Serial.available() > 2) {
            msg[0] = Serial.read(); // command
            msg[1] = Serial.read(); // data1
            msg[2] = Serial.read(); // data2
          }
          break;
      }
    }

    void midiWrite(byte s, byte d1, byte d2) {
      byte status = (s & B11111111); // status byte: msb = 1
      byte data1 = (d1 & B01111111); // data1 byte: msb = 0
      byte data2 = (d2 & B01111111); // data2 byte: msb = 0

      switch (myId) {
        case 0:
          Serial1.write(status);
          Serial1.write(data1);
          Serial1.write(data2);
          break;
        case 1:
          Serial2.write(status);
          Serial2.write(data1);
          Serial2.write(data2);
          break;
        case 2:
          Serial3.write(status);
          Serial3.write(data1);
          Serial3.write(data2);
          break;
        case 3:
          Serial.write(status);
          Serial.write(data1);
          Serial.write(data2);
          break;
      }
    }

    void sendVoicesCnt() {
      midiWrite(MIDICC, MSGVOICE, voicesCnt);
    }

    void sendInitParams(byte voiceN) {
      byte vp[VOICEPARAMNUM];
      voices[voiceN]->getVoiceParams(vp);
      for (byte i = 0; i < VOICEPARAMNUM; i++) {
        midiWrite(MIDICC, MSGPARAMS + i, vp[i]);
        delay(SENDDELAY);
      }
    }

    void sendInitSeq(byte n) {
      midiWrite(MIDICC, MSGSEQ + (sequences[n] >> 7), sequences[n]);  // MSB of sequences[n] as LSB of data1 and rest of sequences[n] as data2
    }

    void sendInitTempo() {
      midiWrite(MIDICC, MSGTEMPO + st, tempo);
    }

    void sendStart() {
      midiWrite(MIDICC, MSGSTARTSTOP + 1, 0);
    }

    void sendStop() {
      midiWrite(MIDICC, MSGSTARTSTOP + 0, 0);
    }

    void sendTick() {
      midiWrite(MIDICC, MSGTEMPO + st, tempo);
    }

    void receiveMidi() {
      byte msg[3] = {0, 0, 0};
      midiRead(msg);

      //byte status = msg[0];
      byte msgtype = msg[1] & B11110000;
      byte msgsubtype = msg[1] & B00001111;
      switch (msgtype) {
        case MSGVOICE:
          voiceN = msg[2];
          break;

        case MSGPARAMS:
          voicesParamsBuff[voiceN][msgsubtype] = msg[2];
          break;

        case MSGSEQ:
          sequences[voiceN] = (msg[1] << 7) + msg[2];  // LSB of msg[1] as MSB of sequences[voiceN] + rest of sequences[voiceN] in msg[2]
          break;

        case MSGTEMPO:
          tempo_buff = msg[2];
          break;

        case MSGSTARTSTOP:
          switch (msgsubtype) {
            case 0:
              stopSound();
              playing = false;
              state = STOP;
              break;

            case 1:
              playSound();
              playing = true;
              state = RUN;
              break;

            case 2:
              voicesMutedBuff[msg[2]] = true;
              break;

            case 3:
              voicesMutedBuff[msg[2]] = false;            
              break;
          }
          break;
      }
    }

    void generateSound() {
      // from http://groovesizer.com/tb2-resources/
      uint32_t sampleVoices;
      for (byte i = 0; i < voicesCnt; i++)
      {
        sampleVoices += voices[i]->getSample();
      }

      //sampleVoices = filter(sampleVoices / voicesCnt, q, f, fb);
      sampleVoices = sampleVoices / voicesCnt;

      // write to DAC0
      dacc_set_channel_selection(DACC_INTERFACE, 0);
      dacc_write_conversion_data(DACC_INTERFACE, (sampleVoices * masterVolume) / 100);
      // write to DAC1
      dacc_set_channel_selection(DACC_INTERFACE, 1);
      dacc_write_conversion_data(DACC_INTERFACE, (sampleVoices * masterVolume) / 100);
    }

    void sequencer() {
      // update synths at the begining only - for better usability
      if (st == (NOTENUM - 1)) {
        tempo = tempo_buff;        
        for (byte k = 0; k < voicesCnt; k++) {
          voices[k]->setMuted(voicesMutedBuff[k]);
                    
          for (byte l = 0; l < VOICEPARAMNUM; l++) {
            voices[k]->setParamValue(l, voicesParamsBuff[k][l]);        
          }
        }      
      }

      // play note
      for (byte j = 0; j < voicesCnt; j++) {
        if (sequences[j] & (1 << st)) {
          voices[j]->trigger();
        }
      }

      // counting
      if (st == 0) {
        st = NOTENUM - 1;
      }
      else {
        st--;
      }
    }

    void getVoiceSeqParams(byte n, byte voiceParamsSeq[]) {
      byte voiceParams[7];
      voices[n]->getVoiceParams(voiceParams);
      voiceParamsSeq[0] = voiceParams[0];
      voiceParamsSeq[1] = voiceParams[1];
      voiceParamsSeq[2] = voiceParams[2];
      voiceParamsSeq[3] = voiceParams[3];
      voiceParamsSeq[4] = voiceParams[4];
      voiceParamsSeq[5] = voiceParams[5];
      voiceParamsSeq[6] = voiceParams[6];
      voiceParamsSeq[7] = sequences[n];
    }
};

//************* WAVETABLE FUNCS
// from http://groovesizer.com/tb2-resources/
void createSineTable() {
  for (uint16_t nIndex = 0; nIndex < WAVE_SAMPLES; nIndex++) {
    // SINE
    nSineTable[nIndex] = (uint16_t)  (((1 + sin(((2.0 * PI) / WAVE_SAMPLES) * nIndex)) * (float) MAXSAMPLEVAL) / 2);
  }
}

void createSquareTable(int16_t pw)
{
  static int16_t lastPw = 127; // don't initialize to 0
  if (pw != lastPw)
  {
    for (uint16_t nIndex = 0; nIndex < WAVE_SAMPLES; nIndex++)
    {
      // SQUARE
      if (nIndex <= ((WAVE_SAMPLES / 2) + pw))
        nSquareTable[nIndex] = 0;
      else
        nSquareTable[nIndex] = MAXSAMPLEVAL;
    }
    lastPw = pw;
  }
}

void createSawTable()
{
  for (uint16_t nIndex = 0; nIndex < WAVE_SAMPLES; nIndex++)
  {
    // SAW
    nSawTable[nIndex] = (MAXSAMPLEVAL / WAVE_SAMPLES) * nIndex;
  }
}

void createTriangleTable()
{
  for (uint16_t nIndex = 0; nIndex < WAVE_SAMPLES; nIndex++)
  {
    // Triangle
    if (nIndex < WAVE_SAMPLES / 2)
      nTriangleTable[nIndex] = (MAXSAMPLEVAL / (WAVE_SAMPLES / 2)) * nIndex;
    else
      nTriangleTable[nIndex] = (MAXSAMPLEVAL / (WAVE_SAMPLES / 2)) * (WAVE_SAMPLES - nIndex);
  }
}

SubSynth * subSynths[MAXSYNTHNUM];

void audioHandler() {
  for (byte i = 0; i < ctrlsCnt; i++) {
    subSynths[i]->generateSound();
  }
}

//************* MENU FUNCS
void switchToCtrlsCnt() {
  curParamVals = ctrlsCntParamVals;
  curParamIndexLimit = paramLimits[0];
}

void switchToSongs() {
  curParamVals = songsParamVals;
  curParamIndexLimit = paramLimits[1];
}

void setPlayMode() {
  currSongName = curParamVal;
  state = CREATESYNTHS;
}


void setCtrlsCnt() {
  ctrlsCnt = getInt(curParamVal);
}

int getInt(char * string) {
  if (string == "1")
    return 1;
  else if (string == "2")
    return 2;
  else if (string == "3")
    return 3;
  else if (string == "4")
    return 4;
}

uint8_t rFocus[8] = {
  0b00001,
  0b00011,
  0b00110,
  0b01100,
  0b01100,
  0b00110,
  0b00011,
  0b00001
};

//************* MAIN
void setup() {
  pinMode(LEFTBUTT, INPUT);
  pinMode(RIGHTBUTT, INPUT);
  leftButtLastState = digitalRead(LEFTBUTT);
  rightButtLastState = digitalRead(RIGHTBUTT);

  uint8_t codeRunningForTheFirstTime = store.read(0);
  if (codeRunningForTheFirstTime) {
    int start_address = SONGSSTARTADDRESS;
    for (byte i = 0; i < SONGNUM; i++) {
      Song song;
      song.name = songsParamVals[i];
      // voice 0
      song.voiceSeqParams[0][0] = 12;             // wave frequency
      song.voiceSeqParams[0][1] = 1;              // wave type
      song.voiceSeqParams[0][2] = 127;              // attack time
      song.voiceSeqParams[0][3] = 127;             // decay time
      song.voiceSeqParams[0][4] = 127;             // sustain time
      song.voiceSeqParams[0][5] = 100;            // sustain level
      song.voiceSeqParams[0][6] = 127;             // release time
      song.voiceSeqParams[0][7] = B10101010;      // sequencer
      // voice 1
      song.voiceSeqParams[1][0] = 24;
      song.voiceSeqParams[1][1] = 1;
      song.voiceSeqParams[1][2] = 5;
      song.voiceSeqParams[1][3] = 100;
      song.voiceSeqParams[1][4] = 127;
      song.voiceSeqParams[1][5] = 100;
      song.voiceSeqParams[1][6] = 100;
      song.voiceSeqParams[1][7] = B01100000;
      // voice 2
      song.voiceSeqParams[2][0] = 36;
      song.voiceSeqParams[2][1] = 1;
      song.voiceSeqParams[2][2] = 127;
      song.voiceSeqParams[2][3] = 127;
      song.voiceSeqParams[2][4] = 50;
      song.voiceSeqParams[2][5] = 100;
      song.voiceSeqParams[2][6] = 5;
      song.voiceSeqParams[2][7] = B00010001;
      // voice 3
      song.voiceSeqParams[3][0] = 48;
      song.voiceSeqParams[3][1] = 1;
      song.voiceSeqParams[3][2] = 127;
      song.voiceSeqParams[3][3] = 127;
      song.voiceSeqParams[3][4] = 50;
      song.voiceSeqParams[3][5] = 80;
      song.voiceSeqParams[3][6] = 5;
      song.voiceSeqParams[3][7] = B01000110;

      byte b[sizeof(Song)];
      memcpy(b, &song, sizeof(Song));
      store.write(start_address, b, sizeof(Song));

      songsStartAddresses[i] = start_address;

      start_address += sizeof(b);                  // start address for next song
    }

    store.write(0, 0);
  }

  menu.set_focusSymbol(Position::RIGHT, rFocus);
  num_line.attach_function(1, switchToCtrlsCnt);
  num_line.attach_function(2, setCtrlsCnt);
  songs_names_line.attach_function(1, switchToSongs);
  songs_names_line.attach_function(2, setPlayMode);

  lcd.begin(16, 2);
  lcd.print("SynthSeq menu");

  analogWrite(DAC0, 0);
  analogWrite(DAC1, 0);

  createSineTable();
  createSquareTable(100);
  createSawTable();
  createTriangleTable();
  //Serial.begin(9600);
}

void loop() {
  static int start_address = SONGSSTARTADDRESS;
  switch (state) {
    case INIT: {
        leftButtState = digitalRead(LEFTBUTT);
        rightButtState = digitalRead(RIGHTBUTT);

        if (rightButtState != rightButtLastState) {
          if (!subMenu) {
            menu.switch_focus();
            subMenu = true;
            menu.call_function(1);
            pos = 0;
            newPos = 0;
            paramIndex = 0;
          }
          else {
            menu.call_function(2);
            menu.switch_focus();
            curParamVal = "";
            subMenu = false;
          }

          rightButtLastState = rightButtState;
        }

        if (leftButtState != leftButtLastState) {
          if (subMenu) {
            menu.switch_focus();
            subMenu = false;
            pos = 0;
            paramIndex = 0;
            curParamVal = "";
          }

          leftButtLastState = leftButtState;
        }

        encoder.tick();    
        newPos = encoder.getPosition();           
        if (pos != newPos) {
          if (!subMenu) {
            if (newPos > pos) {
              menu.next_screen();
            }
            else if (newPos < pos) {
              menu.previous_screen();
            }
          }
          else {
            if (newPos > pos) {
              if ((paramIndex + 1) > (curParamIndexLimit - 1)) {
                paramIndex = 0;
              }
              else {
                paramIndex += 1;
              }
            }
            else if (newPos < pos) {
              if ((paramIndex - 1) < 0) {
                paramIndex = (curParamIndexLimit - 1);
              }
              else {
                paramIndex -= 1;
              }
            }
            curParamVal = curParamVals[paramIndex];
            menu.update();
          }
          pos = newPos;
        }
        break;
      } 

    case CREATESYNTHS: {
        if ((ctrlsCnt == 0) || (currSongName == "")) {
          lcd.clear();
          lcd.print("Choose # of ");
          lcd.setCursor(0, 1);
          lcd.print("ctrls first");
          state = INIT;
        }
        else {
          start_address = SONGSSTARTADDRESS + paramIndex * sizeof(Song);
          byte * b = store.readAddress(start_address); // byte array which is read from flash at adress SONGSSTARTADDRESS
          Song songFromFlash; // create a temporary struct
          memcpy(&songFromFlash, b, sizeof(Song)); // copy byte array to temporary struct

          byte voicesCnt = MAXVOICENUM / ctrlsCnt;
          byte n = 0;
          for (byte i = 0; i < ctrlsCnt; i++) {
            if ((MAXVOICENUM % ctrlsCnt != 0) && (i == (ctrlsCnt - 1)))   // if ctrlsCnt is odd number && this is last subSynth
              voicesCnt = MAXVOICENUM - (voicesCnt * i);            // then voicesCnt == rest of available voices

            byte startVoiceId = n;
            //byte stopVoiceId = n + voicesCnt;
            //n = stopVoiceId;
            n += voicesCnt;

            subSynths[i] = new SubSynth(voicesCnt, i, startVoiceId, songFromFlash);
            subSynths[i]->sendVoicesCnt();
            delay(SENDDELAY);
            subSynths[i]->sendInitTempo();
            delay(SENDDELAY);
            for (byte j = 0; j < voicesCnt; j++) {
              subSynths[i]->sendInitSeq(j);
              delay(SENDDELAY);
              subSynths[i]->sendInitParams(j);
            }
            subSynths[i]->sendStart();
          }

          lcd.clear();
          lcd.print("We can start!");
          lcd.setCursor(0, 1);
          lcd.print(currSongName);
          pos = 0;
          last_tempo = millis();
          state = RUN;
        }
        break;
      }

    case RUN: {
        leftButtState = digitalRead(LEFTBUTT);
        rightButtState = digitalRead(RIGHTBUTT);

        if (rightButtState != rightButtLastState) {
          if (playing) {          
          for (byte i = 0; i < ctrlsCnt; i++)
            subSynths[i]->sendStop();

          playing = false;
          stopSound();
          state = INIT;
          lcd.clear();
          lcd.print("SynthSeq menu");
          return;

          }
          else {
            playing = true;
            //Serial.println("aaaaxxx");            
            playSound();
            ////Serial.println("bbbbxxx");            
          }          
          rightButtLastState = rightButtState;          
        }

        if (leftButtState != leftButtLastState) {
          if (playing) {
            Song song;
            song.name = currSongName;
            byte voicesCnt = MAXVOICENUM / ctrlsCnt;
            byte k = 0;
            for (byte i = 0; i < ctrlsCnt; i++) {
              if ((MAXVOICENUM % ctrlsCnt != 0) && (i == (ctrlsCnt - 1)))     // if ctrlsCnt is odd number && this is last subSynth
                voicesCnt = MAXVOICENUM - (voicesCnt * i);                    // then voicesCnt == rest of available voices
              for (byte j = 0; j < voicesCnt; j++) {
                subSynths[i]->getVoiceSeqParams(j, song.voiceSeqParams[k]);
                k++;
              }
            }

            byte b[sizeof(Song)];
            memcpy(b, &song, sizeof(Song));
            store.write(start_address, b, sizeof(Song));
//            lcd.print("Song ");
//            lcd.print(song.name);
//            lcd.print(" saved");

          //Serial.println("aaaa");
          }
          else {
            //Serial.println("bbbb");
          }
          leftButtLastState = leftButtState;          
        }

        encoder.tick();
        newPos = encoder.getPosition();           
        if (pos != newPos) {
          if (newPos > pos) {
            if ((masterVolume + 10) > 100) {
              masterVolume = 10;
            }
            else {
              masterVolume += 10;
            }
          }
          else if (newPos < pos) {
            if ((masterVolume - 10) < 0) {
              masterVolume = 0;
            }
            else {
              masterVolume -= 10;
            }
          }
          pos = newPos;
        }

        if (masterVolume == 100) {
          lcd.setCursor(13, 1);
        }
        else {
          lcd.setCursor(13, 1);
          lcd.print(" ");
          lcd.setCursor(14, 1);
        }
        lcd.print(masterVolume);

        for (byte i = 0; i < ctrlsCnt; i++)
          subSynths[i]->receiveMidi();

        if (playing) {
          if ((millis() - last_tempo) > (tempo * 100)) {
            for (byte i = 0; i < ctrlsCnt; i++) {
              subSynths[i]->sendTick();
              subSynths[i]->sequencer();
              //Serial.println("playing");
            }

            last_tempo = millis();
          }
        }
        break;
      }
    case STOP: {
      for (byte i = 0; i < ctrlsCnt; i++)
        subSynths[i]->sendStop();
      lcd.clear();
      lcd.print("SynthSeq menu");      
      state = INIT;
      }
  }
}
