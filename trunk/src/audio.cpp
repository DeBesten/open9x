/*
 * Author - Rob Thomson <rob@marotori.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "open9x.h"

audioQueue::audioQueue()
{
  aqinit();
}

void audioQueue::aqinit()
{
  //make sure haptic off by default
  HAPTIC_OFF;

  toneTimeLeft = 0;
  tonePause = 0;

  t_queueRidx = 0;
  t_queueRidx = 0;

#ifdef HAPTIC
  hapticTick = 0;
#endif

#ifdef FRSKY
  frskySample = 0;
#endif    

}

bool audioQueue::busy()
{
  return (toneTimeLeft > 0);
}

/* TODO to be added when needed
bool audioQueue::freeslots(uint8_t slots)
{
  return AUDIO_QUEUE_LENGTH - ((t_queueWidx + AUDIO_QUEUE_LENGTH - t_queueRidx) % AUDIO_QUEUE_LENGTH) >= slots;
}
*/

//heartbeat is responsibile for issueing the audio tones and general square waves
// it is essentially the life of the class.
void audioQueue::heartbeat()
{
#if defined(PCBSTD)
  if (toneTimeLeft > 0) {
    //square wave generator use for speaker mod
    //simply generates a square wave for toneFreq for
    //as long as the toneTimeLeft is more than 0
    static uint8_t toneCounter;
    toneCounter += toneFreq;
    if ((toneCounter & 0x80) == 0x80)
      PORTE |= (1 << OUT_E_BUZZER); // speaker output 'high'
    else
      PORTE &= ~(1 << OUT_E_BUZZER); // speaker output 'low'
  }

  static uint8_t cnt10ms = 77; // execute 10ms code once every 78 ISRs
  if (cnt10ms-- == 0) // every 10ms ...
    cnt10ms = 77;
  else
    return;
#endif

  if (toneTimeLeft > 0) {
#if defined(PCBV4)
    OCR0A = (5000 / toneFreq); // sticking with old values approx 20(abs. min) to 90, 60 being the default tone(?).
    TCCR0A |= (1<<COM0A0);  // tone on
#endif
    toneTimeLeft--; //time gets counted down
    toneFreq += toneFreqIncr; // -2, 0 or 2
  }
  else {
#if defined(PCBV4)
    TCCR0A &= ~(1<<COM0A0);  // tone off
#else
    PORTE &= ~(1 << OUT_E_BUZZER); // speaker output 'low'
#endif
    if (tonePause-- <= 0) {
      if (t_queueRidx != t_queueWidx) {
        toneFreq = queueToneFreq[t_queueRidx];
        toneTimeLeft = queueToneLength[t_queueRidx];
        toneFreqIncr = queueToneFreqIncr[t_queueRidx];
        if (queueToneRepeat[t_queueRidx]--) {
          t_queueRidx = (t_queueRidx + 1) % AUDIO_QUEUE_LENGTH;
        }
      }
    }
  }
}

#ifdef HAPTIC
    uint8_t hapticStrength = g_eeGeneral.hapticStrength;
    if (toneHaptic == 1) {
      if ((hapticTick <= hapticStrength - 1) && hapticStrength > 0) {
        HAPTIC_ON; // haptic output 'high'
        hapticTick++;
      }
      else {
        HAPTIC_OFF; //haptic output low
        hapticTick = 0;
      }
    }
    else {
      HAPTIC_OFF; // haptic output 'low'
    }
#endif

inline uint8_t audioQueue::getToneLength(uint8_t tLen)
{
  uint8_t result = tLen; // default
  if (g_eeGeneral.beeperVal == 2) {
    result /= 2;
  }
  else if (g_eeGeneral.beeperVal == 3) {
    result = (result * 3) / 2;
  }
  else if (g_eeGeneral.beeperVal == 5) {
    //long
    result *= 2;
  }
  else if (g_eeGeneral.beeperVal == 6) {
    //xlong
    result *= 3;
  }
  return result;
}

void audioQueue::playNow(uint8_t tFreq, uint8_t tLen, uint8_t tPause,
    uint8_t tRepeat, uint8_t tHaptic, int8_t tFreqIncr)
{
  if (g_eeGeneral.beeperVal) {
    toneFreq = tFreq + g_eeGeneral.speakerPitch + BEEP_OFFSET; // add pitch compensator
    toneTimeLeft = getToneLength(tLen);
    tonePause = tPause;
  #ifdef HAPTIC
    t_queueToneHaptic = tHaptic;
  #endif
    toneFreqIncr = tFreqIncr;
    t_queueWidx = t_queueRidx;

    if (tRepeat) {
      playASAP(tFreq, tLen, tPause, tRepeat-1, tHaptic, tFreqIncr);
    }
  }
}

void audioQueue::playASAP(uint8_t tFreq, uint8_t tLen, uint8_t tPause,
    uint8_t tRepeat, uint8_t tHaptic, int8_t tFreqIncr)
{
  if (g_eeGeneral.beeperVal) {
    queueToneFreq[t_queueWidx] = tFreq + g_eeGeneral.speakerPitch + BEEP_OFFSET; // add pitch compensator
    queueToneLength[t_queueWidx] = getToneLength(tLen);
    queueTonePause[t_queueWidx] = tPause;
#ifdef HAPTIC
    queueToneHaptic[t_queueWidx] = tHaptic;
#endif
    queueToneRepeat[t_queueWidx] = tRepeat;
    queueToneFreqIncr[t_queueWidx] = tFreqIncr;

    t_queueWidx = (t_queueWidx + 1) % AUDIO_QUEUE_LENGTH;
  }
}

#ifdef FRSKY

//this is done so the menu selections only plays tone once!
void audioQueue::frskyeventSample(uint8_t e)
{
  if(frskySample != e) {
    aqinit(); //flush the queue
    frskyevent(e);
    frskySample = e;
  }
}

void audioQueue::frskyevent(uint8_t e)
{
  // example playASAP(tStart,tLen,tPause,tRepeat,tHaptic,tEnd);
  switch(e) {
    case AU_FRSKY_WARN1:
    playASAP(BEEP_DEFAULT_FREQ+20,25,5,2,1);
    break;
    case AU_FRSKY_WARN2:
    playASAP(BEEP_DEFAULT_FREQ+30,25,5,2,1);
    break;
    case AU_FRSKY_CHEEP:
    playASAP(BEEP_DEFAULT_FREQ+30,20,2,2,1,BEEP_DEFAULT_FREQ+25);
    break;
    case AU_FRSKY_RING:
    playASAP(BEEP_DEFAULT_FREQ+25,2,2,10,1);
    playASAP(BEEP_DEFAULT_FREQ+25,2,10,1,1);
    playASAP(BEEP_DEFAULT_FREQ+25,2,2,10,1);
    break;
    case AU_FRSKY_SCIFI:
    playASAP(80,4,3,2,0,70);
    playASAP(60,4,3,2,0,70);
    playASAP(70,2,1,0,2);
    break;
    case AU_FRSKY_ROBOT:
    playASAP(70,2,1,1,1);
    playASAP(50,6,2,1,1);
    playASAP(80,6,2,1,1);
    break;
    case AU_FRSKY_CHIRP:

    playASAP(BEEP_DEFAULT_FREQ+40,2,1,2,1);
    playASAP(BEEP_DEFAULT_FREQ+54,2,1,3,1);
    break;
    case AU_FRSKY_TADA:
    playASAP(50,10,5);
    playASAP(90,10,5);
    playASAP(110,6,4,2);
    break;
    case AU_FRSKY_CRICKET:
    playASAP(80,1,10,3,1);
    playASAP(80,1,20,1,1);
    playASAP(80,1,10,3,1);
    break;
    case AU_FRSKY_SIREN:
    playASAP(10,5,5,2,1,40);
    break;
    case AU_FRSKY_ALARMC:
    playASAP(50,5,10,2,1);
    playASAP(70,5,20,1,1);
    playASAP(50,5,10,2,1);
    playASAP(70,5,20,1,1);
    break;
    case AU_FRSKY_RATATA:
    playASAP(BEEP_DEFAULT_FREQ+50,2,10,10,1);
    break;
    case AU_FRSKY_TICK:
    playASAP(BEEP_DEFAULT_FREQ+50,2,50,2,1);
    break;
    case AU_FRSKY_HAPTIC1:
    playASAP(0,2,10,1,1);
    break;
    case AU_FRSKY_HAPTIC2:
    playASAP(0,2,10,2,1);
    break;
    case AU_FRSKY_HAPTIC3:
    playASAP(0,2,10,3,1);
    break;
    default:
    break;
  }

}
#endif

void audioQueue::event(uint8_t e, uint8_t f) {

  uint8_t beepVal = g_eeGeneral.beeperVal;

  switch (e) {
    //startup tune
    // case 0:
    case AU_TADA:
      playASAP(50, 10, 5);
      playASAP(90, 10, 5);
      playASAP(110, 6, 4, 2);
      break;

      //warning one
      // case 1:
    case AU_WARNING1:
      playNow(BEEP_DEFAULT_FREQ, 25, 1, 0, 1);
      break;

      //warning two
      //case 2:
    case AU_WARNING2:
      playNow(BEEP_DEFAULT_FREQ, 34, 1, 0, 1);
      break;

      //warning three
      //case 3:
    case AU_WARNING3:
      playNow(BEEP_DEFAULT_FREQ, 15, 1, 0, 1);
      break;

      //error
      //case 4:
    case AU_ERROR:
      playNow(BEEP_DEFAULT_FREQ, 30, 1, 0, 1);
      break;

      //keypad up (seems to be used when going left/right through system menu options. 0-100 scales etc)
      //case 5:
    case AU_KEYPAD_UP:
      if (beepVal != BEEP_NOKEYS) {
        playNow(BEEP_KEY_UP_FREQ, 2, 1);
      }
      break;

      //keypad down (seems to be used when going left/right through system menu options. 0-100 scales etc)
      //case 6:
    case AU_KEYPAD_DOWN:
      if (beepVal != BEEP_NOKEYS) {
        playNow(BEEP_KEY_DOWN_FREQ, 2, 1);
      }
      break;

      //trim sticks move
      //case 7:
    case AU_TRIM_MOVE:
      //if(beepVal != BEEP_NOKEYS){
      playNow(f, 2, 1);
      //}
      break;

      //trim sticks center
      //case 8:
    case AU_TRIM_MIDDLE:
      //if(beepVal != BEEP_NOKEYS){
      playNow(BEEP_DEFAULT_FREQ, 10, 2, 0, 1);
      //}
      break;

      //menu display (also used by a few generic beeps)
      //case 9:
    case AU_MENUS:
      if (beepVal != BEEP_NOKEYS) {
        playNow(BEEP_DEFAULT_FREQ, 2, 2, 0, 1);
      }
      break;
      //pot/stick center
      //case 10:
    case AU_POT_STICK_MIDDLE:
      playNow(BEEP_DEFAULT_FREQ + 50, 3, 1, 0, 1);
      break;

      //mix warning 1
      //case 11:
    case AU_MIX_WARNING_1:
      playNow(BEEP_DEFAULT_FREQ + 50, 2, 1, 1, 1);
      break;

      //mix warning 2
      //case 12:
    case AU_MIX_WARNING_2:
      playNow(BEEP_DEFAULT_FREQ + 52, 2, 1, 2, 1);
      break;

      //mix warning 3
      //case 13:
    case AU_MIX_WARNING_3:
      playNow(BEEP_DEFAULT_FREQ + 54, 2, 1, 3, 1);
      break;

      //time 30 seconds left
      //case 14:
    case AU_TIMER_30:
      playNow(BEEP_DEFAULT_FREQ + 50, 5, 3, 3, 1);
      break;

      //time 20 seconds left
      //case 15:
    case AU_TIMER_20:
      playNow(BEEP_DEFAULT_FREQ + 50, 5, 3, 2, 1);
      break;

      //time 10 seconds left
      //case 16:
    case AU_TIMER_10:
      playNow(BEEP_DEFAULT_FREQ + 50, 5, 3, 1, 1);
      break;

      //time <3 seconds left
      //case 17:
    case AU_TIMER_LT3:
      playNow(BEEP_DEFAULT_FREQ, 20, 5, 1, 1);
      break;

      //inactivity timer alert
      //case 18:
    case AU_INACTIVITY:
      playASAP(70, 3, 2);
      playASAP(50, 3, 5);
      break;

      //low battery in tx
      //case 19:
    case AU_TX_BATTERY_LOW:
      playASAP(60, 4, 3, 2, 1, 2);
      playASAP(80, 4, 3, 2, 1, -2);
      break;

    default:
      break;
  }
}

void audioDefevent(uint8_t e) {
  audio.event(e, BEEP_DEFAULT_FREQ);
}

