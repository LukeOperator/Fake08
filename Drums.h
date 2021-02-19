#include "daisysp.h"
#include "daisy_field.h"


#define UINT32_MSB 0x80000000U
#define MAX_LENGTH 32U

using namespace daisy;
using namespace daisysp;

//we've got 8 knobs
#define NUM_CONTROLS 8
#define NUM_MODES 5

float kvals[NUM_CONTROLS];

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

//We have 3 osc and 2 white noise generator
Oscillator osc[3];
WhiteNoise noise[2];

//5 attack / decay envelopes
AdEnv     ampEnv[NUM_MODES];
AdEnv     pitchEnv[NUM_MODES];

//filter for the hat
Svf flt;

//metronome
Metro     tick;

//2D array of no. of drums x no. of steps
bool    Seq[NUM_MODES][MAX_LENGTH];

//default tempo of 3 (assuming Hz)
float tempo = 3;


//function to process each step in a sequence
void ProcessTick();
//function to process changes in sequence
void ProcessControls();


//TODO design more interesting sounds
//e.g. kick should be a snap of noise into res filter like early MUTE stuff. Dev in Max first
void Kick()
{
  ampEnv[0].Trigger();
  pitchEnv[0].Trigger();
}
void Snare()
{
  ampEnv[1].Trigger();
}
void Hat()
{
  //high hat in here
  ampEnv[2].Trigger();
}
void Laser()
{
  //pew pew
  ampEnv[3].Trigger();
  pitchEnv[3].Trigger();
}
void Tom()
{
  //Tom goes here
  ampEnv[4].Trigger();
}
