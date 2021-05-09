#include "Drums.h"

//this happens every audio callback, called by StartAudio
//size is the size of the AudioCallback
void AudioCallback(float* in, float* out, size_t size)
{
  //floats here are effectively signals, sig being the output
    float kick_out, kick_noise, kick_pitch, kick_env_out, 
    inSnare, snare_noise, snare_out, snr_env_out,
    inHat, hat_noise, hat_env_out, hat_out, 
    cv1_out, cv2_out, sig;

    
    //call ProcessTick (defined below)
    ProcessTick();

    //call ProcessControls (defined below)
    ProcessControls();

    //audio
    for(size_t i = 0; i < size; i += 2)
    {
        //envelope signals
        kick_env_out = ampEnv[0].Process();
        snr_env_out = ampEnv[1].Process();
        hat_env_out = ampEnv[2].Process();
        
        snare_noise = noise[0].Process();
        hat_noise = noise[1].Process();

        //kick
        kick_pitch = pitchEnv[0].Process();        
        osc[0].SetFreq(kick_pitch);
        osc[0].SetAmp(kick_env_out * kick_env_out * params[0][3]);
        kick_out = osc[0].Process() + (kick_noise * 0.001 * kick_env_out * kick_env_out);        
        
        //snare
        sn.Process(snare_noise * snr_env_out * snr_env_out);
        inSnare = sn.Low();
        snare_out = highPass[0].Process(inSnare) * params[1][3] * params[1][3];
        
        //hat
        flt.Process(hat_noise * hat_env_out);
        inHat = flt.High();
        hat_out = highPass[1].Process(inHat) * params[2][3] * params[2][3];

        //CV outs
        cv1_out = ampEnv[3].Process();
        cv2_out = ampEnv[4].Process();
        hardware.SetCvOut1(cv1_out * 4095);
        hardware.SetCvOut2(cv2_out * 4095);

        //add each signal together (divide by number of sources)
        //TODO add gain control to each mode
        sig = (snare_out + kick_out + hat_out);

         //output resultant signal to both stereo channels
        out[i]     = sig;
        out[i + 1] = sig;
        
        for (uint8_t j = 0; j < 5; j++)
        {
          trig[j] = 0;
        }

    }
}

void SetupDrums(float samplerate)
{
  //process all knobs once so that knobs don't send values of 0 on init
  kold[0] = p_Pitch.Process();
  kold[1] = p_Dec.Process();
  kold[2] = p_Fx.Process();
  kold[3] = p_Amp.Process();
  kold[6] = p_tempo.Process();
  kold[7] = p_mode.Process();

  //TODO add more sounds
  //intialise noise object
  noise[0].Init();
  noise[1].Init();

  highPass[0].Init(samplerate);
  highPass[0].SetFreq(passPoint);
  highPass[1].Init(samplerate);
  highPass[1].SetFreq(passPoint);


  for (uint8_t i = 0; i < NUM_MODES; i++)
    { 
      osc[i].Init(samplerate);
      osc[i].SetWaveform(Oscillator::WAVE_SIN);
      osc[i].SetFreq(100);
      osc[i].SetAmp(1);
      //initialise the envelopes
      ampEnv[i].Init(samplerate);
      //use the function SetTime and aim it at the envelope's attack segment
      ampEnv[i].SetTime(ADENV_SEG_ATTACK, 0.01f);
      //use the function SetTime and aim it at the envelope's decay segment
      ampEnv[i].SetTime(ADENV_SEG_DECAY, .2f);
      //set the envelope to travel between 0 and 1
      ampEnv[i].SetMax(1);
      ampEnv[i].SetMin(0);

      pitchEnv[i].Init(samplerate);
      pitchEnv[i].SetTime(ADENV_SEG_ATTACK, 0.01f);
      pitchEnv[i].SetTime(ADENV_SEG_DECAY, .2f);
      pitchEnv[i].SetCurve(-50);
      pitchEnv[i].SetMax(mPitch[i] + oPitch[i]);
      pitchEnv[i].SetMin(oPitch[i]);


    }   

    flt.Init(samplerate);
    flt.SetRes(0.5);
    flt.SetFreq(8000);


    sn.Init(samplerate);
    sn.SetRes(0.5);
    sn.SetFreq(1000);
    sn.SetDrive(1);

}


//put our drums into an array of function pointers so they can be called by mode number in loop later 
typedef void (*Drummer)();

Drummer Drum[] = {Kick, Snare, Hat, CV1, CV2};

//function for setting the sequence
void SetSeq(bool* seq, bool in)
{
  //initialising array with 0s
    for(uint8_t i = 0; i < MAX_LENGTH; i++)
    {
        seq[i] = in;
    }
}

void UpdateSwitch()
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

    //init knobs for parameters
    p_Pitch.Init(hardware.knob[0], 0, 1, Parameter::EXPONENTIAL);
    p_Dec.Init(hardware.knob[1], 0, 1, Parameter::LINEAR);
    p_Fx.Init(hardware.knob[2], 0, 1, Parameter::LINEAR);
    p_Amp.Init(hardware.knob[3], 0, 1, Parameter::LINEAR);

    p_tempo.Init(hardware.knob[6], 2, 20, Parameter::LOGARITHMIC);
    p_mode.Init(hardware.knob[7], 0.5, 4.5, Parameter::LINEAR);


    //setup the drum sounds - call the above function
    SetupDrums(samplerate);

    //Starts at 3 Hz
    tick.Init(3, callbackrate);

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
    for(;;) {
    }
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

float stepLED = 0;

void UpdateLeds()
{
  // TODO lots of if statements here, there should be a better way
  // also need knob LEDS here

  for(size_t i = 0; i < 16; i++)
  {
    //if we press a button, turn on or off
    if(hardware.KeyboardRisingEdge(i))
    {
      Seq[mode][i] = 1 - Seq[mode][i];
    }

    //if we're on the current step, full birghtness
    //if we're not, show whether step will be on or off with less brightness

    if (i == Step)
    {
      stepLED = 1;
    }
    else{
      if (Seq[mode][i])
      {
        stepLED = 0.7;
      }
      else
      {
        stepLED = 0;
      }
    }

    // here I need to set the values of the seq to the LEDs
    hardware.led_driver.SetLed(keyboard_leds[i], stepLED);
    
  }

  //we've set the new values, this command actually sends those values
  hardware.led_driver.SwapBuffersAndTransmit();
  
}


void UpdateVars()
{ 
  kvals[0] = p_Pitch.Process();
  kvals[1] = p_Dec.Process();
  kvals[2] = p_Fx.Process();
  kvals[3] = p_Amp.Process();
  kvals[6] = p_tempo.Process();
  kvals[7] = p_mode.Process();

  //if any of the knobs move, apply that value to the params
  for(uint8_t i = 0; i < NUM_CONTROLS; i++)
  {
    if (abs(kvals[i]-kold[i]) > 0.005)
    {
      //apply the tone controls to the params matrix
      if(i < 4)
      {
        params[mode][i] = kvals[i];
      }
      kold[i] = kvals[i];
    }
  }

  //apply params to envelopes with multipliers and offsets
  for(uint8_t i = 0; i < 3; i++)
  {
    pitchEnv[i].SetMax((params[i][0] * mPitch[i]) + oPitch[i]);
    ampEnv[i].SetTime(ADENV_SEG_DECAY, params[i][1] * mDec[i] + oDec[i]);
    //gain
  }

  //unique vals and fX levels

  //kick
  pitchEnv[0].SetTime(ADENV_SEG_DECAY, ((params[0][2] * mDec[0]) + (oDec[0])));

  //snare
  sn.SetFreq(params[1][0] * mPitch[1] + oPitch[1]);
  sn.SetRes(params[1][2]);

  //hat
  flt.SetFreq(params[2][0] * mPitch[2] + oPitch[2]);
  flt.SetRes(params[2][2]);

  //CV1
  ampEnv[3].SetMin(params[3][0]);
  ampEnv[3].SetMax(params[3][1]);
  ampEnv[3].SetTime(ADENV_SEG_ATTACK, ((params[3][2] * mDec[3]) + (oDec[3])));
  ampEnv[3].SetTime(ADENV_SEG_DECAY, ((params[3][3] * mDec[3]) + (oDec[3])));

  //CV2
  ampEnv[4].SetMin(params[4][0]);
  ampEnv[4].SetMax(params[4][1]);
  ampEnv[4].SetTime(ADENV_SEG_ATTACK, ((params[4][2] * mDec[4]) + (oDec[4])));
  ampEnv[4].SetTime(ADENV_SEG_DECAY, ((params[4][3] * mDec[4]) + (oDec[4])));
  


  tempo = kvals[6];

  //only send the tempo value if we're playing
  if (tick.GetFreq())
      {
        tick.SetFreq(tempo);
      }

  mode = std::floor(kvals[7]);
}

//every callback, update the controls
void ProcessControls()
{
    hardware.ProcessDigitalControls();
    hardware.ProcessAnalogControls();

    UpdateSwitch();

    UpdateVars();

    UpdateLeds();
}
