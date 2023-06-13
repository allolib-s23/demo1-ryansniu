#include <cstdio> // for printing to stdout
#include <fstream>
#include <iostream>

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

class NoteData {
public:
  enum Quality {
    NONE = -1,
    MISS = 0,
    OKAY = 1,
    GOOD = 2,
    PERF = 3
  };
  Quality hit1, hit2;
  float startTime, endTime;
  int lane;
  bool isHeld() {return endTime - startTime > 0; }
};

struct NoteDataCompare{
  bool operator()(const NoteData& a,const NoteData& b) const{
    return a.endTime > b.endTime ? true : a.startTime > b.startTime;
  }
};

struct NoteDataCompare2{
  bool operator()(const NoteData& a,const NoteData& b) const{
    return a.startTime < b.startTime;
  }
};

class BGM : public SynthVoice {
public:
  SoundFilePlayerTS player;
  std::vector<float> soundfile_buffer;
  gam::Timer timer;
  Mesh lMesh, nMesh, rMesh, cMesh;

  bool keyDown = true;
  float rotation, zOffset;
  int colorID;

  void init() override {
    createInternalTriggerParameter("soundType", 0.0, 0.0, 4.0);
    createInternalTriggerParameter("lane", 0.0, 1.0, 4.0);
    createInternalTriggerParameter("quality", 0.0, -1.0, 4.0);
    createInternalTriggerParameter("combo", 0.0, 0.0, 9999.0);

    addRect(lMesh);
    addAnnulus(nMesh, 0.75f);
    addDisc(rMesh);
    addCylinder(cMesh, 0.05f, 500.0f);
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
    if((int)getInternalParameterValue("soundType") != 0 && timer.elapsedSec() >= 1.5f) {
      player.setPause();
      player.setRewind();
      keyDown = false;
      free();
    }
  }

  void setNoteColor(Graphics &g, int quality, int lane) {
    if(quality == NoteData::Quality::NONE) g.color(1, 1, 1);
    else {
      switch(lane) {
        case 1:
          g.color(0, 0, 1);
          break;
        case 2:
          g.color(1, 0, 0);
          break;
        case 3:
          g.color(1, 1, 0);
          break;
        case 4:
          g.color(0, 1, 0);
          break;
      }
    }
  }

  void onProcess(Graphics &g) override {
    timer.stop();
    if((int)getInternalParameterValue("soundType") == 0 || (int)getInternalParameterValue("soundType") == 2) return;

    int lane = (int)getInternalParameterValue("lane");
    int combo = (int)getInternalParameterValue("combo");
    int quality = (int)getInternalParameterValue("quality");

    if(combo > 0 && timer.elapsedSec() <= 0.35f) {
      g.pushMatrix();
      g.translate((lane * 0.25) + 1, 0, -5);
      g.scale(0.012, 4, 1);
      g.color(1, 0.60 + 0.1 * quality, 0);
      // g.draw(lMesh);
      g.popMatrix();
    }

    if(keyDown) {
      g.pushMatrix();
      g.translate((lane * 0.25) + 1, -1, -5);
      float scale = quality > NoteData::Quality::MISS ? std::max((0.05f - timer.elapsedSec() * 0.1f) * quality, 0.0) : 0.1f;
      g.scale(0.5f * scale, 0.5f * scale, 1);
      setNoteColor(g, quality, lane);
      g.draw(rMesh);
      g.popMatrix();
    }

    if(quality > NoteData::Quality::MISS && timer.elapsedSec() < 0.1f) {
      g.pushMatrix();
      g.translate((lane * 0.25) + 1, -1, -5);
      g.scale(0.5f * quality * timer.elapsedSec() + 0.05, 0.5f * quality * timer.elapsedSec() + 0.05, 1);
      setNoteColor(g, quality, lane);
      g.draw(nMesh);
      g.popMatrix();
    }
    
    int xOffset[] = {-1, 0, 0, 1};
    int yOffset[] = {0, -1, 1, 0};

    rotation += combo * 0.015f;

    g.pushMatrix();
    g.translate(-1.25 + xOffset[lane - 1] * timer.elapsedSec(), yOffset[lane - 1] * timer.elapsedSec(), -8 + zOffset);
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
        soundFile += "glamour.wav";
        break;
      case 1:
        soundFile += "Katsu.wav";
        break;
      case 2:
        soundFile += "missnote2.wav";
        break;
      case 3:
        soundFile += "Don.wav";
        break;
    }
    if(!player.open(soundFile.c_str())){
      std::cerr << "File not found: " << soundFile << std::endl;
      exit(1);
    }
    player.setNoLoop();
    player.setPlay();
    rotation = rand() % 360;
    zOffset = rand() % 4 - 2;
    colorID = rand() % 3;
    keyDown = true;
    timer.start();
  }

  void onTriggerOff() override {
    player.setPause();
    player.setRewind();
    keyDown = false;
  }
};

class MyApp : public App {
public:
  SynthGUIManager<BGM> bgmManager{"BGM"};
  FontRenderer fontRender;
  float fontSize = 0.125f;
  Mesh lMesh, nMesh, dMesh, gMesh;
  float donutAngle = 0, donutAngleVelocity = 0;
  float donutPos = 0, donutPosVelocity = 0;

  float bpm = 148, beatLen = 60.0 / bpm, songLen = 137;
  float currTime = 0.0f, songOffset = 2.08f;
  int combo = 0, totalHits = 0;
  float ySpeed = 1.5f;
  NoteData::Quality lastAccuracy = NoteData::Quality::NONE;
  std::queue<NoteData> noteQueue;
  std::vector<NoteData> onScreen;

  void onCreate() override {
    navControl().active(false);

    gam::sampleRate(audioIO().framesPerSecond());

    addRect(lMesh, 1, 1);
    addDisc(nMesh);
    addTorus(dMesh);
    addWireBox(gMesh, 1);

    std::string fontFile = "C:\\Users\\ryann\\Desktop\\College Stuff\\Classes\\23 Q2 Spring\\CCS 130H\\allolib\\demo1-ryansniu\\tutorials\\synthesis\\audio\\RoundPixels.ttf";
    fontRender.load(fontFile.c_str(), 60, 1024);
    fontRender.alignCenter();

    readBeatmap();

    bgmManager.voice()->setInternalParameterValue("soundType", 0);
    bgmManager.triggerOn(0);
    currTime = 0;
  }

  void readBeatmap() {
    std::string line;
    std::string tokens[3];
    NoteData heldNote;
    std::vector<NoteData> tempQueue;

    std::ifstream beatmapFile("C:\\Users\\ryann\\Desktop\\College Stuff\\Classes\\23 Q2 Spring\\CCS 130H\\allolib\\demo1-ryansniu\\tutorials\\synthesis\\audio\\beatmap.txt");
    while(std::getline(beatmapFile, line)) {
      std::stringstream split(line);
      for(int i = 0; i < 3; i++) std::getline(split, tokens[i], ' ');
      if(tokens[1] == "@") {
        NoteData note;
        note.hit1 = note.NONE;
        note.hit2 = note.NONE;
        note.lane = std::atoi(tokens[0].c_str());
        note.startTime = std::atof(tokens[2].c_str()) + songOffset;
        note.endTime = note.startTime;
        tempQueue.push_back(note);
      }
      else if(tokens[1] == "+") {
        NoteData note;
        note.hit1 = note.NONE;
        note.hit2 = note.NONE;
        note.lane = std::atoi(tokens[0].c_str());
        note.startTime = std::atof(tokens[2].c_str()) + songOffset;

        std::getline(beatmapFile, line);
        std::stringstream split(line);
        for(int i = 0; i < 3; i++) std::getline(split, tokens[i], ' ');

        note.endTime = std::atof(tokens[2].c_str()) + songOffset;
        tempQueue.push_back(note);
      }
    }
    beatmapFile.close();
    std::sort(tempQueue.begin(), tempQueue.end(), NoteDataCompare2());
    for(NoteData nd : tempQueue) {
      noteQueue.push(nd);
    }
  }

  // The audio callback function. Called when audio hardware requires data
  void onSound(AudioIOData &io) override {
    bgmManager.render(io); // Render audio
  }

  void onAnimate(double dt) override {
    donutPosVelocity -= dt * donutPosVelocity / (1 + std::log10f(combo));
    if(donutPosVelocity < 0.0f) donutPosVelocity = 0.0f;

    currTime = audioIO().time();

    while(!noteQueue.empty() && noteQueue.front().startTime <= currTime + 4) {
      NoteData note = noteQueue.front();
      noteQueue.pop();
      onScreen.push_back(note);
      std::push_heap(onScreen.begin(), onScreen.end(), NoteDataCompare());
    }

    std::pop_heap(onScreen.begin(), onScreen.end(), NoteDataCompare());
    while(!onScreen.empty() && onScreen.back().endTime <= currTime - 0.5f) {
      if(onScreen.back().hit1 == NoteData::Quality::NONE || (onScreen.back().isHeld() && onScreen.back().hit2 == NoteData::Quality::NONE)) {
        if(onScreen.back().hit1 == NoteData::Quality::NONE) onScreen.back().hit1 = NoteData::Quality::MISS;
        onScreen.back().hit2 = NoteData::Quality::MISS;
        lastAccuracy = NoteData::Quality::MISS;
        combo = 0;
        bgmManager.voice()->setInternalParameterValue("soundType", 2);
        bgmManager.triggerOn(-onScreen.back().lane);
      }
      onScreen.pop_back();
      std::pop_heap(onScreen.begin(), onScreen.end(), NoteDataCompare());
    }
  }

  // The graphics callback function.
  void onDraw(Graphics &g) override {
    g.clear();

    drawBGBox(g);
    drawFrame(g);
    drawFunnyDonut(g);
    drawAccuracy(g);
    drawNotes(g);

    bgmManager.render(g);
  }

  void drawBGBox(Graphics &g) {
    for(int r = -5; r <= 5; r++) {
      for(int c = -10; c <= 10; c++) {
        g.pushMatrix();
        g.translate(c, r, 2.0f * std::sinf(r + c + currTime * 2.0f));
        g.scale(1, 1, 30);
        g.color(0, 0.5f, 0);
        g.draw(gMesh);
        g.popMatrix();
      }
    }
  }

  void drawFrame(Graphics &g) {
    for(int i = 1; i <= 4; i++) {
      g.pushMatrix();
      g.translate((i * 0.25) + 1, 0, -5);
      g.scale(0.01, 4, 1);
      g.color(1, 1, 1);
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

    float x = std::cosf(donutPos) * std::cosf(donutPos);
    float y = std::cosf(donutPos) * std::sinf(donutPos);
    float z = std::cosf(donutPos);

    // fontRender.renderAt(g, {x-1.25, y, z-5});
    fontRender.renderAt(g, {1.625, 0, -5});
    g.popMatrix();
  }

  void setNoteColor(Graphics &g, NoteData note) {
    if(note.hit1 == NoteData::Quality::MISS || note.hit2 == NoteData::Quality::MISS) g.color(0.5, 0.5, 0.5);
    else {
      switch(note.lane) {
        case 1:
          g.color(0, 0, 1);
          break;
        case 2:
          g.color(1, 0, 0);
          break;
        case 3:
          g.color(1, 1, 0);
          break;
        case 4:
          g.color(0, 1, 0);
          break;
      }
      // g.color(1, 1, 1);
    }
  }

  void drawNote(Graphics &g, NoteData note, bool isStart) {
    float offsetScale = std::sqrtf(2) * 0.1f / 4.0f;
    float noteTime = isStart ? note.startTime : note.endTime;
    float yPos = note.isHeld() && isStart && note.hit1 > NoteData::Quality::MISS ? 0 : noteTime - currTime;
    yPos *= ySpeed;

    g.pushMatrix();
    g.translate((note.lane * 0.25) + 1, yPos - 1, -5);
    g.scale(0.1, 0.1, 1);
    setNoteColor(g, note);
    g.rotate(45);
    g.draw(lMesh);
    g.popMatrix();

    g.pushMatrix();
    setNoteColor(g, note);
    if(note.lane - 1 < 2) g.translate((note.lane * 0.25) + 1 + offsetScale, yPos - 1 + offsetScale, -5);  // UR
    else g.translate((note.lane * 0.25) + 1 - offsetScale, yPos - 1 - offsetScale, -5);  // DL
    g.scale(0.05, 0.05, 1);
    g.draw(nMesh);
    g.popMatrix();

    g.pushMatrix();
    setNoteColor(g, note);
    if((note.lane - 1) % 2 == 0) g.translate((note.lane * 0.25) + 1 + offsetScale, yPos - 1 - offsetScale, -5);  // DR
    else g.translate((note.lane * 0.25) + 1 - offsetScale, yPos - 1 + offsetScale, -5);  // UL
    g.scale(0.05, 0.05, 1);
    g.draw(nMesh);
    g.popMatrix();
  }

  void drawNotes(Graphics &g) {
    for(auto &note : onScreen) {
      if(note.hit2 > NoteData::Quality::MISS || (!note.isHeld() && note.hit1 > NoteData::Quality::MISS)) continue;
      if(note.isHeld()) {
        g.pushMatrix();
        float yOffset = note.hit1 > NoteData::Quality::MISS ? std::max(note.startTime, currTime) : note.startTime;
        g.translate((note.lane * 0.25) + 1, ySpeed * ((note.endTime + yOffset) / 2.0f - currTime) - 1, -5);
        g.scale(0.02, (note.endTime - yOffset) * ySpeed, 1);
        setNoteColor(g, note);
        g.draw(lMesh);
        g.popMatrix();
        drawNote(g, note, false);
      }
      drawNote(g, note, true);
    }
  }

  void drawFunnyDonut(Graphics &g) {
    donutPos += donutPosVelocity;
    if(donutPos > 6.28) donutPos -= 6.28;

    donutAngle += totalHits * 0.025f;
    if(donutAngle > 360) donutAngle -= 360;

    float x = std::cosf(donutPos) * std::cosf(donutPos);
    float y = std::cosf(donutPos) * std::sinf(donutPos);
    float z = std::cosf(donutPos);

    g.pushMatrix();
    g.translate(-1.25 + x, y, -5 + z);
    g.scale(0.4, 0.4, 0.4);
    g.rotate(donutAngle, 1, 1, 1);
    g.color(Color(HSV(audioIO().time() - (int)audioIO().time(), 1, 1)));
    g.draw(dMesh);
    g.popMatrix();
  }
  void onExit() override { imguiShutdown(); }

  NoteData::Quality getCurrentAccuracy(NoteData note, bool start) {
    float diff = std::abs((start ? note.startTime : note.endTime) - currTime);
    if(diff < 0.025f) return NoteData::Quality::PERF;
    if(diff < 0.05f) return NoteData::Quality::GOOD;
    if(diff < 0.1f) return NoteData::Quality::OKAY;
    if(diff < 0.2f || !start) return NoteData::Quality::MISS;
    return NoteData::Quality::NONE;
  }

  bool onKeyDown(Keyboard const& k) override {
    int id = keyToID(k);
    if(id != -1) {
      bool noteExistsInLane = false;
      NoteData::Quality bestAccuracy = NoteData::Quality::NONE;
      NoteData* closestNoteInLane;
      for(auto &note : onScreen) {
        if(note.lane == id && note.hit1 == NoteData::Quality::NONE) {
          if(!noteExistsInLane) {
            noteExistsInLane = true;
            closestNoteInLane = &note;
            bestAccuracy = getCurrentAccuracy(*closestNoteInLane, true);
          }
          else if(note.startTime < closestNoteInLane->startTime && getCurrentAccuracy(note, true) >= getCurrentAccuracy(*closestNoteInLane, true)) {
            closestNoteInLane = &note;
            bestAccuracy = getCurrentAccuracy(*closestNoteInLane, true);
          }
        }
      }
      if(noteExistsInLane) {
        closestNoteInLane->hit1 = bestAccuracy;
        lastAccuracy = bestAccuracy;
        if(bestAccuracy == NoteData::Quality::MISS) {
          combo = 0;
          bgmManager.voice()->setInternalParameterValue("soundType", 2);
          bgmManager.triggerOn(-id);
          return true;
        }
        else if(bestAccuracy > NoteData::Quality::MISS) {
          combo++;
          totalHits++;
          bgmManager.voice()->setInternalParameterValue("combo", combo);
          bgmManager.voice()->setInternalParameterValue("quality", NoteData::Quality(bestAccuracy));
          donutPosVelocity += 0.0025f * bestAccuracy * std::log10f(combo);
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
      NoteData::Quality heldAccuracy = NoteData::Quality::NONE;
      for(auto &note : onScreen) {
        if(note.lane == id && note.hit1 > NoteData::Quality::MISS && note.isHeld() && note.hit2 == NoteData::Quality::NONE) {
          NoteData::Quality heldAccuracy = getCurrentAccuracy(note, false);
          lastAccuracy = heldAccuracy;
          note.hit2 = heldAccuracy;
          if(heldAccuracy == NoteData::Quality::MISS) {
            combo = 0;
            bgmManager.voice()->setInternalParameterValue("soundType", 2);
            bgmManager.triggerOn(-id);
            return true;
          }
          else if(heldAccuracy > NoteData::Quality::MISS) {
            combo++;
            totalHits++;
            donutPosVelocity += 0.0025f * heldAccuracy * std::log10f(combo);
            bgmManager.voice()->setInternalParameterValue("combo", combo);
            bgmManager.voice()->setInternalParameterValue("lane", id);
            bgmManager.voice()->setInternalParameterValue("quality", NoteData::Quality(heldAccuracy));
            bgmManager.voice()->setInternalParameterValue("soundType", 3);
            bgmManager.triggerOn(4 + id);
            return true;
          }
        }
      }
      bgmManager.triggerOff(id);
    }
    return true;
  }
};


int main() {
  // Create app instance
  MyApp app;

  // Set up audio
  app.configureAudio(44100., 512, 2, 0);

  app.start();
  return 0;
}

