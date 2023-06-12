#include <cstdio> // for printing to stdout

#include "Gamma/Analysis.h"
#include "Gamma/Effects.h"
#include "Gamma/Envelope.h"
#include "Gamma/Gamma.h"
#include "Gamma/Oscillator.h"
#include "Gamma/Types.h"
#include "Gamma/DFT.h"

#include "al/app/al_App.hpp"
#include "al/graphics/al_Shapes.hpp"
#include "al/scene/al_PolySynth.hpp"
#include "al/scene/al_SynthSequencer.hpp"
#include "al/ui/al_ControlGUI.hpp"
#include "al/ui/al_Parameter.hpp"
#include "al/io/al_MIDI.hpp"
#include "al/math/al_Random.hpp"


// #include <json/json.h>
// #include "json.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
using json = nlohmann::json;

// using namespace gam;
using namespace al;
using namespace std;
#define FFT_SIZE 4048

float freq_of(int midi) {
    float freq = pow(2, ((midi-69)/12.0)) * 440;
    return freq;
}

class SineEnv : public SynthVoice
{
public:
  // Unit generators
  gam::Pan<> mPan;
  gam::Sine<> mOsc1;
  gam::Sine<> mOsc3;
  gam::Saw<> mSaw1;
  gam::Saw<> mSaw2;
  gam::Saw<> mSaw3;
  gam::Env<3> mAmpEnv;

  gam::EnvFollow<> mEnvFollow;
  Mesh mMesh;
  double time = 1;
  double time2 = 0;

  // Initialize voice. This function will only be called once per voice when
  // it is created. Voices will be reused if they are idle.
  void init() override
  {
    // Intialize envelope
    mAmpEnv.curve(4); // make segments lines
    mAmpEnv.levels(0, 0.8, 0.6, 0);
    mAmpEnv.sustainPoint(2); // Make point 2 sustain until a release is issued

    // This is a quick way to create parameters for the voice. Trigger
    // parameters are meant to be set only when the voice starts, i.e. they
    // are expected to be constant within a voice instance. (You can actually
    // change them while you are prototyping, but their changes will only be
    // stored and aplied when a note is triggered.)

    //addCone(mMesh);
    addDodecahedron(mMesh,0.4);
    addCircle(mMesh, 0.7);

    createInternalTriggerParameter("amplitude", 0.1, 0.0, 1.0);
    createInternalTriggerParameter("frequency", 60, 20, 5000);
    createInternalTriggerParameter("attackTime", 0.2, 0.01, 3.0);
    createInternalTriggerParameter("releaseTime", 1, 0.1, 10.0);
    createInternalTriggerParameter("pan", 0.0, -1.0, 1.0);
  }

  // The audio processing function
  void onProcess(AudioIOData &io) override
  {
    // Get the values from the parameters and apply them to the corresponding
    // unit generators. You could place these lines in the onTrigger() function,
    // but placing them here allows for realtime prototyping on a running
    // voice, rather than having to trigger a new voice to hear the changes.
    // Parameters will update values once per audio callback because they
    // are outside the sample processing loop.
    mOsc1.freq(getInternalParameterValue("frequency"));
    mOsc3.freq(3*getInternalParameterValue("frequency"));
    mSaw1.freq(getInternalParameterValue("frequency"));
    mSaw3.freq(3*getInternalParameterValue("frequency"));
    mSaw2.freq(2*getInternalParameterValue("frequency"));
  

    mAmpEnv.lengths()[0] = getInternalParameterValue("attackTime");
    mAmpEnv.lengths()[2] = getInternalParameterValue("releaseTime");
    mPan.pos(getInternalParameterValue("pan"));
    while (io())
    {
      
      float s1 =  // mSaw1() * (1.0) * mAmpEnv() * getInternalParameterValue("amplitude")
              // + mSaw3() * (1.0/6.0) * mAmpEnv() * getInternalParameterValue("amplitude")
              mSaw2() * (1.0/3.0) * mAmpEnv() * getInternalParameterValue("amplitude")
               + mOsc1() * (1.0) * mAmpEnv() * getInternalParameterValue("amplitude")
               + mOsc3() * (1.0/3.0) * mAmpEnv() * getInternalParameterValue("amplitude");
      float s2;
      mPan(s1, s1, s2);
      io.out(0) += s1;
      io.out(1) += s2;
    }
    // We need to let the synth know that this voice is done
    // by calling the free(). This takes the voice out of the
    // rendering chain
    if (mAmpEnv.done() && (mEnvFollow.value() < 0.001f)){
      free();
    }
  }

  // The graphics processing function
  void onProcess(Graphics &g) override
  {
    // empty if there are no graphics to draw
    // Get the paramter values on every video frame, to apply changes to the
    // current instance
    float frequency = getInternalParameterValue("frequency");
    float amplitude = getInternalParameterValue("amplitude");
    time += 0.02;
    // Now draw
    g.pushMatrix();
    // Move x according to frequency, y according to amplitude
    g.translate(frequency / 300 * (time) - 4, frequency / 100 * cos(time) +2, -14);
    // Scale in the x and y directions according to amplitude
    g.scale(frequency/1000, frequency/700, 1);
    // Set the color. Red and Blue according to sound amplitude and Green
    // according to frequency. Alpha fixed to 0.4
    //g.color(frequency / 1000, amplitude / 1000,  mEnvFollow.value(), 0.4);
    g.color(HSV(frequency/100*sin(0.05*time)));
    //g.numLight(10);
    g.draw(mMesh);
    g.popMatrix();
  }

  // The triggering functions just need to tell the envelope to start or release
  // The audio processing function checks when the envelope is done to remove
  // the voice from the processing chain.
  void onTriggerOn() override { mAmpEnv.reset(); }

  void onTriggerOff() override { mAmpEnv.release(); }
};

class SineEnv2 : public SynthVoice
{
public:
  // Unit generators
  gam::Pan<> mPan;
  gam::Sine<> mOsc1;
  gam::Sine<> mOsc3;
  gam::Saw<> mSaw1;
  gam::Saw<> mSaw2;
  gam::Saw<> mSaw3;
  gam::Env<3> mAmpEnv;

  gam::EnvFollow<> mEnvFollow;
  Mesh mMesh;
  double time = 1;
  double time2 = 0;

  // Initialize voice. This function will only be called once per voice when
  // it is created. Voices will be reused if they are idle.
  void init() override
  {
    // Intialize envelope
    mAmpEnv.curve(4); // make segments lines
    mAmpEnv.levels(0, 1, 1, 0);
    mAmpEnv.sustainPoint(2); // Make point 2 sustain until a release is issued

    // This is a quick way to create parameters for the voice. Trigger
    // parameters are meant to be set only when the voice starts, i.e. they
    // are expected to be constant within a voice instance. (You can actually
    // change them while you are prototyping, but their changes will only be
    // stored and aplied when a note is triggered.)

    //addCone(mMesh);
    addDodecahedron(mMesh,0.4);
    addCircle(mMesh, 0.7);

    createInternalTriggerParameter("amplitude", 0.1, 0.0, 1.0);
    createInternalTriggerParameter("frequency", 60, 20, 5000);
    createInternalTriggerParameter("attackTime", 0.2, 0.01, 3.0);
    createInternalTriggerParameter("releaseTime", 1, 0.1, 10.0);
    createInternalTriggerParameter("pan", 0.0, -1.0, 1.0);
  }

  // The audio processing function
  void onProcess(AudioIOData &io) override
  {
    // Get the values from the parameters and apply them to the corresponding
    // unit generators. You could place these lines in the onTrigger() function,
    // but placing them here allows for realtime prototyping on a running
    // voice, rather than having to trigger a new voice to hear the changes.
    // Parameters will update values once per audio callback because they
    // are outside the sample processing loop.
    mOsc1.freq(getInternalParameterValue("frequency"));
    mOsc3.freq(3*getInternalParameterValue("frequency"));
    mSaw1.freq(getInternalParameterValue("frequency"));
    mSaw3.freq(3*getInternalParameterValue("frequency"));
    mSaw2.freq(2*getInternalParameterValue("frequency"));
  

    mAmpEnv.lengths()[0] = getInternalParameterValue("attackTime");
    mAmpEnv.lengths()[2] = getInternalParameterValue("releaseTime");
    mPan.pos(getInternalParameterValue("pan"));
    while (io())
    {
      
      float s1 =  // mSaw1() * (1.0) * mAmpEnv() * getInternalParameterValue("amplitude")
              // + mSaw3() * (1.0/6.0) * mAmpEnv() * getInternalParameterValue("amplitude")
              0.4*(mSaw2() * (1.0/3.0) * mAmpEnv() * getInternalParameterValue("amplitude")
               + mOsc1() * (1.0) * mAmpEnv() * getInternalParameterValue("amplitude")
               + mOsc3() * (1.0/3.0) * mAmpEnv() * getInternalParameterValue("amplitude"));
      float s2;
      mPan(s1, s1, s2);
      io.out(0) += s1;
      io.out(1) += s2;
    }
    // We need to let the synth know that this voice is done
    // by calling the free(). This takes the voice out of the
    // rendering chain
    if (mAmpEnv.done() && (mEnvFollow.value() < 0.001f)){
      free();
    }
  }

  // The graphics processing function
  void onProcess(Graphics &g) override
  {
    // empty if there are no graphics to draw
    // Get the paramter values on every video frame, to apply changes to the
    // current instance
    float frequency = getInternalParameterValue("frequency");
    float amplitude = getInternalParameterValue("amplitude");
    time += 0.02;
    // Now draw
    g.pushMatrix();
    // Move x according to frequency, y according to amplitude
    g.translate(frequency / 300 * (time) - 4, frequency / 100 * cos(time) +2, -14);
    // Scale in the x and y directions according to amplitude
    g.scale(frequency/1000, frequency/700, 1);
    // Set the color. Red and Blue according to sound amplitude and Green
    // according to frequency. Alpha fixed to 0.4
    //g.color(frequency / 1000, amplitude / 1000,  mEnvFollow.value(), 0.4);
    g.color(HSV(frequency/100*sin(0.05*time)));
    //g.numLight(10);
    g.draw(mMesh);
    g.popMatrix();
  }

  // The triggering functions just need to tell the envelope to start or release
  // The audio processing function checks when the envelope is done to remove
  // the voice from the processing chain.
  void onTriggerOn() override { mAmpEnv.reset(); }

  void onTriggerOff() override { mAmpEnv.release(); }
};

class SineEnv3 : public SynthVoice
{
public:
  // Unit generators
  gam::Pan<> mPan;
  gam::Sine<> mOsc1;
  gam::Sine<> mOsc3;
  gam::Saw<> mSaw1;
  gam::Saw<> mSaw2;
  gam::Saw<> mSaw3;
  gam::Env<3> mAmpEnv;

  gam::EnvFollow<> mEnvFollow;
  Mesh mMesh;
  double time = 1;
  double time2 = 0;

  // Initialize voice. This function will only be called once per voice when
  // it is created. Voices will be reused if they are idle.
  void init() override
  {
    // Intialize envelope
    mAmpEnv.curve(4); // make segments lines
    mAmpEnv.levels(0, 0.8, 0.6, 0);
    mAmpEnv.sustainPoint(2); // Make point 2 sustain until a release is issued

    // This is a quick way to create parameters for the voice. Trigger
    // parameters are meant to be set only when the voice starts, i.e. they
    // are expected to be constant within a voice instance. (You can actually
    // change them while you are prototyping, but their changes will only be
    // stored and aplied when a note is triggered.)

    //addCone(mMesh);
    // addDodecahedron(mMesh,0.4);
    addRect(mMesh, 2.5,0.25);

    createInternalTriggerParameter("amplitude", 0.1, 0.0, 1.0);
    createInternalTriggerParameter("frequency", 60, 20, 5000);
    createInternalTriggerParameter("attackTime", 0.2, 0.01, 3.0);
    createInternalTriggerParameter("releaseTime", 1, 0.1, 10.0);
    createInternalTriggerParameter("pan", 0.0, -1.0, 1.0);
  }

  // The audio processing function
  void onProcess(AudioIOData &io) override
  {
    // Get the values from the parameters and apply them to the corresponding
    // unit generators. You could place these lines in the onTrigger() function,
    // but placing them here allows for realtime prototyping on a running
    // voice, rather than having to trigger a new voice to hear the changes.
    // Parameters will update values once per audio callback because they
    // are outside the sample processing loop.
    mOsc1.freq(getInternalParameterValue("frequency"));
    mOsc3.freq(2*getInternalParameterValue("frequency"));
    mSaw1.freq(getInternalParameterValue("frequency"));
    mSaw3.freq(3*getInternalParameterValue("frequency"));
    mSaw2.freq(2*getInternalParameterValue("frequency"));
  

    mAmpEnv.lengths()[0] = getInternalParameterValue("attackTime");
    mAmpEnv.lengths()[2] = getInternalParameterValue("releaseTime");
    mPan.pos(getInternalParameterValue("pan"));
    while (io())
    {
      
      float s1 =  // mSaw1() * (1.0) * mAmpEnv() * getInternalParameterValue("amplitude")
              // + mSaw3() * (1.0/6.0) * mAmpEnv() * getInternalParameterValue("amplitude")
              //mSaw2() * (1.0/3.0) * mAmpEnv() * getInternalParameterValue("amplitude")
                mOsc1() * (1.0) * mAmpEnv() * getInternalParameterValue("amplitude")
               + mOsc3() * (1.0/3.0) * mAmpEnv() * getInternalParameterValue("amplitude");
      float s2;
      mPan(s1, s1, s2);
      io.out(0) += s1;
      io.out(1) += s2;
    }
    // We need to let the synth know that this voice is done
    // by calling the free(). This takes the voice out of the
    // rendering chain
    if (mAmpEnv.done() && (mEnvFollow.value() < 0.001f)){
      free();
    }
  }

  // The graphics processing function
  void onProcess(Graphics &g) override
  {
    // empty if there are no graphics to draw
    // Get the paramter values on every video frame, to apply changes to the
    // current instance
    float frequency = getInternalParameterValue("frequency");
    float amplitude = getInternalParameterValue("amplitude");
    time += 0.02;
    // Now draw
    g.pushMatrix();
    // Move x according to frequency, y according to amplitude
    // g.translate(frequency / 300 * (time) - 4, frequency / 100 * cos(time) +2, -14);
    g.translate(500 / 300 * 0.2*cos(time) + 2.5, 500 / 100 * 0.02 +2.7, -14);
    // Scale in the x and y directions according to amplitude
    // g.scale(frequency/1000, frequency/700, 1);
    // Set the color. Red and Blue according to sound amplitude and Green
    // according to frequency. Alpha fixed to 0.4
    //g.color(frequency / 1000, amplitude / 1000,  mEnvFollow.value(), 0.4);
    g.color(HSV(1));
    //g.numLight(10);
    //g.draw(mMesh);
    g.popMatrix();
  }

  // The triggering functions just need to tell the envelope to start or release
  // The audio processing function checks when the envelope is done to remove
  // the voice from the processing chain.
  void onTriggerOn() override { mAmpEnv.reset(); }

  void onTriggerOff() override { mAmpEnv.release(); }
};

class SquareWave : public SynthVoice
{
public:
  // Unit generators
  gam::Pan<> mPan;
  gam::Sine<> mOsc1;
  gam::Sine<> mOsc3;
  gam::Sine<> mOsc5;
  gam::Sine<> mOsc7;
  gam::Sine<> mOsc9;
  gam::Env<3> mAmpEnv;

  gam::EnvFollow<> mEnvFollow;
  Mesh mMesh;
  double time = 3.5;
  double time2 = 0;

  // Initialize voice. This function will only be called once per voice when
  // it is created. Voices will be reused if they are idle.
  void init() override
  {
    // Intialize envelope
    mAmpEnv.curve(0); // make segments lines
    mAmpEnv.levels(0, 1, 1, true);
    mAmpEnv.sustainPoint(2); // Make point 2 sustain until a release is issued

    // This is a quick way to create parameters for the voice. Trigger
    // parameters are meant to be set only when the voice starts, i.e. they
    // are expected to be constant within a voice instance. (You can actually
    // change them while you are prototyping, but their changes will only be
    // stored and aplied when a note is triggered.)

    addIcosphere(mMesh,0.8,2);
    //addAnnulus(mMesh, 1.2,1.5);
    addWireBox(mMesh, 0.01);

    createInternalTriggerParameter("amplitude", 0.3, 0.0, 1.0);
    createInternalTriggerParameter("frequency", 60, 20, 5000);
    createInternalTriggerParameter("attackTime", 0.2, 0.01, 3.0);
    createInternalTriggerParameter("releaseTime", 0.2, 0.1, 10.0);
    createInternalTriggerParameter("pan", 0.0, -1.0, 1.0);
  }

  // The audio processing function
  void onProcess(AudioIOData &io) override
  {
    // Get the values from the parameters and apply them to the corresponding
    // unit generators. You could place these lines in the onTrigger() function,
    // but placing them here allows for realtime prototyping on a running
    // voice, rather than having to trigger a new voice to hear the changes.
    // Parameters will update values once per audio callback because they
    // are outside the sample processing loop.
    mOsc1.freq(getInternalParameterValue("frequency"));
    mOsc3.freq(3*getInternalParameterValue("frequency"));
    mOsc5.freq(5*getInternalParameterValue("frequency"));
    mOsc7.freq(7*getInternalParameterValue("frequency"));
    mOsc9.freq(9*getInternalParameterValue("frequency"));
  

    mAmpEnv.lengths()[0] = getInternalParameterValue("attackTime");
    mAmpEnv.lengths()[2] = getInternalParameterValue("releaseTime");
    mPan.pos(getInternalParameterValue("pan"));
    while (io())
    {
      
      float s1 =  0.4*(mOsc1() * (1.0) * mAmpEnv() * getInternalParameterValue("amplitude")
               + mOsc3() * (1.0/3.0) * mAmpEnv() * getInternalParameterValue("amplitude")
               + mOsc5() * (1.0/5.0) * mAmpEnv() * getInternalParameterValue("amplitude")
               + mOsc7() * (1.0/7.0) * mAmpEnv() * getInternalParameterValue("amplitude")
               + mOsc9() * (1.0/9.0) * mAmpEnv() * getInternalParameterValue("amplitude"));
      float s2;
      mPan(s1, s1, s2);
      io.out(0) += s1;
      io.out(1) += s2;
    }
    // We need to let the synth know that this voice is done
    // by calling the free(). This takes the voice out of the
    // rendering chain
    if (mAmpEnv.done() && (mEnvFollow.value() < 0.001f)){
      free();
    }
  }

  // The graphics processing function
  void onProcess(Graphics &g) override
  {
    // empty if there are no graphics to draw
    // Get the paramter values on every video frame, to apply changes to the
    // current instance
    float frequency = getInternalParameterValue("frequency");
    float amplitude = getInternalParameterValue("amplitude");
    time += 0.03;
    time2 -= 0.02;
    // Now draw
    g.pushMatrix();
    // Move x according to frequency, y according to amplitude
    g.translate(frequency / 360 * (0.1*time) + 1.5, frequency / 180 * sin(0.05*time2) + 0.8, -10);
    // Scale in the x and y directions according to amplitude
    g.scale((frequency+600)/1500, (frequency+600)/1400, 1.2* sin(0.2*time2));
    // Set the color. Red and Blue according to sound amplitude and Green
    // according to frequency. Alpha fixed to 0.4
    //g.color(frequency / 1000, amplitude / 1000,  mEnvFollow.value(), 0.4);
    g.color(HSV(frequency/200));
    //g.numLight(10);
    g.draw(mMesh);
    g.popMatrix();
  }

  // The triggering functions just need to tell the envelope to start or release
  // The audio processing function checks when the envelope is done to remove
  // the voice from the processing chain.
  void onTriggerOn() override { mAmpEnv.reset(); }

  void onTriggerOff() override { mAmpEnv.release(); }
};

class PluckedString : public SynthVoice
{
public:
    float mAmp;
    float mDur;
    float mPanRise;
    gam::Pan<> mPan;
    //gam::NoiseWhite<> noise;
    gam::NoisePink<> noise;
    gam::Decay<> env;
    gam::MovingAvg<> fil{2};
    gam::Delay<float, gam::ipl::Trunc> delay;
    gam::ADSR<> mAmpEnv;
    gam::EnvFollow<> mEnvFollow;
    gam::Env<2> mPanEnv;
    gam::STFT stft = gam::STFT(FFT_SIZE, FFT_SIZE / 4, 0, gam::BLACKMAN_HARRIS, gam::MAG_FREQ);
    // This time, let's use spectrograms for each notes as the visual components.
    Mesh mSpectrogram;
    vector<float> spectrum;
    double a = 0;
    double b = 0;
    double timepose = 10;
    // Additional members
    Mesh mMesh;

    virtual void init() override
    {
        // Declare the size of the spectrum
        spectrum.resize(FFT_SIZE / 2 + 1);
        // mSpectrogram.primitive(Mesh::POINTS);
        mSpectrogram.primitive(Mesh::LINE_STRIP);
        mAmpEnv.levels(0, 1, 1, 0);
        mPanEnv.curve(4);
        env.decay(0.1);
        delay.maxDelay(1. / 27.5);
        //delay.maxDelay(1.);
        delay.delay(1. / 440.0);
        //delay.delay(1. / 2);

        addDisc(mMesh, 1.0, 30);
        createInternalTriggerParameter("amplitude", 0.3, 0.0, 1.0);
        createInternalTriggerParameter("frequency", 60, 20, 5000);
        createInternalTriggerParameter("attackTime", 0.001, 0.001, 1.0);
        createInternalTriggerParameter("releaseTime", 3.0, 0.1, 10.0);
        createInternalTriggerParameter("sustain", 0.7, 0.0, 1.0);
        createInternalTriggerParameter("Pan1", 0.0, -1.0, 1.0);
        createInternalTriggerParameter("Pan2", 0.0, -1.0, 1.0);
        createInternalTriggerParameter("PanRise", 0.0, 0, 3.0); // range check
    }

    //    void reset(){ env.reset(); }

    float operator()()
    {
        return (*this)(noise() * env());
    }
    float operator()(float in)
    {
        return delay(
            fil(delay() + in));
    }

    virtual void onProcess(AudioIOData &io) override
    {

        while (io())
        {
            mPan.pos(mPanEnv());
            float s1 = (*this)() * mAmpEnv() * mAmp;
            float s2;
            mEnvFollow(s1);
            mPan(s1, s1, s2);
            io.out(0) += s1;
            io.out(1) += s2;
            // STFT for each notes
            if (stft(s1))
            { // Loop through all the frequency bins
                for (unsigned k = 0; k < stft.numBins(); ++k)
                {
                    // Here we simply scale the complex sample
                    spectrum[k] = tanh(pow(stft.bin(k).real(), 1.3));
                }
            }
        }
        if (mAmpEnv.done() && (mEnvFollow.value() < 0.001))
            free();
    }

    virtual void onProcess(Graphics &g) override
    {
        float frequency = getInternalParameterValue("frequency");
        float amplitude = getInternalParameterValue("amplitude");
        a += 0.6;
        b += 0.28;
        timepose -= 0.09;

        mSpectrogram.reset();
        // mSpectrogram.primitive(Mesh::LINE_STRIP);

        for (int i = 0; i < FFT_SIZE / 2; i++)
        {
            // mSpectrogram.color(HSV(spectrum[i] * 1000 + al::rnd::uniform()));
            mSpectrogram.color(HSV(frequency/800));
            mSpectrogram.vertex(3*i, 5*spectrum[i], 0.0);
        }
        g.meshColor(); // Use the color in the mesh
        g.pushMatrix();
        g.translate(0, -0.5, -10);
        g.rotate(a, Vec3f(0, 1, 0));
        g.rotate(b, Vec3f(1));
        g.scale(30.0 / FFT_SIZE, 500, 1.0);
        g.draw(mSpectrogram);
        g.popMatrix();
    }

    virtual void onTriggerOn() override
    {
        mAmpEnv.reset();
        timepose = 10;
        updateFromParameters();
        env.reset();
        delay.zero();
        mPanEnv.reset();
    }

    virtual void onTriggerOff() override
    {
        mAmpEnv.triggerRelease();
    }

    void updateFromParameters()
    {
        mPanEnv.levels(getInternalParameterValue("Pan1"),
                       getInternalParameterValue("Pan2"),
                       getInternalParameterValue("Pan1"));
        mPanRise = getInternalParameterValue("PanRise");
        delay.freq(getInternalParameterValue("frequency"));
        mAmp = getInternalParameterValue("amplitude");
        mAmpEnv.levels()[1] = 1.0;
        mAmpEnv.levels()[2] = getInternalParameterValue("sustain");
        mAmpEnv.lengths()[0] = getInternalParameterValue("attackTime");
        mAmpEnv.lengths()[3] = getInternalParameterValue("releaseTime");
        mPanEnv.lengths()[0] = mPanRise;
        mPanEnv.lengths()[1] = mPanRise;
    }
};

// We make an app.
class MyApp : public App, public MIDIMessageHandler
{
public:
  SynthGUIManager<SineEnv> synthManager{"SineEnv"};
  // GUI manager for SineEnv voices
  // The name provided determines the name of the directory
  // where the presets and sequences are stored
  // ParameterMIDI parameterMIDI;
  RtMidiIn midiIn; // MIDI input carrier

  bool showGUI = true;
  bool showSpectro = true;
  bool navi = false;
  gam::STFT stft = gam::STFT(FFT_SIZE, FFT_SIZE / 4, 0, gam::HANN, gam::MAG_FREQ);

  void onInit() override
    {
        imguiInit();
        // Set sampling rate for Gamma objects from app's audio
        gam::sampleRate(audioIO().framesPerSecond());
        // Check for connected MIDI devices
        if (midiIn.getPortCount() > 0)
        {
            // Bind ourself to the RtMidiIn object, to have the onMidiMessage()
            // callback called whenever a MIDI message is received
            MIDIMessageHandler::bindTo(midiIn);

            // Open the last device found
            unsigned int port = midiIn.getPortCount() - 1;
            midiIn.openPort(port);
            printf("Opened port to %s\n", midiIn.getPortName(port).c_str());
        }
        else
        {
            printf("Error: No MIDI devices found.\n");
        }
    }
  void onMIDIMessage(const MIDIMessage &m)
    {
      switch (m.type())
      {
      case MIDIByte::NOTE_ON:
      {
        int midiNote = m.noteNumber();
        if (midiNote > 0 && m.velocity() > 0.001)
        {
          synthManager.voice()->setInternalParameterValue(
              "frequency", ::pow(2.f, (midiNote - 69.f) / 12.f) * 432.f);
          synthManager.voice()->setInternalParameterValue(
              "attackTime", 0.01 / m.velocity());
          synthManager.triggerOn(midiNote);
        }
        else
        {
          synthManager.triggerOff(midiNote);
        }
        break;
      }
      case MIDIByte::NOTE_OFF:
      {
        int midiNote = m.noteNumber();
        printf("Note OFF %u, Vel %f", m.noteNumber(), m.velocity());
        synthManager.triggerOff(midiNote);
        break;
      }
      default:;
      }
    }

  // This function is called right after the window is created
  // It provides a grphics context to initialize ParameterGUI
  // It's also a good place to put things that should
  // happen once at startup.
  void onCreate() override {
    navControl().active(false); // Disable navigation via keyboard, since we
                                // will be using keyboard for note triggering

    // Set sampling rate for Gamma objects from app's audio
    gam::sampleRate(audioIO().framesPerSecond());

    imguiInit();

    // Play example sequence. Comment this line to start from scratch
    playTune();
    // synthManager.synthSequencer().playSequence("synth1.synthSequence");
    synthManager.synthRecorder().verbose(true);
  }

  // The audio callback function. Called when audio hardware requires data
  void onSound(AudioIOData &io) override {
    synthManager.render(io); // Render audio
  }

  void onAnimate(double dt) override {
    // The GUI is prepared here
    imguiBeginFrame();
    // Draw a window that contains the synth control panel
    synthManager.drawSynthControlPanel();
    imguiEndFrame();
  }

  // The graphics callback function.
  void onDraw(Graphics &g) override {
    g.clear();
    // Render the synth's graphics
    synthManager.render(g);

    // GUI is drawn here
    imguiDraw();
  }

  // Whenever a key is pressed, this function is called
  bool onKeyDown(Keyboard const &k) override {
    if (ParameterGUI::usingKeyboard()) { // Ignore keys if GUI is using
                                         // keyboard
      return true;
    }
    if (k.shift()) {
      // If shift pressed then keyboard sets preset
      int presetNumber = asciiToIndex(k.key());
      synthManager.recallPreset(presetNumber);
    } else {
      // Otherwise trigger note for polyphonic synth
      int midiNote = asciiToMIDI(k.key());
      if (midiNote > 0) {
        synthManager.voice()->setInternalParameterValue(
            "frequency", ::pow(2.f, (midiNote - 69.f) / 12.f) * 440.f);
        synthManager.triggerOn(midiNote);
      }
    }
    return true;
  }

  // Whenever a key is released this function is called
  bool onKeyUp(Keyboard const &k) override {
    int midiNote = asciiToMIDI(k.key());
    if (midiNote > 0) {
      synthManager.triggerOff(midiNote);
    }
    return true;
  }

  void onExit() override { imguiShutdown(); }


  void playSineEnv(float freq, float time, float duration, float amp = .001, float attack = 0.3, float release = 0.3)
  {
    auto *voice = synthManager.synth().getVoice<SineEnv>();
    // amp, freq, attack, release, pan
    vector<float> params = vector<float>({amp, freq, attack, release ,0.0});
    voice->setTriggerParams(params);
    synthManager.synthSequencer().addVoiceFromNow(voice, time, duration);
  }

  void playSineEnv2(float freq, float time, float duration, float amp = .001, float attack = 0.3, float release = 0.2)
  {
    auto *voice = synthManager.synth().getVoice<SineEnv2>();
    // amp, freq, attack, release, pan
    vector<float> params = vector<float>({amp, freq, attack, release ,0.0});
    voice->setTriggerParams(params);
    synthManager.synthSequencer().addVoiceFromNow(voice, time, duration);
  }

  void playSineEnv3(float freq, float time, float duration, float amp = .001, float attack = 0.3, float release = 0.1)
  {
    auto *voice = synthManager.synth().getVoice<SineEnv3>();
    // amp, freq, attack, release, pan
    vector<float> params = vector<float>({amp, freq, attack, release ,0.0});
    voice->setTriggerParams(params);
    synthManager.synthSequencer().addVoiceFromNow(voice, time, duration);
  }

  void playSquareWave(float freq, float time, float duration, float amp = .07, float attack = 0.1, float release = 0.2)
  {
    auto *voice = synthManager.synth().getVoice<SquareWave>();
    // amp, freq, attack, release, pan
    vector<float> params = vector<float>({amp, freq, attack, release, 0.0});
    voice->setTriggerParams(params);
    synthManager.synthSequencer().addVoiceFromNow(voice, time, duration);
  }

  void playPluckString(float freq, float time, float duration, float amp = 0.5)
  {
    
    auto *voice = synthManager.synth().getVoice<PluckedString>();
    //vector<VariantValue> params = vector<VariantValue>({amp, freq, attack, decay, 0.0});
    //voice->setTriggerParams(params);

    voice->setInternalParameterValue("frequency", freq);
    voice->setInternalParameterValue("amplitude", amp);

    synthManager.synthSequencer().addVoiceFromNow(voice, time, duration);
  }

  void playTune(){
        // read json file
        std::ifstream f("/Users/architgupta/allolib/demo1-archit-gpt/tutorials/synthesis/interstellar.json");
        // cout << "FILE" << endl;
        // cout << f.rdbuf();
        json music = json::parse(f);

        json violin = music["tracks"][0]["notes"];
        json ensemble_violin = music["tracks"][1]["notes"];
        json piano = music["tracks"][2]["notes"];
        json bass_piano = music["tracks"][3]["notes"];
        // json notes = music["tracks"];

    auto v = violin.begin();
    auto ev = ensemble_violin.begin();
    auto p = piano.begin();
    auto bp = bass_piano.begin();


    while(v != violin.end() || ev != ensemble_violin.end() || p != piano.end() || bp != bass_piano.end())
    {
        if(v != violin.end())
        {
            auto note = *v;
            playSineEnv(freq_of(note["midi"]), note["time"], note["duration"], note["velocity"]);
            ++v;
        }
        if(p != piano.end())
        {
            auto note = *p;
            playSineEnv2(freq_of(note["midi"]), note["time"], note["duration"], note["velocity"]);
            ++p;
        }
        if(bp != bass_piano.end())
        {
            auto note = *bp;
            playSineEnv3(freq_of(note["midi"]), note["time"], note["duration"], note["velocity"]);
            ++bp;
        }
        if(ev != ensemble_violin.end())
        {
            auto note = *ev;
            playPluckString(freq_of(note["midi"]), note["time"], note["duration"], note["velocity"]);
            ++ev;
        }
    }

    }

};

int main() {
  // Create app instance
  MyApp app;

  // Set up audio
  app.configureAudio(48000., 512, 2, 0);

  app.start();
  return 0;
}