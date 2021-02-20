#include "Drums.h"

//this happens every audio callback, called by StartAudio
//size is the size of the AudioCallback
void AudioCallback(float* in, float* out, size_t size)
{
  //floats here are effectively signals, sig being the output
    float kick_out, noise_out, snr_env_out, hat_env_out, hat_out, sig;

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
        hat_env_out = ampEnv[2].Process();

        ping.SetFreq(pitchEnv[0].Process());
        kick_out = ping.Process(trig);
        //multiply noise by snare env for snare sound
        noise_out = noise[0].Process();
        noise_out *= snr_env_out;

        hat_out = noise[1].Process();
        flt.Process(hat_out);
        hat_out = flt.Band();
        hat_out *= hat_env_out;

        //add each signal together (divide by number of sources)
        //TODO add gain control to each mode
        sig = .3 * noise_out + .3 * kick_out + .3 * hat_out;

         //output resultant signal to both stereo channels
        out[i]     = sig;
        out[i + 1] = sig;
        trig = 0;

    }
}

void SetupDrums(float samplerate)
{
    //initialise the oscillator object with sample rate and set waveform and amplitude
    //TODO add more sounds
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
    flt.SetRes(1);
    flt.SetFreq(8000);

    pitchEnv[0].SetMin(50);
    ping.Init(samplerate, init_buff, 128, PLUCK_MODE_WEIGHTED_AVERAGE);
    ping.SetAmp(1);
    ping.SetFreq(50);
    ping.SetDamp(0.8);
    ping.SetDecay(0.2);
}


//put our drums into an array of function pointers so they can be called by mode number in loop later 
typedef void (*Drummer)();

Drummer Drum[] = {Kick, Snare, Hat, Laser, Tom};

//function for setting the sequence
void SetSeq(bool* seq, bool in)
{
  //initialising array with 0s
    for(uint8_t i = 0; i < MAX_LENGTH; i++)
    {
        seq[i] = in;
    }
}

void    UpdateSwitch()
{
    //left switch triggers one shot of current drum
    if(hardware.sw[0].RisingEdge())
    {
      Drum[mode]();
    }

    //right switch start and stop
    if(hardware.sw[1].RisingEdge())
    {
      if (tick.GetFreq())
      {
        tick.SetFreq(0);
      }
      else
      {
        tick.SetFreq(tempo);
      }
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

    //Starts at 5 Hz
    tick.Init(5, callbackrate);

    for (uint8_t i = 0; i < NUM_MODES; i++)
      {
        SetSeq(Seq[i], 0);
        kold[i] = 0;
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
      
        for (uint8_t i = 0; i < NUM_MODES; i++)
        {
         //if there is supposed to be a drum trigger on this step
          if(Seq[i][Step])
          {
            Drum[i]();
          }
        }
    }
}


void  UpdateTempo()
{
  float k6 = kvals[6];
//if 7th knob has moved more than 0.0005, assign new value to tempo
    if (abs(k6 - kold[6])> 0.0005){
      // TODO Logarithms mate
        tempo = (8 * k6) + 3;
    }
    if (tick.GetFreq())
    {
      tick.SetFreq(tempo);
    }
    kold[6] = k6;
}

float stepLED = 0;

void UpdateLeds()
{
  //if you press the button, switch the step on / off
  // TODO lots of if statements here, there should be a better way
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
  //currently values change when you change page
  // requires comparison of parameter value, kval and kold??
  // look into Daisy Type Parameter
  switch (mode)
  {
    //kick drum vars
  case 0:
    pitchEnv[mode].SetTime(ADENV_SEG_DECAY, kvals[0] * 0.5 + 0.05);
    pitchEnv[mode].SetMax(kvals[1]*100 + 50);
    ping.SetDamp(kvals[2]);
    ping.SetDecay(kvals[3]);
    ping.SetAmp(kvals[4]);
    break;

    //snare vars
  case 1:
    ampEnv[mode].SetTime(ADENV_SEG_DECAY, kvals[0] * 0.5 + 0.05);
    ampEnv[mode].SetMax(kvals[4]);
    break;

    //hi-hat vars
  case 2:
    ampEnv[mode].SetTime(ADENV_SEG_DECAY, kvals[0] * 0.5 + 0.05);
    flt.SetFreq(4000 + (kvals[1]*6000));
    ampEnv[mode].SetMax(kvals[4]);
    break;

  default:
    break;
  }

    //TODO this creates only 4 modes, lots of dead space at the bottom
    float k7 = std::floor(kvals[7] * (NUM_MODES - 1));
    if (abs(k7 - kold[7])>=1)
    {
      mode = k7;
      kold[7] = k7;
    }
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
