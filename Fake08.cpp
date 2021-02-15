#include "daisysp.h"
#include "daisy_field.h"

#define UINT32_MSB 0x80000000U
#define MAX_LENGTH 16U

using namespace daisy;
using namespace daisysp;

//we've got 8 knobs
#define NUM_CONTROLS 8

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

//We have 1 osc and 1 white noise generator
Oscillator osc;
WhiteNoise noise;

//3 attack / decay envelopes
AdEnv     kickVolEnv, kickPitchEnv, snareEnv;
//metronome
Metro     tick;


//an array of bools, presumably on and off
bool    kickSeq[MAX_LENGTH];
bool    snareSeq[MAX_LENGTH];
//the value of which step in the sequence we're on
uint8_t Step  = 0;

//function to process each step in a sequence
void ProcessTick();
//function to process changes in sequence
void ProcessControls();

//this happens every audio callback, called by StartAudio
//size is the size of the AudioCallback
void AudioCallback(float* in, float* out, size_t size)
{
  //floats here are effectively signals, sig being the output
    float osc_out, noise_out, snr_env_out, kck_env_out, sig;

     for(int i = 0; i < NUM_CONTROLS; i++)
    {
        kvals[i] = hardware.knob[i].Process();
    }
    //call ProcessTick (defined below)
    ProcessTick();


    //call ProcessControls (defined below)
    ProcessControls();

    //audio
    for(size_t i = 0; i < size; i += 2)
    {
      //envelope signals
        snr_env_out = snareEnv.Process();
        kck_env_out = kickVolEnv.Process();

        //oscillator freq is the kickPitchEnv value
        osc.SetFreq(kickPitchEnv.Process());
        //set oscillator amp to the value of the envelope
        osc.SetAmp(kck_env_out);
        //set the float osc_out to be the current value of the oscillator
        osc_out = osc.Process();

        //multiply noise by snare env for snare sound
        noise_out = noise.Process();
        noise_out *= snr_env_out;

        //add each signal together at -6dB
         sig = .5 * noise_out + .5 * osc_out;

         //output resultant signal to both stereo channels
        out[i]     = sig;
        out[i + 1] = sig;
    }
}

void SetupDrums(float samplerate)
{
    //initialise the oscillator object with sample rate and set waveform and amplitude
    osc.Init(samplerate);
    osc.SetWaveform(Oscillator::WAVE_TRI);
    osc.SetAmp(1);

    //intialise noise object
    noise.Init();

    //initialise the envelope to be used on the snare
    snareEnv.Init(samplerate);
    //use the function SetTime and aim it at the envelope's attack segment
    snareEnv.SetTime(ADENV_SEG_ATTACK, .01);
    //use the function SetTime and aim it at the envelope's decay segment
    snareEnv.SetTime(ADENV_SEG_DECAY, .2);
    //set the envelope to travel between 0 and 1
    snareEnv.SetMax(1);
    snareEnv.SetMin(0);

    kickPitchEnv.Init(samplerate);
    kickPitchEnv.SetTime(ADENV_SEG_ATTACK, .01);
    kickPitchEnv.SetTime(ADENV_SEG_DECAY, .05);
    //pitch runs from 400Hz to 50Hz
    kickPitchEnv.SetMax(400);
    kickPitchEnv.SetMin(50);

    kickVolEnv.Init(samplerate);
    kickVolEnv.SetTime(ADENV_SEG_ATTACK, .01);
    kickVolEnv.SetTime(ADENV_SEG_DECAY, 1);
    kickVolEnv.SetMax(1);
    kickVolEnv.SetMin(0);
}


//function for setting the sequence
void SetSeq(bool* seq, bool in)
{
  //unint32_t - unsigned 32-bit integer type
  //for every step in the sequence, apply the steps to the input
    for(uint32_t i = 0; i < MAX_LENGTH; i++)
    {
        seq[i] = in;
    }
}


int main(void)
{
  //initialise the Daisy
    hardware.Init();
    //what's the sample rate?
    float samplerate   = hardware.AudioSampleRate();
    //what's the callback rate?
    float callbackrate = hardware.AudioCallbackRate();

    //setup the drum sounds - call the above function
    SetupDrums(samplerate);

    //5 ticks per callback? 5 callbacks per tick?
    tick.Init(5, callbackrate);

    SetSeq(snareSeq, 0);
    SetSeq(kickSeq, 0);

    //turn on DSP
    hardware.StartAdc();

    //perform the AudioCallback function
    hardware.StartAudio(AudioCallback);

    // Loop forever
    for(;;) {}
}


void IncrementSteps()
{
  //add 1 to the step value
    Step++;
  //when you hit the max length, go back to 0
    Step %= 16;
}

void ProcessTick()
{
  //if if this is an audio callback where a tick is occuring
    if(tick.Process())
    {
      //see above
        IncrementSteps();

//if there is supposed to be a kick on this step
        if(kickSeq[Step])
        {
          //trigger the envelopes
            kickVolEnv.Trigger();
            kickPitchEnv.Trigger();
        }
//if there is supposed to be a snare on this step
        if(snareSeq[Step])
        {
          //trigger envelope
            snareEnv.Trigger();
        }
    }
}

//switches used to select whether affecting kick or snare sequence
uint8_t mode = 0;
void    UpdateSwitch()
{
    mode += hardware.sw[0].RisingEdge() ? -1 : 0;
    mode += hardware.sw[1].RisingEdge() ? 1 : 0;
    mode = DSY_MIN(DSY_MAX(0, mode), 1);
}


float k7old            = 0.f;
//default tempo of 3 (assuming Hz)
float tempo = 3;

void  UpdateTempo()
{
  float k7 = kvals[7];
//if 8th knob has moved more than 0.0005, assign new value to tempo
    if (abs(k7 - k7old)> 0.0005){
        tempo = (8 * k7) + 1;
    }
    tick.SetFreq(tempo);
    k7old = k7;
}

float stepLED = 0;

void UpdateSteps()
{
    //if you press the button, switch the step on / off
    if(mode)
    {
        for(size_t i = 0; i < 16; i++)
        {
          if(hardware.KeyboardRisingEdge(i))
          {
              snareSeq[i] = 1 - snareSeq[i];
          }

          if (i == Step)
          {
            stepLED = 1;
          }
          if (i != Step && snareSeq[i] == 1)
          {
            stepLED = 0.7;
          }
          if(i != Step && snareSeq[i] == 0)
          {
            stepLED = 0;
          }

            hardware.led_driver.SetLed(keyboard_leds[i], stepLED);
        }
    }

    else {
        for(size_t i = 0; i < 16; i++)
        {
          if(hardware.KeyboardRisingEdge(i))
          {
            kickSeq[i] = 1 - kickSeq[i];
          }

         if (i == Step)
          {
            stepLED = 1;
          }
          else
          {
            stepLED = 0;
          }

          if (i == Step)
          {
            stepLED = 1;
          }
          if (i != Step && kickSeq[i] == 1)
          {
            stepLED = 0.7;
          }
          if(i != Step && kickSeq[i] == 0)
          {
            stepLED = 0;
          }

          // here I need to set the values of the seq to the LEDs
          hardware.led_driver.SetLed(keyboard_leds[i], stepLED);
        }
    }
    hardware.led_driver.SwapBuffersAndTransmit();

}


void UpdateVars()
{ 
    kickVolEnv.SetTime(ADENV_SEG_DECAY, kvals[2] * 0.5 + 0.05);
    kickPitchEnv.SetTime(ADENV_SEG_DECAY, kvals[3] * 0.5 + 0.05);
    //pitch runs from 400Hz to 50Hz
    kickPitchEnv.SetMax(kvals[1]*200 + 200);
    kickPitchEnv.SetMin(kvals[0]*50 + 50);

    snareEnv.SetTime(ADENV_SEG_DECAY, kvals[5]  * 0.5 + 0.05);


}

//every callback, update the controls
void ProcessControls()
{
    hardware.ProcessDigitalControls();
    hardware.ProcessAnalogControls();

    UpdateSwitch();

    UpdateTempo();

    UpdateSteps();

    UpdateVars();
}
