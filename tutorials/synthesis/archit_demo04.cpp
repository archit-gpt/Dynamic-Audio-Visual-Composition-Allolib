// MUS109IA & MAT276IA.
// Spring 2022
// Course Instrument 09. Plucked String synthesis-Visual (Mesh & Spectrum)
// Press '[' or ']' to turn on & off GUI
// Able to play with MIDI device
// Myungin Lee
#include <cstdio> // for printing to stdout
#include <cmath>
#include <vector>
#include <iostream>

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

class Kick : public SynthVoice
{
public:
    // Unit generators
    gam::Pan<> mPan;
    gam::Sine<> mOsc;
    gam::Decay<> mDecay; // Added decay envelope for pitch
    gam::AD<> mAmpEnv;   // Changed amp envelope from Env<3> to AD<>

    void init() override
    {
        // Intialize amplitude envelope
        // - Minimum attack (to make it thump)
        // - Short decay
        // - Maximum amplitude
        mAmpEnv.attack(0.01);
        mAmpEnv.decay(0.3);
        mAmpEnv.amp(1.0);

        // Initialize pitch decay
        mDecay.decay(0.3);

        createInternalTriggerParameter("amplitude", 0.5, 0.0, 1.0);
        createInternalTriggerParameter("frequency", 150, 20, 5000);
    }

    // The audio processing function
    void onProcess(AudioIOData &io) override
    {
        mOsc.freq(getInternalParameterValue("frequency"));
        mPan.pos(0);
        // (removed parameter control for attack and release)

        while (io())
        {
            mOsc.freqMul(mDecay()); // Multiply pitch oscillator by next decay value
            float s1 = mOsc() * mAmpEnv() * getInternalParameterValue("amplitude");
            float s2;
            mPan(s1, s1, s2);
            io.out(0) += s1;
            io.out(1) += s2;
        }

        if (mAmpEnv.done())
        {
            free();
        }
    }

    void onTriggerOn() override
    {
        mAmpEnv.reset();
        mDecay.reset();
    }

    void onTriggerOff() override
    {
        mAmpEnv.release();
        mDecay.finish();
    }
};

class Snare : public SynthVoice
{
public:
    // Unit generators
    gam::Pan<> mPan;
    gam::AD<> mAmpEnv;   // Amplitude envelope
    gam::Sine<> mOsc;    // Main pitch osc (top of drum)
    gam::Sine<> mOsc2;   // Secondary pitch osc (bottom of drum)
    gam::Decay<> mDecay; // Pitch decay for oscillators
    // gam::ReverbMS<> reverb;  // Schroeder reverberator
    gam::Burst mBurst; // Noise to simulate rattle/chains

    void init() override
    {
        // Initialize burst
        mBurst = gam::Burst(10000, 5000, 0.1);
        // editing last number of burst shortens/makes sound snappier

        // Initialize amplitude envelope
        mAmpEnv.attack(0.01);
        mAmpEnv.decay(0.01);
        mAmpEnv.amp(0.005);

        // Initialize pitch decay
        mDecay.decay(0.1);

        // reverb.resize(gam::FREEVERB);
        // reverb.decay(0.5); // Set decay length, in seconds
        // reverb.damping(0.2); // Set high-frequency damping factor in [0, 1]
    }

    // The audio processing function
    void onProcess(AudioIOData &io) override
    {
        mOsc.freq(200);
        mOsc2.freq(150);

        while (io())
        {

            // Each mDecay() call moves it forward (I think), so we only want
            // to call it once per sample
            float decay = mDecay();
            mOsc.freqMul(decay);
            mOsc2.freqMul(decay);

            float amp = mAmpEnv();
            float s1 = mBurst() + (mOsc() * amp * 0.1) + (mOsc2() * amp * 0.05);
            // s1 += reverb(s1) * 0.2;
            float s2;
            mPan(s1, s1, s2);
            io.out(0) += s1;
            io.out(1) += s2;
        }

        if (mAmpEnv.done())
            free();
    }
    void onTriggerOn() override
    {
        mBurst.reset();
        mAmpEnv.reset();
        mDecay.reset();
    }

    void onTriggerOff() override
    {
        mAmpEnv.release();
        mDecay.finish();
    }
};

class Hihat : public SynthVoice
{
public:
    // Unit generators
    gam::Pan<> mPan;
    gam::AD<> mAmpEnv; // Changed amp envelope from Env<3> to AD<>

    gam::Burst mBurst; // Resonant noise with exponential decay

    void init() override
    {
        // Initialize burst - Main freq, filter freq, duration
        mBurst = gam::Burst(20000, 15000, 0.05);
    }

    // The audio processing function
    void onProcess(AudioIOData &io) override
    {
        while (io())
        {
            float s1 = mBurst();
            float s2;
            mPan(s1, s1, s2);
            io.out(0) += s1;
            io.out(1) += s2;
        }
        // Left this in because I'm not sure how to tell when a burst is done
        if (mAmpEnv.done())
            free();
    }
    void onTriggerOn() override { mBurst.reset(); }
    // void onTriggerOff() override {  }
};

class SineEnv : public SynthVoice
{
public:
    float mAmp;
    float mDur;
    float mPanRise;
    gam::Pan<> mPan;
    gam::NoiseWhite<> noise;
    gam::Decay<> env;
    gam::MovingAvg<> fil{2};
    gam::Delay<float, gam::ipl::Trunc> delay;
    gam::ADSR<> mAmpEnv;
    gam::EnvFollow<> mEnvFollow;
    gam::Env<2> mPanEnv;
    gam::STFT stft = gam::STFT(FFT_SIZE, FFT_SIZE / 4, 0, gam::HANN, gam::MAG_FREQ);
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
        delay.delay(1. / 440.0);

        addDisc(mMesh, 1.0, 30);
        createInternalTriggerParameter("amplitude", 0.1, 0.0, 1.0);
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
        a += 0.29;
        b += 0.23;
        timepose -= 0.1;

        mSpectrogram.reset();
        // mSpectrogram.primitive(Mesh::LINE_STRIP);

        for (int i = 0; i < FFT_SIZE / 2; i++)
        {
            mSpectrogram.color(HSV(spectrum[i] * 1000 + al::rnd::uniform()));
            mSpectrogram.vertex(i, spectrum[i], 0.0);
        }
        g.meshColor(); // Use the color in the mesh
        g.pushMatrix();
        g.translate(0, 0, -10);
        g.rotate(a, Vec3f(0, 1, 0));
        g.rotate(b, Vec3f(1));
        g.scale(10.0 / FFT_SIZE, 500, 1.0);
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

class SquareWave : public SynthVoice
{
public:
  // Unit generators
  gam::Pan<> mPan;
  gam::Sine<> mOsc1;
  gam::Sine<> mOsc3;
  gam::Sine<> mOsc5;

  gam::Env<3> mAmpEnv;

  // Initialize voice. This function will only be called once per voice when
  // it is created. Voices will be reused if they are idle.
  void init() override
  {
    // Intialize envelope
    mAmpEnv.curve(0); // make segments lines
    mAmpEnv.levels(0, 1, 1, 0);
    mAmpEnv.sustainPoint(2); // Make point 2 sustain until a release is issued

    createInternalTriggerParameter("amplitude", 0.8, 0.0, 1.0);
    createInternalTriggerParameter("frequency", 440, 20, 5000);
    createInternalTriggerParameter("attackTime", 0.1, 0.01, 3.0);
    createInternalTriggerParameter("releaseTime", 0.1, 0.1, 10.0);
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
    float f = getInternalParameterValue("frequency");
    mOsc1.freq(f);
    mOsc3.freq(f * 3);
    mOsc5.freq(f * 5);

    float a = getInternalParameterValue("amplitude");
    mAmpEnv.lengths()[0] = getInternalParameterValue("attackTime");
    mAmpEnv.lengths()[2] = getInternalParameterValue("releaseTime");
    mPan.pos(getInternalParameterValue("pan"));
    while (io())
    {
      float s1 = mAmpEnv() * (mOsc1() * a +
                              mOsc3() * (a / 3.0) +
                              mOsc5() * (a / 5.0));

      float s2;
      mPan(s1, s1, s2);
      io.out(0) += s1;
      io.out(1) += s2;
    }
    // We need to let the synth know that this voice is done
    // by calling the free(). This takes the voice out of the
    // rendering chain
    if (mAmpEnv.done())
      free();
  }

  // The triggering functions just need to tell the envelope to start or release
  // The audio processing function checks when the envelope is done to remove
  // the voice from the processing chain.
  void onTriggerOn() override { mAmpEnv.reset(); }
  void onTriggerOff() override { mAmpEnv.release(); }
};

class Sub : public SynthVoice {
public:

    // Unit generators
    float mNoiseMix;
    gam::Pan<> mPan;
    gam::ADSR<> mAmpEnv;
    gam::EnvFollow<> mEnvFollow;  // envelope follower to connect audio output to graphics
    gam::DSF<> mOsc;
    gam::NoiseWhite<> mNoise;
    gam::Reson<> mRes;
    gam::Env<2> mCFEnv;
    gam::Env<2> mBWEnv;
    // Additional members
    Mesh mMesh;

    // Initialize voice. This function will nly be called once per voice
    void init() override {
        mAmpEnv.curve(0); // linear segments
        mAmpEnv.levels(0,1.0,1.0,0); // These tables are not normalized, so scale to 0.3
        mAmpEnv.sustainPoint(2); // Make point 2 sustain until a release is issued
        mCFEnv.curve(0);
        mBWEnv.curve(0);
        mOsc.harmonics(12);
        // We have the mesh be a sphere
        addDisc(mMesh, 1.0, 30);

        createInternalTriggerParameter("amplitude", 0.3, 0.0, 1.0);
        createInternalTriggerParameter("frequency", 60, 20, 5000);
        createInternalTriggerParameter("attackTime", 0.1, 0.01, 3.0);
        createInternalTriggerParameter("releaseTime", 3.0, 0.1, 10.0);
        createInternalTriggerParameter("sustain", 0.7, 0.0, 1.0);
        createInternalTriggerParameter("curve", 4.0, -10.0, 10.0);
        createInternalTriggerParameter("noise", 0.0, 0.0, 1.0);
        createInternalTriggerParameter("envDur",1, 0.0, 5.0);
        createInternalTriggerParameter("cf1", 400.0, 10.0, 5000);
        createInternalTriggerParameter("cf2", 400.0, 10.0, 5000);
        createInternalTriggerParameter("cfRise", 0.5, 0.1, 2);
        createInternalTriggerParameter("bw1", 700.0, 10.0, 5000);
        createInternalTriggerParameter("bw2", 900.0, 10.0, 5000);
        createInternalTriggerParameter("bwRise", 0.5, 0.1, 2);
        createInternalTriggerParameter("hmnum", 12.0, 5.0, 20.0);
        createInternalTriggerParameter("hmamp", 1.0, 0.0, 1.0);
        createInternalTriggerParameter("pan", 0.0, -1.0, 1.0);

    }

    //
    
    virtual void onProcess(AudioIOData& io) override {
        updateFromParameters();
        float amp = getInternalParameterValue("amplitude");
        float noiseMix = getInternalParameterValue("noise");
        while(io()){
            // mix oscillator with noise
            float s1 = mOsc()*(1-noiseMix) + mNoise()*noiseMix;

            // apply resonant filter
            mRes.set(mCFEnv(), mBWEnv());
            s1 = mRes(s1);

            // appy amplitude envelope
            s1 *= mAmpEnv() * amp;

            float s2;
            mPan(s1, s1,s2);
            io.out(0) += s1;
            io.out(1) += s2;
        }
        
        
        if(mAmpEnv.done() && (mEnvFollow.value() < 0.001f)) free();
    }

   virtual void onProcess(Graphics &g) {
          float frequency = getInternalParameterValue("frequency");
          float amplitude = getInternalParameterValue("amplitude");
          g.pushMatrix();
          g.translate(amplitude,  amplitude, -4);
          //g.scale(frequency/2000, frequency/4000, 1);
          float scaling = 0.1;
          g.scale(scaling * frequency/200, scaling * frequency/400, scaling* 1);
          g.color(mEnvFollow.value(), frequency/1000, mEnvFollow.value()* 10, 0.4);
          g.draw(mMesh);
          g.popMatrix();
   }
    virtual void onTriggerOn() override {
        updateFromParameters();
        mAmpEnv.reset();
        mCFEnv.reset();
        mBWEnv.reset();
        
    }

    virtual void onTriggerOff() override {
        mAmpEnv.triggerRelease();
//        mCFEnv.triggerRelease();
//        mBWEnv.triggerRelease();
    }

    void updateFromParameters() {
        mOsc.freq(getInternalParameterValue("frequency"));
        mOsc.harmonics(getInternalParameterValue("hmnum"));
        mOsc.ampRatio(getInternalParameterValue("hmamp"));
        mAmpEnv.attack(getInternalParameterValue("attackTime"));
    //    mAmpEnv.decay(getInternalParameterValue("attackTime"));
        mAmpEnv.release(getInternalParameterValue("releaseTime"));
        mAmpEnv.levels()[1]=getInternalParameterValue("sustain");
        mAmpEnv.levels()[2]=getInternalParameterValue("sustain");

        mAmpEnv.curve(getInternalParameterValue("curve"));
        mPan.pos(getInternalParameterValue("pan"));
        mCFEnv.levels(getInternalParameterValue("cf1"),
                      getInternalParameterValue("cf2"),
                      getInternalParameterValue("cf1"));


        mCFEnv.lengths()[0] = getInternalParameterValue("cfRise");
        mCFEnv.lengths()[1] = 1 - getInternalParameterValue("cfRise");
        mBWEnv.levels(getInternalParameterValue("bw1"),
                      getInternalParameterValue("bw2"),
                      getInternalParameterValue("bw1"));
        mBWEnv.lengths()[0] = getInternalParameterValue("bwRise");
        mBWEnv.lengths()[1] = 1- getInternalParameterValue("bwRise");

        mCFEnv.totalLength(getInternalParameterValue("envDur"));
        mBWEnv.totalLength(getInternalParameterValue("envDur"));
    }
};



class MyApp : public App, public MIDIMessageHandler
{
public:
    SynthGUIManager<SineEnv> synthManager{"SineEnv"};
    //    ParameterMIDI parameterMIDI;
    RtMidiIn midiIn; // MIDI input carrier
    Mesh mSpectrogram;
    vector<float> spectrum;
    bool showGUI = true;
    bool showSpectro = true;
    bool navi = false;
    gam::STFT stft = gam::STFT(FFT_SIZE, FFT_SIZE / 4, 0, gam::HANN, gam::MAG_FREQ);

    virtual void onInit() override
    {
        imguiInit();
        navControl().active(false); // Disable navigation via keyboard, since we
                                    // will be using keyboard for note triggering
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
        // Declare the size of the spectrum
        spectrum.resize(FFT_SIZE / 2 + 1);
    }

    void playGuitar(float freq, float time, float duration, float amp = 0.4)
    {
        auto *voice = synthManager.synth().getVoice<SineEnv>();

        voice->setInternalParameterValue("frequency", freq);
        voice->setInternalParameterValue("amplitude", amp);
        // voice->setInternalParameterValue("sustain", sus);

        synthManager.synthSequencer().addVoiceFromNow(voice, time, duration);
    }

    void playKick(float freq, float time, float duration = 0.5, float amp = 1.0, float attack = 0.01, float decay = 0.1)
    {
        auto *voice = synthManager.synth().getVoice<Kick>();
        // amp, freq, attack, release, pan
        vector<VariantValue> params = vector<VariantValue>({amp, freq, 0.01, 0.1, 0.0});
        voice->setTriggerParams(params);
        synthManager.synthSequencer().addVoiceFromNow(voice, time, duration);
    }
    void playSnare(float time, float duration = 0.3)
    {
        auto *voice = synthManager.synth().getVoice<Snare>();
        // amp, freq, attack, release, pan
        synthManager.synthSequencer().addVoiceFromNow(voice, time, duration);
    }
    void playHihat(float time, float duration = 0.3)
    {
        auto *voice = synthManager.synth().getVoice<Hihat>();
        // amp, freq, attack, release, pan
        synthManager.synthSequencer().addVoiceFromNow(voice, time, duration);
    }


    float timeElapsed(int bpm, float beatsElapsed)
    {
        return (60 * beatsElapsed) / (bpm);
    }

    void metronome(float sequenceStart, int bpm)
    {
        for (int i = 0; i < 8; i = i + 1)
        { 
            playSnare(((i * 60) / (bpm)) + sequenceStart);
        }
    }

    void hihatBeat(float sequenceStart, int bpm) {
    for (int i = 0; i < 8; ++i) {
        playHihat(timeElapsed(bpm, i * 0.5) + sequenceStart);
        }
    }


    void kickBeat(float sequenceStart, int bpm) {
    for (int i = 0; i < 1; ++i) {
        playKick(150, (timeElapsed(bpm, i * 0.0125) + sequenceStart));
        }
    }

    void snareBeat(float sequenceStart, int bpm) {
    float beatDuration = 60.0 / bpm;

    // Play the snare on beats 2 and 4
    playSnare(sequenceStart + beatDuration);
    playSnare(sequenceStart + 3 * beatDuration);
    }


    void playNote(float freq, float time, float duration, float amp = .1, float attack = 0.1, float decay = 0.2)
    {
        auto *voice = synthManager.synth().getVoice<SquareWave>();
        // amp, freq, attack, release, pan
        vector<VariantValue> params = vector<VariantValue>({amp, freq, attack, decay, 0.0});
        voice->setTriggerParams(params);
        synthManager.synthSequencer().addVoiceFromNow(voice, time, duration);
    }


    void playTune(){
        // read json file
        std::ifstream f("/Users/architgupta/allolib/demo1-archit-gpt/tutorials/synthesis/passionfruit.json");
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
            playNote(freq_of(note["midi"]), note["time"], note["duration"], note["velocity"]);
            ++v;
            hihatBeat(note["time"], 113);
            kickBeat(note["time"], 113/2);
        }
        if(p != piano.end())
        {
            auto note = *p;
            playGuitar(freq_of(note["midi"]), note["time"], note["duration"], note["velocity"]);
            ++p;
        }
        if(bp != bass_piano.end())
        {
            auto note = *bp;
            playGuitar(freq_of(note["midi"]), note["time"], note["duration"], note["velocity"]);
            ++bp;
        }
        if(ev != ensemble_violin.end())
        {
            auto note = *ev;
            playNote(freq_of(note["midi"]), note["time"], note["duration"], note["velocity"]);
            ++ev;
        }
    }

    }

    void onCreate() override
    {
        // Play example sequence. Comment this line to start from scratch
        //    synthManager.synthSequencer().playSequence("synth8.synthSequence");
        synthManager.synthRecorder().verbose(true);

        playTune();
    }

    void onSound(AudioIOData &io) override
    {
        synthManager.render(io); // Render audio
        // STFT
        while (io())
        {
            if (stft(io.out(0)))
            { // Loop through all the frequency bins
                for (unsigned k = 0; k < stft.numBins(); ++k)
                {
                    // Here we simply scale the complex sample
                    spectrum[k] = tanh(pow(stft.bin(k).real(), 1.3));
                    // spectrum[k] = stft.bin(k).real();
                }
            }
        }        
    }

    void onAnimate(double dt) override
    {
        navControl().active(navi); // Disable navigation via keyboard, since we
        imguiBeginFrame();
        synthManager.drawSynthControlPanel();
        imguiEndFrame();
    }

    void onDraw(Graphics &g) override
    {
        g.clear();
        synthManager.render(g);
        // // Draw Spectrum
        mSpectrogram.reset();
        mSpectrogram.primitive(Mesh::LINE_STRIP);
        if (showSpectro)
        {
            for (int i = 0; i < FFT_SIZE / 2; i++)
            {
                mSpectrogram.color(HSV(0.5 - spectrum[i] * 100));
                mSpectrogram.vertex(i, spectrum[i], 0.0);
            }
            g.meshColor(); // Use the color in the mesh
            g.pushMatrix();
            g.translate(-3, -3, 0);
            g.scale(20.0 / FFT_SIZE, 100, 1.0);
            g.draw(mSpectrogram);
            g.popMatrix();
        }
        // Draw GUI
        imguiDraw();
    }
  // This gets called whenever a MIDI message is received on the port
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
    bool onKeyDown(Keyboard const &k) override
    {
        if (ParameterGUI::usingKeyboard())
        { // Ignore keys if GUI is using them
            return true;
        }
        if (k.shift())
        {
            // If shift pressed then keyboard sets preset
            int presetNumber = asciiToIndex(k.key());
            synthManager.recallPreset(presetNumber);
        }
        else
        {
            // Otherwise trigger note for polyphonic synth
            int midiNote = asciiToMIDI(k.key());
            if (midiNote > 0)
            {
                synthManager.voice()->setInternalParameterValue(
                    "frequency", ::pow(2.f, (midiNote - 69.f) / 12.f) * 432.f);
                synthManager.triggerOn(midiNote);
            }
        }
        switch (k.key())
        {
        case ']':
        showGUI = !showGUI;
        break;
        case '[':
        showSpectro = !showSpectro;
        break;
        case '=':
        navi = !navi;
        break;
        }
        return true;
    }

    bool onKeyUp(Keyboard const &k) override
    {
        int midiNote = asciiToMIDI(k.key());
        if (midiNote > 0)
        {
            synthManager.triggerOff(midiNote);
        }
        return true;
    }

    void onExit() override { imguiShutdown(); }
};

int main()
{
    MyApp app;

    // Set up audio
    app.configureAudio(48000., 512, 2, 0);

    app.start();
}

