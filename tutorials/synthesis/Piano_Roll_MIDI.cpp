#include <cstdio> // for printing to stdout

#include "Gamma/Analysis.h"
#include "Gamma/Effects.h"
#include "Gamma/Envelope.h"
#include "Gamma/Oscillator.h"
#include "Gamma/Timer.h"

#include "al/app/al_App.hpp"
#include "al/graphics/al_Shapes.hpp"
#include "al/scene/al_PolySynth.hpp"
#include "al/scene/al_SynthSequencer.hpp"
#include "al/ui/al_ControlGUI.hpp"
#include "al/ui/al_Parameter.hpp"

// using namespace gam;
using namespace al;

int asciiToKeyLabelIndex(int asciiKey) {
  switch (asciiKey) {
  case '2':
    return 30;
  case '3':
    return 31;
  case '5':
    return 33;
  case '6':
    return 34;
  case '7':
    return 35;
  case '9':
    return 37;
  case '0':
    return 38;

  case 'q':
    return 10;
  case 'w':
    return 11;
  case 'e':
    return 12;
  case 'r':
    return 13;
  case 't':
    return 14;
  case 'y':
    return 15;
  case 'u':
    return 16;
  case 'i':
    return 17;
  case 'o':
    return 18;
  case 'p':
    return 19;

  case 's':
    return 20;
  case 'd':
    return 21;
  case 'g':
    return 23;
  case 'h':
    return 24;
  case 'j':
    return 25;
  case 'l':
    return 27;
  case ';':
    return 28;

  case 'z':
    return 0;
  case 'x':
    return 1;
  case 'c':
    return 2;
  case 'v':
    return 3;
  case 'b':
    return 4;
  case 'n':
    return 5;
  case 'm':
    return 6;
  case ',':
    return 7;
  case '.':
    return 8;
  case '/':
    return 9;
  }
  return 0;
}

class SineEnv : public SynthVoice {
public:
  // Unit generators
  gam::Pan<> mPan;
  gam::Sine<> mOsc;
  gam::Env<3> mAmpEnv;
  // envelope follower to connect audio output to graphics
  gam::EnvFollow<> mEnvFollow;
  gam::Timer timer, timer2;

  bool noteStart = false, noteEnd = false;

  // Additional members
  Mesh mMesh, mMesh2, mMesh3;
  
  // Initialize voice. This function will only be called once per voice when
  // it is created. Voices will be reused if they are idle.
  void init() override {
    // Intialize envelope
    mAmpEnv.curve(0); // make segments lines
    mAmpEnv.levels(0, 1, 1, 0);
    mAmpEnv.sustainPoint(2); // Make point 2 sustain until a release is issued

    addRect(mMesh, 1.0, 1.0);
    addRect(mMesh3, 1.0, 1.0);
    addDisc(mMesh2, 0.75);

    // This is a quick way to create parameters for the voice. Trigger
    // parameters are meant to be set only when the voice starts, i.e. they
    // are expected to be constant within a voice instance. (You can actually
    // change them while you are prototyping, but their changes will only be
    // stored and aplied when a note is triggered.)

    createInternalTriggerParameter("amplitude", 0.03, 0.0, 1.0);
    createInternalTriggerParameter("frequency", 60, 20, 5000);
    createInternalTriggerParameter("attackTime", 0.0, 0.01, 3.0);
    createInternalTriggerParameter("releaseTime", 0.0, 0.1, 10.0);
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
    if(mAmpEnv.done() && (mEnvFollow.value() < 0.001f) && !noteEnd) {
      if(getInternalParameterValue("amplitude") > 0)
        free();
      noteEnd = true;
      timer2.start();
    }
  }

  // The graphics processing function
  void onProcess(Graphics &g) override {

    bool fakeNote = getInternalParameterValue("amplitude") == 0;

    if(noteEnd) {
      timer2.stop();
      if(timer2.elapsedSec() > 8) {
        noteStart = false;
        noteEnd = false;
        free();
      }
    }
    else if(!noteStart){
      noteStart = true;
      timer.start();
    } 
    else {
      timer.stop();
    }

    if(timer.elapsedSec() > 8 || timer.elapsed() < 0) timer.start();
    if(timer2.elapsedSec() > 8 || timer2.elapsed() < 0) timer2.start();

    if(timer.elapsedSec() > 8 || timer.elapsed() < 0) timer.stop();
    if(timer2.elapsedSec() > 8 || timer2.elapsed() < 0) timer2.stop();

    float frequency = getInternalParameterValue("frequency");
    float midiNote = round(12 * log(frequency / 440.0) / log(2)) + 69;
    
    if(noteStart && !noteEnd && !fakeNote) {
      g.pushMatrix();
      g.translate(-0.02, 0.025 * (midiNote - 64), -4);
      g.scale(0.02, 0.02, 1);
      g.color(1, 1, 0.5);
      // g.draw(mMesh2);
      g.popMatrix();
    }

    if(fakeNote) {
      float delta1 = timer.elapsedSec() * (noteStart ? 1 : 0);
      float delta2 = timer2.elapsedSec() * (noteEnd ? 1 : 0);
      float delta3 = delta2 - 4 + delta1;
      float delta4 = delta3 >= delta1 ? delta2 - 4: 0;
      g.pushMatrix();

      g.translate(2 - delta1 / 4 - delta2 / 2, 0.025 * (midiNote - 64), -4);
      g.scale(delta1 / 2, 0.02, 1);
      g.color(1, 1, 1);
      g.draw(mMesh);

      g.popMatrix();
      g.pushMatrix();

      g.translate(delta3 > 0 ? 0 - delta3 / 4 - delta4 / 4 : 0, 0.025 * (midiNote - 64), -4);
      g.scale(delta3 > 0 ? std::min(delta3 / 2, delta1 / 2) : 0, 0.02, 1);
      g.color(1, midiNote / 128, (1.0 - midiNote / 128) * (1.0 - midiNote / 128));
      g.draw(mMesh);

      g.popMatrix();
    }
  }

  void onTriggerOn() override { mAmpEnv.reset(); }

  void onTriggerOff() override { mAmpEnv.release(); }
};

class MyApp : public App {
public:
  SynthGUIManager<SineEnv> synthManager{"SineEnv"};
  Mesh aMesh;

  void onCreate() override {
    navControl().active(false);

    gam::sampleRate(audioIO().framesPerSecond());

    imguiInit();

    addRect(aMesh, 1, 1);

    // Play example sequence. Comment this line to start from scratch
    synthManager.synthSequencer().playSequence("pool.synthSequence");
    synthManager.synthRecorder().verbose(true);
    synthManager.synthSequencer().setTime(0.0f);
  }

  // The audio callback function. Called when audio hardware requires data
  void onSound(AudioIOData &io) override {
    synthManager.render(io); // Render audio
  }

  void onAnimate(double dt) override {
  }

  // The graphics callback function.
  void onDraw(Graphics &g) override {
    g.clear();

    g.pushMatrix();
    g.translate(0, 0, -5);
    g.scale(0.01, 4, 1);
    g.color(1, 0, 0);
    g.draw(aMesh);
    g.popMatrix();

    synthManager.render(g);
  }

  void onExit() override { imguiShutdown(); }

  // Whenever a key is pressed, this function is called
  bool onKeyDown(Keyboard const& k) override {
    if (ParameterGUI::usingKeyboard()) {  // Ignore keys if GUI is using
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
        // Check which key is pressed
        int keyIndex = asciiToKeyLabelIndex(k.key());
        
        synthManager.voice()->setInternalParameterValue("frequency", ::pow(2.f, (midiNote - 69.f) / 12.f) * 432.f);
        
        synthManager.triggerOn(midiNote);
      }
    }
    return true;
  }

  bool onKeyUp(Keyboard const& k) override {
    int midiNote = asciiToMIDI(k.key());
    if (midiNote > 0) {
      synthManager.triggerOff(midiNote);
    }
    return true;
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
