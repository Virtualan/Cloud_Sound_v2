/*
  ATTiny85 Midi Jazz Machine - by Alan Smith
*/

#include <SendOnlySoftwareSerial.h>
SendOnlySoftwareSerial midiSerial (0); // Tx pin

const byte  MidiChannels = 16 , offSetPin = 3, tempoPin = 2, LDRPin = 1,
            senstivity = 1, arraySize = 32, threeOctaves = 36, twoOctaves = 24,
            tunings[5] = {40, 52, 64, 76, 88};

unsigned long bassSpeed = 0, melodySpeed = 0, tuneSpeed = 0,
              melodyStamp = 0, timeStamp = 0, chordSpeed = 0,
              melodyTimer = 0, chordTimer = 0, bassTimer = 0, drumTimer = 0;

int  oldLightLevel = 0, lightChange = 5, lightLevel = 0;

byte bdn = 48, bdnm = 48, bdnc = 0, bdncm = 0 , hhn = 48, hhnm = 48,
     hhnc = 0, hhncm = 0, chordPulse = 4, groove = 4, chordChannel = 2,
     melodyChannel = 1 , bassChannel = 0, bassRange = arraySize,
     chordRange = arraySize, playCtrl = 0xFF,// playCrtl bits - kick on|snare on|bd note|hh note|beat note|melody|chords|bass
     melodyRange = arraySize, chordMult = 7, melodyMult = 5,
     drum = 35, bassMult = 4, onmm = 0, onm = 0, bnm = 0,
     bnc = 0, ncm = 0, nnm = 0, cht = 0, chtb = 0,
     chc = 0, chn = 0, nm = 0, cm = 0,
     progRange = 119, basicChords[3] = {0, 1, 6};


byte vels[MidiChannels], melodyBank[arraySize] , chanChord[MidiChannels];

char melDir = 0,  keys[8], key = 3;// keyMem = 3;

unsigned int  chordPattern = random(0xFFFFu) ,
              playPattern = random(0xFFFFu) ,
              melodyPattern = random(0xFFFFu) ,
              bassPattern = random(0xFFFFu) ;

unsigned int scales[10] = {
  2773U, 2708U, 2418U , 2906U, 2901U, 2905U, 2925U, 2733U, 3290U,  2730U
};
unsigned int loopCount = 1, bassIndex = 1, drumIndex = 1,
             ctIndex = 0, scaleIndex = 0,
             chordIndex = 1, melodyIndex = 1, scale = 0x2773U;

unsigned int chords[17] = {
  0x8000U,
  0x8900U, 0x8940U, 0x8920U, 0x8910U, 0x8908U, //maj chords
  0x9100U, 0x9140U, 0x9120U, 0xB128U, // min chords
  0x9200U, 0x8880U, 0xA100U, 0x8500U, 0xA120U,
  0x8520U,  0x9970U
}, chord = 0x8000U;

struct midiMessage {
  byte n = 0;
  byte p = 65;
  byte c = 9;
};

//above is the basic midimessage.
//n = note,
//p = (if its playing or not, the vcf filter setting),
//c = the midi channel, TODO - still have the top nibble to use for something

//create the space, chew up the bytes, go for a p, and think "what more can I do?"
struct midiMessage m[arraySize];
byte chordTrack[arraySize] ;
byte mas = arraySize;

//we now set up the ATTiny85's ports to do what you demand of the little silicon bugger
void setup() {
  pinMode(2, INPUT); //the tuneSpeed speed pot - attiny85 pin 7
  pinMode(3, INPUT); //the note/lightLevel offset pot - attiny85 pin 2
  pinMode(1, OUTPUT);//output for the beat/activity LED - attiny85 pin 6
  pinMode(4, INPUT); //the LDR input for the lightLevel - attiny85 pin 3
  midiSerial.begin(31250); // Start serial port at the midi 31250 baud - out on attiny85 pin 5
  gsReset();  // reset the Boss DR-330 synth and switch to multitimberal mode
  delay(100); //GS Reset needs a 50ms delay

  //  //Load up the array with some starting notes, programs and channels
  //  for (byte x = 0; x < mas; x++) {
  //    m[x].n = (byte)(analogRead(LDRPin) * random(24, 80));
  //    m[x].c = randomChannel();
  //    ProgChange(m[x].c, random(119));
  //    delay(10);
  //    melodyBank[x] = (byte)(analogRead(LDRPin) * random(24, 80));
  //    for (byte y = 0; y < m[x].n; y += 3) {
  //      NoteOn(m[x].c, y, 50);
  //      delay(5);
  //      NoteOff(m[x].c, y);
  //    }
  //  }


} // end of setup

// ok here we go for a trip.....

void loop() {    // the main billion times a second loop

  // set the various array pointers

  byte ki = (byte)((chordIndex / bassMult)  % 8);
  byte di = (byte)(drumIndex % mas);
  byte bi = (byte)(bassIndex % bassRange);
  byte chi = (byte)(chordIndex % chordRange);
  byte ci = (byte)(loopCount % mas);
  byte vi = (byte)(drumIndex % MidiChannels);

  // get sensor data every loop
  int offset = (map(analogRead(offSetPin), 0, 1023, 1, 4));   // aux controller ??? - timing multiplier 0,2x,4x,8x
  tuneSpeed = (unsigned long)(map(analogRead(tempoPin), 0, 1023, 50, 600) - (lightLevel >> 2));  // the main tempo control
  bassSpeed = (unsigned long)(tuneSpeed * bassMult);
  melodySpeed = (unsigned long)((tuneSpeed * melodyMult) / offset);
  chordSpeed = (unsigned long)(tuneSpeed * chordMult);

  // set sync timer
  if (millis() > timeStamp) {
    timeStamp = (unsigned long)millis() + tuneSpeed; // we reset the non drum timer so they sync with the drums
  }

  // Don't look at the light!
  lightLevel = analogRead(LDRPin);  // read the LDR value
  lightChange = 0;  // reset lightChange

  if (abs(oldLightLevel - lightLevel) > senstivity) {
    lightChange = (int)constrain((lightLevel - oldLightLevel), -12, 12) ; // measure the change in the lightLevel
    oldLightLevel = lightLevel;
    keys[ctIndex % 8] += lightChange;
    ctIndex++;
    key = keys[ctIndex % 8];
    if (lightChange > 0) {        //major chord shapes
      chordTrack[ctIndex % mas] = random(1, 6); //byte(lightLevel % 17);
      m[ctIndex % mas].n++;
      scale = scales[random(3)];  //major scales
    }
    else if (lightChange < 0) {  //minor chord shapes
      chordTrack[ctIndex % mas] = random(6, 10); //byte(lightLevel % 17);
      m[ctIndex % mas].n--;
      scale = scales[random(3, 7)];  //minor scales
    }
  }
  else {
    chordTrack[ctIndex % mas] = 0 ;//(basicChords[random(3)]);  //none, major or minor
    melodyBank[ctIndex % mas] += Nudge();
  }


  //check all the timers

  //melody line - haha
  if ((timeStamp > melodyTimer)  ) {//
    if (bitRead(playCtrl, 2) && ((melodyPattern >> (ci % MidiChannels)) & 1U) ) {
      NoteOff(ncm , nnm );
      ncm = melodyChannel;// m[chordIndex % mas].c;//
      nnm += ((nnm < melodyBank[melodyIndex % melodyRange]) ?
              ScaleFilter(chords[chanChord[ncm]] >> 4, abs(Nudge()), key) :
              -(ScaleFilter(chords[chanChord[ncm]] >> 4, abs(Nudge()), key)));
      if (nnm == melodyBank[melodyIndex % melodyRange]) {
        melodyIndex++;
      }
      NoteOn(ncm , nnm, (random(60, 90))); //100***************************************Note on*************
    }
    // reset the timer
    melodyTimer = (unsigned long)((timeStamp - (tuneSpeed << 1)) + melodySpeed ) ;
  }

  //bass Line
  if ((timeStamp > bassTimer)  ) {//
    //bass melody
    if (bitRead(playCtrl, 0) && (bassPattern >> (ci % MidiChannels)) & 1U) {
      NoteOff(bassChannel, nm);
      nm = ScaleFilter
           (
             scale,
             keys[bi % 8] + chordTrack[bi] + (m[bi].n  % threeOctaves) + (melodyBank[bi] % threeOctaves),
             key
           ) ;
      bassIndex++;
      NoteOn(bassChannel, nm, vels[1]); //***************************************Note on*************
    }
    // reset the timer
    bassTimer = (unsigned long)(timeStamp + bassSpeed) ;
  }


  if ((timeStamp > chordTimer)  ) { //
    //chord stabs
    if (bitRead(playCtrl, 1) && (di % chordPulse == 0)
        && ( di % groove == 0
             || (chordPattern >> (ci % MidiChannels)) & 1U)) { //
      playChord(chords[cht], chc , chn , 0, 0);
      chn = ScaleFilter(scale, m[di % chordRange].n, key) ; //key + nnm;
      cht = chordTrack[chi];
      if (di % groove == 0) {
        chc = melodyChannel;
        cht = chanChord[melodyChannel];
        chn -= 12;
      }
      else {
        chc = m[di % chordRange].c;
      }
      if (cht > 0) {
        playChord(chords[cht], chc , chn , vels[2], 1); //***************************************Note on*************
        chordIndex++;
      }
    }
    // reset the timer
    chordTimer = (unsigned long)(timeStamp + (chordSpeed )) ;  //<< 1
  }


  //Drums
  if ((timeStamp > drumTimer)  ) { //  The main timed trigger to keep a constant tempo
    //  feed the channel chords
    chanChord[m[di].c] = (byte)(chordTrack[di]);
    chanChord[0] = (byte)(chordTrack[di]);

    // filter changes
    m[di].p += (char)(random(13) - 6);
    DoFilter(m[di].c, vels[5] % 70, m[di].p & 0x7F); //
    DoFilter(bassChannel, 80, m[di].p & 0x7F); //

    //the beat note /chords
    if (bitRead(playCtrl, 3) && (((bassPattern | melodyPattern) >> (di % MidiChannels)) & 1U) == 0) {
      NoteOff(bnc, bnm);
      bnm = ScaleFilter(scale, twoOctaves + melodyBank[di], key); //+ (m[di].n % twoOctaves) + (nm % twoOctaves) + (nnm % twoOctaves)
      bnc = m[di].c;
      NoteOn(bnc, bnm, random(60, 100) ); //***************************************Note on*************
    }

    //drums **************************
    if ((((playPattern | bassPattern) >> (di % MidiChannels)) & 1U)) { // //drumIndex % groove == 0 ||

      //digitalWrite(1, !(di % 4)); //running lightchange indicator LED
      if (bitRead(playCtrl, 6) && di % (groove << 2) == 0 ) { // snare  && random(4)
        drum = 38 + (random(2) * 2);
        NoteOn(9, drum, random(50, 110));//**********************drum note*************
        NoteOff(9, drum);
        NoteOff(hhncm, hhnm);
        hhn = ScaleFilter(scale, keys[di % 8] + melodyBank[di % melodyRange], key) ; // % vels[6]
        hhnc = m[di].c; //m[di].c; //random(MidiChannels); % vels[6]
        if (bitRead(playCtrl, 4)) {
          NoteOn(hhnc, hhn, vels[3]);    //***************************************Note on*************
          hhnm = hhn;
          hhncm = hhnc;
        }
      }

      else if (bitRead(playCtrl, 7) && (di % 4 == 0 || di % (groove << 1) == 0)) { // kick
        //analogWrite(1, 0);
        NoteOff(bdnc, bdnm);
        bdn = ScaleFilter(scale, keys[di % 8] + m[(bassIndex + melodyIndex) % mas].n, key); // % vels[7]
        bdnc = m[chordIndex % mas].c;//random(MidiChannels); % vels[7]
        if (bitRead(playCtrl, 5)) {
          NoteOn(bdnc, bdn, vels[4]);  //***************************************Note on*************
          bdnm = bdn;
          bdncm = bdnc;
        }
        drum = 35 ; // kick
        DoArticulations();  //used exclusively for kontakt
        NoteOn(9, drum, random(90, 120));//**********************drum note*************
        NoteOff(9, drum);
        CC(random(MidiChannels), 10, random(10, 120)); //pan
        //CC(random(MidiChannels), 1, random(1, 50)); //modulation
        CC(9, 10, random(60, 69)); //pan drums around centre -- very slight movement +/- 4
        CC(bassChannel, 10, random(54, 75)); //0 pan around centre -- slighty more movement +/- 10
        CC(melodyChannel, 10, random(44, 85)); //3 pan around centre -- slighty more movement +/- 10
        if (lightChange != 0) {
          //drum tunings
          CC(9, 0x63, 0x18);
          CC(9, 0x62, random(twoOctaves, 60));
          CC(9, 6, random(0x2e, 0x5f));

          //drum reverbs
          CC(9, 0x63, 0x1D);
          CC(9, 0x62, random(twoOctaves, 60));
          CC(9, 6, random(127));

          //drum chorus
          CC(9, 0x63, 0x1E);
          CC(9, 0x62, random(twoOctaves, 60));
          CC(9, 6, random(127));
        }
      }
      else if (di % bassMult == 0 && lightChange != 0) { // toms
        drum = 41 + (random(3) * 3);
        NoteOn(9, drum, 50);//**********************drum note*************
        NoteOff(9, drum);
        //melodyChannel = random(1, MidiChannels);
      }

      if (lightChange != 0 || di % (random(1, 4) << 1) == 0) { // hh
        drum = 42 + (random(3) * 2);
        NoteOn(9, drum, random(20, 60));//**********************drum note*************
        NoteOff(9, drum);
      }

      if (random(2) &&  drumIndex % (mas * 8) >= ((mas * 8) - (groove << 2))) { // wee end loop fills
        //melodyBank[di] += lightChange; // mix up the notes a little
        drum = 35 + (random(MidiChannels));
        NoteOn(9, drum, random(30, 80));//**********************drum note*************
        NoteOff(9, drum);
      }
    }
    else {
      drum = 35 + (lightLevel % 32);// ;
      NoteOn(9, drum, random(10, 40));//**********************drum note*************
      NoteOff(9, drum);
    }
    // change channel volumes
    if (vels[vi] > 64 && vels[vi] < 90) {
      vels[vi] += Nudge();
      if (vi != 9) {
        CC(vi, 7, vels[vi]);
      }
      else {
        CC( 9, 7, 60);
      }
    }
    else {
      vels[vi] = 70;
    }
    //    if ( drumIndex % int(mas * 8) == 0) {
    //      progRange ++;
    //      progRange %= 119;
    //    }
    // increment the 'timed loop' pointer
    drumIndex++;
    // reset the timer
    drumTimer = (unsigned long)millis() + tuneSpeed ;


  }// end of timed drum events

  // changes
  // eggs are thrown onto a hot plate and let's see what happens

  if (drumIndex % (int)((mas * 64)) == 0) {
    KillNotes();
    mas = byte(arraySize / (random(1, 3)));
    playCtrl = 0x01;
    bassMult = MultiplierThingy (8);
    ProgChange(bassChannel, random(33, 40));  // GM
    ProgChange(9, random(6) * 8);
    chordPattern = Pattern() ;
    melodyPattern = Pattern() ;
    melodyRange = randomArrayRange(8); //mas / random(1, 9);
    bassRange = randomArrayRange(8); //mas / random(1, 9);
    bassPattern = Pattern() ;
    chordMult = MultiplierThingy (4);

  }
  // the big periodic changes
  else if (drumIndex % int((mas * 32)) == 0) {
    KillNotes();
    scale = scales[random(10)];
    groove = random(2, 13);
    playPattern = Pattern() ;
    bassPattern = Pattern() ;
    chordPulse = random(1, 5);
    //drum sounds
    ProgChange(9, random(6) * 8);
    playCtrl = random(0xFF);
  }

  //important but subtle changes to the songs progression
  else if (drumIndex % (int)((mas * 8)) == 0) {
    KillNotes();
    melodyChannel = randomChannel();
    // change the envelope of the channel (DS330 Only)
    byte cc = random(16); //pick a channel - any channel not bass though
    if (cc != 9) {
      ADSR(cc, random(0x0e, 0x72), random(0x0e, 0x72), random(0x0e, 0x62));   // Channel - Attack, Decay, Release
      MasterTune(cc, tunings[random(5)]); // -twoOctaves,-12,0,12,twoOctaves (octaves)   40 - 88
    }
    //drums - only move slightly
    else {
      MasterTune(cc, random(60, 69));
    }
    ///production and rhythm dynamics
    CC(cc, 0x5B, random(127)); // reverb send
    CC(cc, 0x5D, random(127)); // chorus send
    ProgChange(randomChannel(), random(progRange)  );
    m[random(mas)].c = randomChannel();
    //bass sounds changed
    chordPulse = random(1, 5);
    ProgChange(bassChannel, random(33, 40));
    m[random(mas)].c = randomChannel();
    bassMult = MultiplierThingy (4);
    melodyMult = MultiplierThingy (8) ;
    melodyPattern = Pattern() ;
    playPattern = Pattern() ;
    bassRange = randomArrayRange(MidiChannels); //mas / random(1, 9);
    chordRange = randomArrayRange(8); //mas / random(1, 9);
    chordChannel = randomChannel();
    ProgChange(9, random(6) * 8);
    playCtrl |= random(0xFF);
  }

  //check for range of notes

  if (keys[ki] > 17 || keys[ki] < -17) {  // key is also used as an array pointer for chords
    keys[ki] = 0;
  }
  if (m[ci].n > 70 || m[ci].n < twoOctaves) {
    m[ci].n = threeOctaves + (lightLevel % threeOctaves);
    m[ci].c = randomChannel();
  }
  if (melodyBank[ci] > 70 || melodyBank[ci] < twoOctaves) {
    melodyBank[ci] = threeOctaves + (lightLevel % 48);
  }

  m[ci].n  = ScaleFilter(chords[chanChord[m[ci].c]] >> 4, m[ci].n, key);
  melodyBank[ci] = ScaleFilter(chords[chanChord[melodyChannel]] >> 4, melodyBank[ci], key);

  //    while (melodyBank[ci] != ScaleFilter(chords[chanChord[melodyChannel]] >> 4, melodyBank[ci], key)) {
  //      melodyBank[ci] += Nudge();
  //    }
  //    while (melodyBank[ci] != ScaleFilter(scale, melodyBank[ci], key)) {
  //      melodyBank[ci] += Nudge();
  //    }
  //
  //    while (m[ci].n != ScaleFilter(chords[chanChord[m[ci].c]] >> 4, m[ci].n, key)) {
  //      m[ci].n += Nudge();
  //    }
  //
  //  while (m[ci].n != ScaleFilter(scale, m[ci].n, key)) {
  //    m[ci].n += Nudge();
  //  }

  analogWrite(1, abs(lightChange) * 30); //running lightchange indicator LED

  // keep melody lines moving
  //  while (melodyBank[ci] == melodyBank[(ci - 1) % mas]) {
  //    melodyBank[ci] += Nudge(); //
  //  }

  //  while (m[ci].n == m[(ci - 1) % mas].n) {
  //    m[ci].n += Nudge(); //
  //  }

  // keep the channels moving
  //  while ((m[ci].c == m[(ci + 1) % mas].c) || m[ci].c == 9) {
  //    m[ci].c = randomChannel();  //but not bass
  //  }

  // rinse and repeat
  loopCount++;
} // end of main loop

//helpers///////////////////////////////////////////////////////////////////////////////

//diagnostic cowbell - only used when required or Christopher Walken demands it!!
void DCB() {
  NoteOn(9, 56, 120);
  NoteOff(9, 56);
}

byte randomChannel() {
  byte randChan = random(1, MidiChannels);
  while (randChan == 9 ) { // not bass or drums
    randChan = random(1, MidiChannels);
  }
  return randChan;
}

byte randomArrayRange(byte factor) {
  return (byte)(mas / random(1, factor));
}

byte MultiplierThingy (byte scope) {
  return (byte)(random(1, scope) * 2);
}

unsigned int Pattern() {
  return (unsigned int) random(10, 0xFFFFU) ;
}

char Nudge() {
  char rr[8] = { -4, -3, -2, -1, 1, 2, 3, 4};
  return (char) rr[random(8)]; //
}

void KillNotes() {
  for (byte g = 0; g < MidiChannels; g++) {
    //if (random(2)) {
    CC(g, 123, 0); // all notes off on all channels. Happens so fast no one will notice?
    //}
  }
}

// send note on stuff
void NoteOn(byte chan, byte note, byte vel) {
  //if (note > twoOctaves && note < 90) {
  chan &= 0x0F;
  midiSerial.write((chan + 0x90));
  midiSerial.write(note & 0x7F);
  midiSerial.write(vel & 0x7F);
  //}
}
// send note off stuff
void NoteOff(byte chan, byte note) {
  chan &= 0x0F;
  midiSerial.write((chan + 0x80));
  midiSerial.write(note & 0x7F);
  midiSerial.write(byte(0));
}

byte ScaleFilter(unsigned int s, byte n, int k) {
  byte v = 0;
  s <<= 4;
  while ((((s << ((n  + k ) % 12)) & 0x8000U) != 0x8000U) && v < 12) {
    v++; n++;
  }
  if (v < 12) {
    return n;
  }
  else {
    return n - v;
  }
}

//chord plays the chord shape
void playChord(unsigned int cord, byte chan, byte note, byte vel, byte cont) {
  // cont is either play or kill
  //chan &= 0x0f;
  for (byte c = 0; c < 15; c++) {
    if (((cord << c) & 0x8000U) > 0U ) {
      if (cont) {
        NoteOn(chan, note + c  , vel);
      }
      else {
        NoteOff(chan, note + c  );
      }
      if (cord == 0x8000U) { // no point going any further, it is a single note
        break;
      }
    }
  }
}

//specificaly NRPN for BOSS DR-Synth 330 Channel Tuning (octaves apart from Drums)
void MasterTune(byte chan, byte b) {
  chan &= 0x0F;
  CC(chan, 0x65, 0);
  CC(chan, 0x64, 2);
  CC(chan, 6, b);
}

//specificaly NRPN for BOSS DR-Synth 330 ADSR settings
void ADSR(byte chan, byte a, byte d, byte r) {
  chan &= 0x0F;
  //Attack
  CC(chan, 0x63, 0x01);
  CC(chan, 0x62, 0x63);
  CC(chan, 6, a);
  //decay
  CC(chan, 0x63, 0x01);
  CC(chan, 0x62, 0x64);
  CC(chan, 6, d);
  //release
  CC(chan, 0x63, 0x01);
  CC(chan, 0x62, 0x66);
  CC(chan, 6, r);
}

//Basic Channel control message
void CC(byte chan, byte cont, byte val) {
  chan &= 0x0F;
  midiSerial.write((chan + 0xB0)) ;
  midiSerial.write(cont & 0x7F);
  midiSerial.write(val & 0x7F);
}

//specificaly NRPN for BOSS DR-Synth 330
void DoFilter(byte ch, byte res, byte coff) {
  ch &= 0x0F;
  CC(ch, 0x63, 0x01);
  CC(ch, 0x62, 0x21);
  CC(ch, 6, res & 0x7F); //resonance can go to 0x72
  CC(ch, 0x63, 0x01);
  CC(ch, 0x62, 0x20);
  CC(ch, 6, coff & 0x7F);//cut off frequency
}

// for Native Instruments Kontakt
void DoArticulations() {
  byte articnote = random(23, 127);
  byte rcc = random(MidiChannels);
  NoteOn(rcc, articnote, 1 );
  NoteOff(rcc, articnote);
}

//specificaly sysex for BOSS DR-Synth DS330
void gsReset() {
  byte gs[11] = {0xF0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7F, 0x00, 0x41, 0xF7};
  for (byte g = 0; g < 11; g++) {
    midiSerial.write(gs[g]) ;
    //randomSeed(analogRead(1) + analogRead(2) + analogRead(3));
  }
}

// Program change for a given midi channel
void ProgChange(byte chan, byte prog) {
  chan &= 0x0F;
  midiSerial.write((chan + 0xC0));
  midiSerial.write(prog & 0x7F);
}

/*Whoa! We are at the end of all the code
  Sketch uses 7980 bytes (97%) of program storage space. Maximum is 8192 bytes.
  Global variables use 424 bytes (82%) of dynamic memory. Maximum is 512 bytes.
*/




