// Recreation of Love Yourz by J Cole

#include <cstdio> // for printing to stdout

#include "Gamma/Analysis.h"
#include "Gamma/Effects.h"
#include "Gamma/Envelope.h"
#include "Gamma/Oscillator.h"

#include "al/app/al_App.hpp"
#include "al/graphics/al_Shapes.hpp"
#include "al/scene/al_PolySynth.hpp"
#include "al/scene/al_SynthSequencer.hpp"
#include "al/ui/al_ControlGUI.hpp"
#include "al/ui/al_Parameter.hpp"

// using namespace gam;
using namespace al;
using namespace std;

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
    // gam::ReverbMS<> reverb;	// Schroeder reverberator
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
    // Unit generators
    gam::Pan<> mPan;
    gam::Sine<> mOsc;
    gam::Env<3> mAmpEnv;
    // envelope follower to connect audio output to graphics
    gam::EnvFollow<> mEnvFollow;

    // Additional members
    Mesh mMesh;

    // Initialize voice. This function will only be called once per voice when
    // it is created. Voices will be reused if they are idle.
    void init() override
    {
        // Intialize envelope
        mAmpEnv.curve(0); // make segments lines
        mAmpEnv.levels(0, 1, 1, 0);
        mAmpEnv.sustainPoint(2); // Make point 2 sustain until a release is issued

        // We have the mesh be a sphere
        addDisc(mMesh, 1.0, 30);

        // This is a quick way to create parameters for the voice. Trigger
        // parameters are meant to be set only when the voice starts, i.e. they
        // are expected to be constant within a voice instance. (You can actually
        // change them while you are prototyping, but their changes will only be
        // stored and aplied when a note is triggered.)

        createInternalTriggerParameter("amplitude", 0.3, 0.0, 1.0);
        createInternalTriggerParameter("frequency", 60, 20, 5000);
        createInternalTriggerParameter("attackTime", 1.0, 0.01, 3.0);
        createInternalTriggerParameter("releaseTime", 3.0, 0.1, 10.0);
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
        mOsc.freq(getInternalParameterValue("frequency"));
        mAmpEnv.lengths()[0] = getInternalParameterValue("attackTime");
        mAmpEnv.lengths()[2] = getInternalParameterValue("releaseTime");
        mPan.pos(getInternalParameterValue("pan"));
        while (io())
        {
            float s1 = mOsc() * mAmpEnv() * getInternalParameterValue("amplitude");
            float s2;
            mEnvFollow(s1);
            mPan(s1, s1, s2);
            io.out(0) += s1;
            io.out(1) += s2;
        }
        // We need to let the synth know that this voice is done
        // by calling the free(). This takes the voice out of the
        // rendering chain
        if (mAmpEnv.done() && (mEnvFollow.value() < 0.001f))
        {
            free();
        }
    }

    // The graphics processing function
    void onProcess(Graphics &g) override
    {
        // Get the paramter values on every video frame, to apply changes to the
        // current instance
        float frequency = getInternalParameterValue("frequency");
        float amplitude = getInternalParameterValue("amplitude");
        // Now draw
        g.pushMatrix();
        // Move x according to frequency, y according to amplitude
        g.translate(frequency / 200 - 3, amplitude, -8);
        // Scale in the x and y directions according to amplitude
        g.scale(1 - amplitude, amplitude, 1);
        // Set the color. Red and Blue according to sound amplitude and Green
        // according to frequency. Alpha fixed to 0.4
        g.color(mEnvFollow.value(), frequency / 1000, mEnvFollow.value() * 10, 0.4);
        g.draw(mMesh);
        g.popMatrix();
    }

    // The triggering functions just need to tell the envelope to start or release
    // The audio processing function checks when the envelope is done to remove
    // the voice from the processing chain.
    void onTriggerOn() override { mAmpEnv.reset(); }

    void onTriggerOff() override { mAmpEnv.release(); }
};

// We make an app.
class MyApp : public App
{
public:
    // GUI manager for SineEnv voices
    // The name provided determines the name of the directory
    // where the presets and sequences are stored
    SynthGUIManager<SineEnv> synthManager{"SineEnv"};

    // This function is called right after the window is created
    // It provides a grphics context to initialize ParameterGUI
    // It's also a good place to put things that should
    // happen once at startup.
    void onCreate() override
    {
        navControl().active(false); // Disable navigation via keyboard, since we
                                    // will be using keyboard for note triggering

        // Set sampling rate for Gamma objects from app's audio
        gam::sampleRate(audioIO().framesPerSecond());

        imguiInit();

        // Play example sequence. Comment this line to start from scratch
        playFullSong();
        // synthManager.synthSequencer().playSequence("synth1.synthSequence");
        synthManager.synthRecorder().verbose(true);
    }

    // The audio callback function. Called when audio hardware requires data
    void onSound(AudioIOData &io) override
    {
        synthManager.render(io); // Render audio
    }

    void onAnimate(double dt) override
    {
        // The GUI is prepared here
        imguiBeginFrame();
        // Draw a window that contains the synth control panel
        synthManager.drawSynthControlPanel();
        imguiEndFrame();
    }

    // The graphics callback function.
    void onDraw(Graphics &g) override
    {
        g.clear();
        // Render the synth's graphics
        synthManager.render(g);

        // GUI is drawn here
        imguiDraw();
    }

    // Whenever a key is pressed, this function is called
    bool onKeyDown(Keyboard const &k) override
    {
        if (ParameterGUI::usingKeyboard())
        { // Ignore keys if GUI is using
          // keyboard
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
        return true;
    }

    // Whenever a key is released this function is called
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
    
    void playNote(float freq, float time, float duration, float amp = .2, float attack = 0.01, float decay = 0.01)
    {
        auto *voice = synthManager.synth().getVoice<SineEnv>();
        // amp, freq, attack, release, pan
        vector<VariantValue> params = vector<VariantValue>({amp, freq, 0.1, 0.1, 0.0});
        voice->setTriggerParams(params);
        synthManager.synthSequencer().addVoiceFromNow(voice, time, duration);
    }

    void playKick(float freq, float time, float duration = 0.5, float amp = 0.4, float attack = 0.01, float decay = 0.1)
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

void chordSequence1(float sequenceStart, int bpm) {
    float vol = .4;
    float sus = .05;
    
    const float C4 = 261.63;
    const float E4 = 329.63;
    const float G4 = 392.00;
    const float A4 = 440.00;
    const float B4 = 493.88;

    // C major chord
    playNote(C4, sequenceStart, sus, vol);
    playNote(E4, sequenceStart, sus, vol);
    playNote(G4, sequenceStart, sus, vol);

    // A minor chord
    playNote(A4, timeElapsed(bpm, 2) + sequenceStart, sus, vol);
    playNote(C4, timeElapsed(bpm, 2) + sequenceStart, sus, vol);
    playNote(E4, timeElapsed(bpm, 2) + sequenceStart, sus, vol);

    // G major chord
    playNote(G4, timeElapsed(bpm, 4) + sequenceStart, sus, vol);
    playNote(B4, timeElapsed(bpm, 4) + sequenceStart, sus, vol);
    playNote(E4, timeElapsed(bpm, 4) + sequenceStart, sus, vol);
}

void chordSequence2(float sequenceStart, int bpm) {
    float vol = .4;
    float sus = .05;
    
    const float C4 = 261.63;
    const float E4 = 329.63;
    const float G4 = 392.00;
    const float A4 = 440.00;
    const float B4 = 493.88;

    // C major chord
    playNote(C4, sequenceStart, sus, vol);
    playNote(E4, sequenceStart, sus, vol);
    playNote(G4, sequenceStart, sus, vol);

    // A minor chord
    playNote(A4, timeElapsed(bpm, 2) + sequenceStart, sus, vol);
    playNote(C4, timeElapsed(bpm, 2) + sequenceStart, sus, vol);
    playNote(E4, timeElapsed(bpm, 2) + sequenceStart, sus, vol);

    // G major chord
    playNote(G4, timeElapsed(bpm, 4) + sequenceStart, sus, vol);
    playNote(B4, timeElapsed(bpm, 4) + sequenceStart, sus, vol);
    playNote(E4, timeElapsed(bpm, 4) + sequenceStart, sus, vol);
}       

void melody1(float sequenceStart, int bpm) {
    float vol = 2;
    const float A4 = 440.00;
    const float B4 = 493.88;
    const float C5 = 523.25;
    const float D5 = 587.33;
    const float E5 = 659.25;
    float sus = .03;
    playNote(A4, sequenceStart, sus, vol);
    playNote(B4, timeElapsed(bpm, 0.5) + sequenceStart, sus, vol);
    playNote(C5, timeElapsed(bpm, 0.75) + sequenceStart, sus, vol);
    playNote(D5, timeElapsed(bpm, 1) + sequenceStart, sus, vol);
    playNote(E5, timeElapsed(bpm, 1.25) + sequenceStart, sus, vol);
    playNote(E5, timeElapsed(bpm, 1.5) + sequenceStart, sus, vol);
    playNote(D5, timeElapsed(bpm, 1.75) + sequenceStart, sus, vol);
    playNote(C5, timeElapsed(bpm, 2) + sequenceStart, sus, vol);
    playNote(B4, timeElapsed(bpm, 2.25) + sequenceStart, sus, vol);
    playNote(A4, timeElapsed(bpm, 2.5) + sequenceStart, sus, vol);
}

void melody2(float sequenceStart, int bpm) {
    float vol = 2;
    const float A4 = 440.00;
    const float B4 = 493.88;
    const float C5 = 523.25;
    const float D5 = 587.33;
    const float E5 = 659.25;
    float sus = .03;

    playNote(A4, sequenceStart, sus, vol);
    playNote(B4, timeElapsed(bpm, 0.5) + sequenceStart, sus, vol);
    playNote(C5, timeElapsed(bpm, 0.75) + sequenceStart, sus, vol);
    playNote(D5, timeElapsed(bpm, 1) + sequenceStart, sus, vol);
    playNote(E5, timeElapsed(bpm, 1.25) + sequenceStart, sus, vol);
    playNote(E5, timeElapsed(bpm, 1.5) + sequenceStart, sus, vol);
    playNote(D5, timeElapsed(bpm, 1.75) + sequenceStart, sus, vol);
    playNote(C5, timeElapsed(bpm, 2) + sequenceStart, sus, vol);
    playNote(B4, timeElapsed(bpm, 2.25) + sequenceStart, sus, vol);
    playNote(A4, timeElapsed(bpm, 2.5) + sequenceStart, sus, vol);
}

// Add a new function for a new melody
    void melody3(float sequenceStart, int bpm) {
        float vol = 2;
        const float F4 = 349.23;
        const float G4 = 392.00;
        const float A4 = 440.00;
        const float B4 = 493.88;
        const float C5 = 523.25;
        float sus = .03;

        playNote(F4, sequenceStart, sus, vol);
        playNote(G4, timeElapsed(bpm, 0.5) + sequenceStart, sus, vol);
        playNote(A4, timeElapsed(bpm, 0.75) + sequenceStart, sus, vol);
        playNote(B4, timeElapsed(bpm, 1) + sequenceStart, sus, vol);
        playNote(C5, timeElapsed(bpm, 1.25) + sequenceStart, sus, vol);
        playNote(C5, timeElapsed(bpm, 1.5) + sequenceStart, sus, vol);
        playNote(B4, timeElapsed(bpm, 1.75) + sequenceStart, sus, vol);
        playNote(A4, timeElapsed(bpm, 2) + sequenceStart, sus, vol);
        playNote(G4, timeElapsed(bpm, 2.25) + sequenceStart, sus, vol);
        playNote(F4, timeElapsed(bpm, 2.5) + sequenceStart, sus, vol);
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
        playKick(150, sequenceStart);
        playKick(150, timeElapsed(bpm, 1.5) + sequenceStart);
        playKick(150, timeElapsed(bpm, 2) + sequenceStart);
        playKick(150, timeElapsed(bpm, 3) + sequenceStart);
        playKick(150, timeElapsed(bpm, 3.5) + sequenceStart);
    }

    void snareBeat(float sequenceStart, int bpm) {
    float beatDuration = 60.0 / bpm;

    // Play the snare on beats 2 and 4
    playSnare(sequenceStart + beatDuration);
    playSnare(sequenceStart + 3 * beatDuration);
    }


    void playBass(float sequenceStart, int bpm) {
        float vol = 0.3; // Further reduce the bass volume
        float sus = 0.15; // Shorten the sustain
        float rel = 0.1; // Add a release parameter to make the notes smoother

        const float E2 = 82.41;
        const float A2 = 110.00;
        const float B2 = 123.47;
        const float C3 = 130.81;
        const float D3 = 146.83;

        // E minor scale
        playNote(E2, sequenceStart, sus, vol, rel);
        playNote(B2, timeElapsed(bpm, 0.75) + sequenceStart, sus, vol, rel);
        playNote(A2, timeElapsed(bpm, 1.5) + sequenceStart, sus, vol, rel);

        // A minor scale
        playNote(A2, timeElapsed(bpm, 2) + sequenceStart, sus, vol, rel);
        playNote(E2, timeElapsed(bpm, 2.75) + sequenceStart, sus, vol, rel);
        playNote(C3, timeElapsed(bpm, 3.5) + sequenceStart, sus, vol, rel);
    }


    void playEnding(float sequenceStart, int bpm) {
        float vol = 2;
        float sus = .05;

        const float C5 = 523.25;
        const float D5 = 587.33;
        const float E5 = 659.25;
        const float G5 = 783.99;
        const float B5 = 987.77;

        // Play a sequence of notes for the ending
        playNote(E5, sequenceStart, sus, vol);
        playNote(G5, timeElapsed(bpm, 0.5) + sequenceStart, sus, vol);
        playNote(B5, timeElapsed(bpm, 1) + sequenceStart, sus, vol);
        playNote(C5, timeElapsed(bpm, 1.5) + sequenceStart, sus, vol);
        playNote(D5, timeElapsed(bpm, 2) + sequenceStart, sus, vol);

        // Play a final chord to signify the end of the piece
        chordSequence1(timeElapsed(bpm, 2.5) + sequenceStart, bpm);
    }   

    void playFullSong() {
        int bpm = 60; 

        for (int i = 0; i < 16; ++i) { 
            float sequenceStart = timeElapsed(bpm, i * 4);

            // Play the chords
            chordSequence1(sequenceStart, bpm);
            chordSequence2(timeElapsed(bpm, 2) + sequenceStart, bpm);

            // Cycle through the melodies
            if (i % 4 == 0) {
                melody1(timeElapsed(bpm, 0.75) + sequenceStart, bpm);
            } else if (i % 4 == 1) {
                melody2(timeElapsed(bpm, 0.75) + sequenceStart, bpm);
            } else {
                melody3(timeElapsed(bpm, 0.75) + sequenceStart, bpm); // New melody
            }

            // Add drums
            metronome(16, bpm);
            kickBeat(sequenceStart, bpm);
            hihatBeat(timeElapsed(bpm, 0.5) + sequenceStart, bpm); 

            // Play the bass
            playBass(sequenceStart, bpm);

            // Add variation to the second stanza
            if (i >= 4) {
                hihatBeat(timeElapsed(bpm, 1) + sequenceStart, bpm);
                hihatBeat(timeElapsed(bpm, 1.5) + sequenceStart, bpm);
                snareBeat(timeElapsed(bpm, 2) + sequenceStart, bpm);
            }

            // Add a second ending after the 8th measure to vary the structure
            if (i == 8 || i == 9 || i == 10 || i == 11) {
                float midEndingStart = timeElapsed(bpm, 4 * 8);
                playEnding(midEndingStart, bpm);
            }
            
        }

        // Add ending to signify the completion of the piece
        float endingStart = timeElapsed(bpm, 4 * 16);
        playEnding(endingStart, bpm);
    }

};
        

int main()
{
    // Create app instance
    MyApp app;

    // Set up audio
    app.configureAudio(48000., 512, 2, 0);

    app.start();
    return 0;
}