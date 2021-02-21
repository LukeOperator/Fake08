#include "daisysp.h"
#include "daisy_field.h"


#define UINT32_MSB 0x80000000U
#define MAX_LENGTH 32U

using namespace daisy;
using namespace daisysp;

//we've got 8 knobs
#define NUM_CONTROLS 8
#define NUM_MODES 5

//every drum has 4 parameters, global controls are mode and tempo
Parameter p_Pitch, p_Dec, p_Fx, p_Amp, p_mode, p_tempo;

//0 = pitch, 1 = decay, 2 = FX, 3 = amp
float params[NUM_MODES][4]; 

float kvals[NUM_CONTROLS];
float kold[NUM_CONTROLS];


//parameter multipliers and offsets for 0 - 1 range
//for example, the kick pitch runs from 50 - 150, so mPitch is 100 (0-100), and oPitch is +50 (50-150)

float mPitch[] = {100.0f, 2000.0f, 4000.0f, 4000.0f, 200.0f};
float mDec[] = {0.2f, 0.5f, 0.5f, 1.0f, 0.5f};

float oPitch[] = {100.0f, 1000.0f, 3000.0f, 500.0f, 100.0f};
float oDec[] = {0.01f, 0.1f, 0.05f, 0.1f, 0.1f};

float trig[] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

//We have one Daisy Field
DaisyField hardware;

//there are 16 buttons
uint8_t buttons[16];

//keyboard LEDS
size_t keyboard_leds[] = {
        hardware.LED_KEY_A1,
        hardware.LED_KEY_A2,
        hardware.LED_KEY_A3,
        hardware.LED_KEY_A4,
        hardware.LED_KEY_A5,
        hardware.LED_KEY_A6,
        hardware.LED_KEY_A7,
        hardware.LED_KEY_A8,
        hardware.LED_KEY_B1,
        hardware.LED_KEY_B2,
        hardware.LED_KEY_B3,
        hardware.LED_KEY_B4,
        hardware.LED_KEY_B5,
        hardware.LED_KEY_B6,
        hardware.LED_KEY_B7,
        hardware.LED_KEY_B8,
  };

//We have 3 white noise generator
WhiteNoise noise[3];

//5 attack / decay envelopes
AdEnv     ampEnv[NUM_MODES];
AdEnv     pitchEnv[NUM_MODES];
Oscillator osc[NUM_MODES];

//filter for the hat
Biquad flt, sn, kck;


//metronome
Metro     tick;

//the value of which step in the sequence we're on
uint8_t Step  = 0;

//use this to change pages
uint8_t mode = 0;
uint8_t oldMode = 0;

//2D array of no. of drums x no. of steps
bool    Seq[NUM_MODES][MAX_LENGTH];

//default tempo of 3 Hz
float tempo = 3;


//function to process each step in a sequence
void ProcessTick();
//function to process changes in sequence
void ProcessControls();


//TODO design more interesting sounds. KS for kick drum is a bit too toned
//e.g. kick should be a snap of noise into res filter like early MUTE stuff. Dev in Max first
void Kick()
{
  trig[0] = 1.0f;
  ampEnv[0].Trigger();
}
void Snare()
{
  ampEnv[1].Trigger();
  pitchEnv[1].Trigger();
  trig[1] = 1.0f;
}
void Hat()
{
  //high hat in here
  ampEnv[2].Trigger();
  ampEnv[2].Trigger();
  trig[2] = 1.0f;
}
void Laser()
{
  //pew pew
  ampEnv[3].Trigger();
  pitchEnv[3].Trigger();
  trig[3] = 1.0f;
}
void Tom()
{
  //Tom goes here
  ampEnv[4].Trigger();
  pitchEnv[4].Trigger();
  trig[4] = 1.0f;
}
