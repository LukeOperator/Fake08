#include "Drums.h"

//this happens every audio callback, called by StartAudio
//size is the size of the AudioCallback
void AudioCallback(float* in, float* out, size_t size)
{
  //floats here are effectively signals, sig being the output
    float osc_out, noise_out, snr_env_out, kck_env_out, hat_env_out, hat_out, sig;

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
        snr_env_out = ampEnv[1].Process();
        kck_env_out = ampEnv[0].Process();
        hat_env_out = ampEnv[2].Process();

        //oscillator freq is the kickPitchEnv value
        osc[0].SetFreq(pitchEnv[0].Process());
        //set oscillator amp to the value of the envelope
        osc[0].SetAmp(kck_env_out);
        //set the float osc_out to be the current value of the oscillator
        osc_out = osc[0].Process();

        //multiply noise by snare env for snare sound
        noise_out = noise[0].Process();
        noise_out *= snr_env_out;

        hat_out = noise[1].Process();
        flt.Process(hat_out);
        hat_out = flt.High();
        hat_out *= hat_env_out;

        //add each signal together at -6dB
         sig = .3 * noise_out + .3 * osc_out + .3 * hat_out;

         //output resultant signal to both stereo channels
        out[i]     = sig;
        out[i + 1] = sig;
    }
}

//switches used to select whether affecting kick or snare sequence
uint8_t mode = 0;
void    UpdateSwitch()
{
    mode += hardware.sw[0].RisingEdge() ? -1 : 0;
    mode += hardware.sw[1].RisingEdge() ? 1 : 0;
    mode = DSY_MIN(DSY_MAX(0, mode), 4);
}

void SetupDrums(float samplerate)
{
    //initialise the oscillator object with sample rate and set waveform and amplitude
    osc[0].Init(samplerate);
    osc[0].SetWaveform(Oscillator::WAVE_TRI);
    osc[0].SetAmp(1);

    osc[1].Init(samplerate);
    osc[1].SetWaveform(Oscillator::WAVE_SIN);
    osc[1].SetAmp(1);
    
    osc[2].Init(samplerate);
    osc[2].SetWaveform(Oscillator::WAVE_SIN);
    osc[2].SetAmp(1);
    
    //intialise noise object
    noise[0].Init();
    noise[1].Init();


  for (uint8_t i = 0; i < NUM_MODES; i++)
    {   
      //initialise the envelopes
      ampEnv[i].Init(samplerate);
      //use the function SetTime and aim it at the envelope's attack segment
      ampEnv[i].SetTime(ADENV_SEG_ATTACK, .01);
      //use the function SetTime and aim it at the envelope's decay segment
      ampEnv[i].SetTime(ADENV_SEG_DECAY, .2);
      //set the envelope to travel between 0 and 1
      ampEnv[i].SetMax(1);
      ampEnv[i].SetMin(0);

      pitchEnv[i].Init(samplerate);
      pitchEnv[i].SetTime(ADENV_SEG_ATTACK, .01);
      pitchEnv[i].SetTime(ADENV_SEG_DECAY, .2);
      pitchEnv[i].SetMax(1);
      pitchEnv[i].SetMin(0);
    }

    flt.Init(samplerate);
    flt.SetFreq(8000);
    flt.SetRes(2);
}

typedef void (*Drummer)();

Drummer Drum[] = {Kick, Snare, Hat, Laser, Tom};

//function for setting the sequence
void SetSeq(bool* seq, bool in)
{
  //unint32_t - unsigned 32-bit integer type
  //for every step in the sequence, apply the steps to the input
    for(uint8_t i = 0; i < MAX_LENGTH; i++)
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

    for (uint8_t i = 0; i < NUM_MODES; i++)
      {
        SetSeq(Seq[i], 0);
      }

    //turn on DSP
    hardware.StartAdc();

    //perform the AudioCallback function
    hardware.StartAudio(AudioCallback);

    // Loop forever
    for(;;) {}
}

//the value of which step in the sequence we're on
uint8_t Step  = 0;

void IncrementSteps()
{
  //add 1 to the step value
    Step++;
  //when you hit the max length, go back to 0
    Step %= 16;
}

void ProcessTick()
{
//if this is an audio callback where a tick is occuring
    if(tick.Process())
    {
      //see above
        IncrementSteps();
      //if there is supposed to be a kick on this step
        for (uint8_t i = 0; i < NUM_MODES; i++)
        {
          if(Seq[i][Step])
          {
            Drum[i]();
          }
        }
    }
}

float k6old            = 0.f;

void  UpdateTempo()
{
  float k6 = kvals[6];
//if 8th knob has moved more than 0.0005, assign new value to tempo
    if (abs(k6 - k6old)> 0.0005){
        tempo = (8 * k6) + 1;
    }
    tick.SetFreq(tempo);
    k6old = k6;
}

float stepLED = 0;

void UpdateLeds()
{
  //if you press the button, switch the step on / off
  for(size_t i = 0; i < 16; i++)
  {
    if(hardware.KeyboardRisingEdge(i))
    {
      Seq[mode][i] = 1 - Seq[mode][i];
    }
    if (i == Step)
    {
      stepLED = 1;
    }
    if (i != Step && Seq[mode][i] == 1)
    {
      stepLED = 0.7;
    }
    if(i != Step && Seq[mode][i] == 0)
    {
      stepLED = 0;
    }
    // here I need to set the values of the seq to the LEDs
    hardware.led_driver.SetLed(keyboard_leds[i], stepLED);
  }
  //we've set the new values, this command actually sends those values
  hardware.led_driver.SwapBuffersAndTransmit();
}


void UpdateVars()
{ 

    ampEnv[0].SetTime(ADENV_SEG_DECAY, kvals[2] * 0.5 + 0.05);
    pitchEnv[0].SetTime(ADENV_SEG_DECAY, kvals[3] * 0.5 + 0.05);
    //pitch runs from 400Hz to 50Hz
    pitchEnv[0].SetMax(kvals[1]*200 + 200);
    pitchEnv[0].SetMin(kvals[0]*50 + 50);

    ampEnv[1].SetTime(ADENV_SEG_DECAY, kvals[5]  * 0.5 + 0.05);
}

//every callback, update the controls
void ProcessControls()
{
    hardware.ProcessDigitalControls();
    hardware.ProcessAnalogControls();

    UpdateSwitch();

    UpdateTempo();

    UpdateLeds();

    UpdateVars();
}
