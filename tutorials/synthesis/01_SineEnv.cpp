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

// This example shows how to use SynthVoice and SynthManagerto create an audio
// visual synthesizer. In a class that inherits from SynthVoice you will
// define the synth's voice parameters and the sound and graphic generation
// processes in the onProcess() functions.

class SineEnv : public SynthVoice {
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
  void init() override {
    // Intialize envelope
    mAmpEnv.curve(0); // make segments lines
    mAmpEnv.levels(0, 1, 1, 0);
    mAmpEnv.sustainPoint(2); // Make point 2 sustain until a release is issued

    // We have the mesh be a sphere
    addRect(mMesh, 1.0, 1.0);

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
  void onProcess(AudioIOData &io) override {
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
    while (io()) {
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
      free();
  }

  // The graphics processing function
  void onProcess(Graphics &g) override {
    // Get the paramter values on every video frame, to apply changes to the
    // current instance
    float frequency = getInternalParameterValue("frequency");
    float midiNote = round(12 * log(frequency / 440.0) / log(2)) + 69;

    g.pushMatrix();

    g.translate(0.05 * (midiNote - 64), 0.0, -4);
    g.scale(0.05, mEnvFollow.value() * 128.0, 1);
    g.color(0.0, midiNote / 128, (1.0 - midiNote / 128) * (1.0 - midiNote / 128));
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
class MyApp : public App {
public:
  // GUI manager for SineEnv voices
  // The name provided determines the name of the directory
  // where the presets and sequences are stored
  SynthGUIManager<SineEnv> synthManager{"SineEnv"};

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
    synthManager.synthSequencer().playSequence("raining.synthSequence");
    synthManager.synthRecorder().verbose(true);
    synthManager.synthSequencer().setTime(0.0f);
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

    synthManager.render(g);
  }

  void onExit() override { imguiShutdown(); }
};

int main() {
  // Create app instance
  MyApp app;

  // Set up audio
  app.configureAudio(48000., 512, 2, 0);

  app.start();
  return 0;
}
