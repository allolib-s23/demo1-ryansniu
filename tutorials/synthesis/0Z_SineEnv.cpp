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
#include "al/sound/al_SoundFile.hpp"
#include "al/graphics/al_Font.hpp"

// using namespace gam;
using namespace al;

int keyToID(Keyboard const& k) {
  switch(k.key()) {
    case Keyboard::Key::LEFT:
      return 1;
    case Keyboard::Key::DOWN:
      return 2;
    case Keyboard::Key::UP:
      return 3;
    case Keyboard::Key::RIGHT:
      return 4;
  }
  return -1;
}

class BGM : public SynthVoice {
public:
  SoundFilePlayerTS player;
  std::vector<float> soundfile_buffer;
  gam::Timer timer;
  Mesh lMesh, nMesh, cMesh;

  bool drawNoteCircle = true;
  float rotation, zOffset;
  int colorID;

  void init() override {
    createInternalTriggerParameter("soundType", 0.0, 0.0, 1.0);
    createInternalTriggerParameter("lane", 0.0, 1.0, 4.0);
    createInternalTriggerParameter("quality", 0.0, -1.0, 4.0);
    createInternalTriggerParameter("combo", 0.0, 0.0, 9999.0);

    addRect(lMesh);
    addDisc(nMesh);
    addCylinder(cMesh, 0.05f, 80.0f);
  }

  // The audio processing function
  void onProcess(AudioIOData& io) override {
    int frames = (int)io.framesPerBuffer();
    int channels = player.soundFile.channels;
    int bufferLength = frames * channels;
    if ((int)soundfile_buffer.size() < bufferLength) {
      soundfile_buffer.resize(bufferLength);
    }
    player.getFrames(frames, soundfile_buffer.data(), (int)soundfile_buffer.size());
    int second = (channels < 2) ? 0 : 1;
    while (io()) {
      int frame = (int)io.frame();
      int idx = frame * channels;
      io.out(0) = soundfile_buffer[idx];
      io.out(1) = soundfile_buffer[idx + second];
    }
    
    timer.stop();
    if((int)getInternalParameterValue("soundType") != 0 && timer.elapsedSec() >= 1.5f) free();
  }

  void onProcess(Graphics &g) override {
    timer.stop();
    if((int)getInternalParameterValue("soundType") == 0) return;

    int lane = (int)getInternalParameterValue("lane");
    int combo = (int)getInternalParameterValue("combo");
    int quality = (int)getInternalParameterValue("quality");

    if(combo > 0 && timer.elapsedSec() <= 0.35f) {
      g.pushMatrix();
      g.translate((lane * 0.25) + 1, 0, -5);
      g.scale(0.012, 4, 1);
      g.color(1, 0.60 + 0.1 * quality, 0);
      g.draw(lMesh);
      g.popMatrix();
    }

    if(drawNoteCircle) {
      g.pushMatrix();
      g.translate((lane * 0.25) + 1, -1, -5);
      g.scale(0.06, 0.06, 1);
      g.color(1, 0.25, 0.25);
      g.draw(nMesh);
      g.popMatrix();
    }
    
    int xOffset[] = {-1, 0, 0, 1};
    int yOffset[] = {0, -1, 1, 0};

    rotation += combo * 0.08f;

    g.pushMatrix();
    g.translate(-1.5 + xOffset[lane - 1] * timer.elapsedSec(), yOffset[lane - 1] * timer.elapsedSec(), -8 + zOffset);
    g.rotate(rotation, -1, -1, 1);
    g.scale(0.5, 0.5, 1);

    switch(colorID) {
      case 0:
        g.color(1, 0, 1, 0.15 * quality);
        break;
      case 1:
        g.color(0, 1, 1, 0.15 * quality);
        break;
      case 2:
        g.color(1, 1, 0, 0.15 * quality);
        break;
    }
    
    g.draw(cMesh);
    g.popMatrix();
  }

  void onTriggerOn() override {
    int soundType = (int)getInternalParameterValue("soundType");
    std::string soundFile = "C:\\Users\\ryann\\Desktop\\College Stuff\\Classes\\23 Q2 Spring\\CCS 130H\\allolib\\demo1-ryansniu\\tutorials\\synthesis\\audio\\";
    switch(soundType) {
      case 0:
        soundFile += "aibomb.wav";
        break;
      case 1:
        soundFile += "Katsu.wav";
        break;
    }
    if(!player.open(soundFile.c_str())){
      std::cerr << "File not found: " << soundFile << std::endl;
      exit(1);
    }
    player.setNoLoop();
    player.setPlay();
    drawNoteCircle = true;
    rotation = rand() % 360;
    zOffset = rand() % 4 - 2;
    colorID = rand() % 3;
    timer.start();
  }

  void onTriggerOff() override {
    player.setPause();
    player.setRewind();
    drawNoteCircle = false;
  }
};

class NoteData {
public:
  enum Quality {
    NONE = -1,
    MISS = 0,
    OKAY = 1,
    GOOD = 2,
    PERF = 3
  };
  Quality hit;
  float startTime, endTime;
  int lane;
  bool isHeld() {return endTime - startTime > 0; }
};

struct NoteDataCompare{
  bool operator()(const NoteData& a,const NoteData& b) const{
    return a.endTime > b.endTime ? true : a.startTime > b.startTime;
  }
};

class MyApp : public App {
public:
  SynthGUIManager<BGM> bgmManager{"BGM"};
  FontRenderer fontRender;
  float fontSize = 0.125f;
  Mesh lMesh, nMesh, dMesh;
  float donutAngle = 0, donutAngleVelocity = 1;
  float donutPos = 0;

  float bpm = 148, beatLen = 60.0 / bpm, songLen = 135;
  float currTime = 0.0f, songOffset = 0.92f;
  int combo = 0, totalHits = 0;
  NoteData::Quality lastAccuracy = NoteData::Quality::NONE;
  std::queue<NoteData> noteQueue;
  std::vector<NoteData> onScreen;

  void onCreate() override {
    navControl().active(false);

    gam::sampleRate(audioIO().framesPerSecond());

    addRect(lMesh, 1, 1);
    addDisc(nMesh);
    addTorus(dMesh);

    std::string fontFile = "C:\\Users\\ryann\\Desktop\\College Stuff\\Classes\\23 Q2 Spring\\CCS 130H\\allolib\\demo1-ryansniu\\tutorials\\synthesis\\audio\\RoundPixels.ttf";
    fontRender.load(fontFile.c_str(), 60, 1024);
    fontRender.alignCenter();

    randBeatmap();

    bgmManager.voice()->setInternalParameterValue("soundType", 0);
    bgmManager.triggerOn(0);
    currTime = 0;
  }

  void randBeatmap() {
    for(int beat = 0; beat * beatLen < songLen; beat++) {
      NoteData note;
      note.hit = note.NONE;
      note.startTime = beat * beatLen + songOffset;
      note.endTime = note.startTime;
      note.lane = rand() % 4 + 1;
      noteQueue.push(note);
    }
  }

  // The audio callback function. Called when audio hardware requires data
  void onSound(AudioIOData &io) override {
    bgmManager.render(io); // Render audio
  }

  void onAnimate(double dt) override {
    donutAngleVelocity -= dt * (9999 - combo) / 6666.0f;
    if(donutAngleVelocity < 4.0f) donutAngleVelocity = 4.0f;

    currTime = audioIO().time();

    while(!noteQueue.empty() && noteQueue.front().startTime <= currTime + 4) {
      NoteData note = noteQueue.front();
      noteQueue.pop();
      onScreen.push_back(note);
      std::push_heap(onScreen.begin(), onScreen.end(), NoteDataCompare());
    }
    std::pop_heap(onScreen.begin(), onScreen.end(), NoteDataCompare());
    while(!onScreen.empty() && onScreen.back().endTime <= currTime - 0.5) {
      if(onScreen.back().hit == NoteData::Quality::NONE) {
        onScreen.back().hit = NoteData::Quality::MISS;
        lastAccuracy = NoteData::Quality::MISS;
        combo = 0;
      }
      onScreen.pop_back();
      std::pop_heap(onScreen.begin(), onScreen.end(), NoteDataCompare());
    }
  }

  // The graphics callback function.
  void onDraw(Graphics &g) override {
    g.clear();

    drawAccuracy(g);
    drawFrame(g);
    drawNotes(g);
    drawFunnyDonut(g);

    bgmManager.render(g);
  }

  void drawFrame(Graphics &g) {
    for(int i = 1; i <= 4; i++) {
      g.pushMatrix();
      g.translate((i * 0.25) + 1, 0, -5);
      g.scale(0.01, 4, 1);
      g.color(1, 0.65, 0.45);
      g.draw(lMesh);
      g.popMatrix();
    }

    g.pushMatrix();
    g.translate(1.625, -1, -5);
    g.scale(1, 0.02, 1);
    g.color(1, 0, 0);
    g.draw(lMesh);
    g.popMatrix();
  }

  void drawAccuracy(Graphics &g) {
    g.pushMatrix();
    std::string quality = "";
    switch(lastAccuracy) {
      case NoteData::Quality::MISS:
        quality = "MISS";
        break;
      case NoteData::Quality::OKAY:
        quality = "OKAY ";
        break;
      case NoteData::Quality::GOOD:
        quality = "GOOD ";
        break;
      case NoteData::Quality::PERF:
        quality = "PERFECT ";
        break;
    }
    if(lastAccuracy != NoteData::Quality::NONE && combo > 0) quality.append(std::to_string(combo));
    fontRender.write(quality.c_str(), fontSize);
    fontRender.renderAt(g, {1.625, 0, -5});
    g.popMatrix();
  }

  void drawNotes(Graphics &g) {
    float offsetScale = 0.03f;
    int xOffset[] = {-1, 0, 0, 1};
    int yOffset[] = {0, -1, 1, 0};

    for(auto &note : onScreen) {
      if(note.hit > NoteData::Quality::MISS) continue;
      g.pushMatrix();
      g.translate((note.lane * 0.25) + 1, note.startTime - currTime - 1, -5);
      g.scale(0.06, 0.06, 1);
      if(note.hit == NoteData::Quality::MISS) g.color(0.5, 0.5, 0.5);
      else g.color(1, 1, 1);
      g.draw(nMesh);
      g.popMatrix();

      g.pushMatrix();
      g.translate((note.lane * 0.25) + 1 + xOffset[note.lane - 1] * offsetScale, note.startTime - currTime - 1 +  + yOffset[note.lane - 1] * offsetScale, -5);

      if(note.lane == 2 || note.lane == 3) g.scale(0.06, 0.12, 1);
      else g.scale(0.12, 0.06, 1);

      if(note.hit == NoteData::Quality::MISS) g.color(0.5, 0.5, 0.5);
      else g.color(1, 1, 1);
      g.draw(lMesh);
      g.popMatrix();
    }
  }

  void drawFunnyDonut(Graphics &g) {
    donutAngle += donutAngleVelocity;
    if(donutAngle > 360) donutAngle -= 360;

    donutPos += totalHits * 0.0005f;
    if(donutPos > 6.28) donutPos -= 6.28;

    float x = std::cosf(donutPos) * std::cosf(donutPos);
    float y = std::cosf(donutPos) * std::sinf(donutPos);
    float z = std::cosf(donutPos);

    g.pushMatrix();
    g.translate(-1.5 + x, y, -5 + z);
    g.scale(0.4, 0.4, 0.4);
    g.rotate(donutAngle, 1, 1, 1);
    g.color(Color(HSV(audioIO().time() - (int)audioIO().time(), 1, 1)));
    g.draw(dMesh);
    g.popMatrix();
  }
  void onExit() override { imguiShutdown(); }

  NoteData::Quality getCurrentAccuracy(NoteData note) {
    float diff = std::abs(note.startTime - currTime);
    if(diff < 0.025f) return NoteData::Quality::PERF;
    if(diff < 0.05f) return NoteData::Quality::GOOD;
    if(diff < 0.1f) return NoteData::Quality::OKAY;
    if(diff < 0.2f) return NoteData::Quality::MISS;
    return NoteData::Quality::NONE;
  }

  bool onKeyDown(Keyboard const& k) override {

    int id = keyToID(k);
    if(id != -1) {
      bool noteExistsInLane = false;
      NoteData::Quality bestAccuracy = NoteData::Quality::NONE;
      NoteData* closestNoteInLane;
      for(auto &note : onScreen) {
        if(note.lane == id && note.hit == NoteData::Quality::NONE) {
          if(!noteExistsInLane) {
            noteExistsInLane = true;
            closestNoteInLane = &note;
            bestAccuracy = getCurrentAccuracy(*closestNoteInLane);
          }
          else if(note.startTime < closestNoteInLane->startTime && getCurrentAccuracy(note) >= getCurrentAccuracy(*closestNoteInLane)) {
            closestNoteInLane = &note;
            bestAccuracy = getCurrentAccuracy(*closestNoteInLane);
          }
        }
      }
      if(noteExistsInLane) {
        closestNoteInLane->hit = bestAccuracy;
        lastAccuracy = bestAccuracy;
        if(bestAccuracy == NoteData::Quality::MISS) {
          // TO-DO: miss noise
          combo = 0;
          return true;
        }
        else if(bestAccuracy > NoteData::Quality::MISS) {
          combo++;
          totalHits++;
          bgmManager.voice()->setInternalParameterValue("combo", combo);
          bgmManager.voice()->setInternalParameterValue("quality", NoteData::Quality(bestAccuracy));
          donutAngleVelocity += 0.25f * bestAccuracy;
        }
        else {
          bgmManager.voice()->setInternalParameterValue("combo", 0);
          bgmManager.voice()->setInternalParameterValue("quality", -1);
        }
        bgmManager.voice()->setInternalParameterValue("lane", id);
      }
      bgmManager.voice()->setInternalParameterValue("soundType", 1);
      bgmManager.triggerOn(id);
    }
    return true;
  }

  bool onKeyUp(Keyboard const& k) override {
    int id = keyToID(k);
    if(id != -1) {
      bgmManager.triggerOff(id);
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

