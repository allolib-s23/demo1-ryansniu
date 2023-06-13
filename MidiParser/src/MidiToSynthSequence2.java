import javax.sound.midi.*;
import java.io.*;

public class MidiToSynthSequence2 {
  public String songName = "src/Purgatory/yokai";
  public double midiNoteToFreq(int note) { return 440.0 * Math.pow(2.0, (note - 69) / 12.0); }
  public String bytesToHex(byte[] bytes) {
    final char[] hexArray = "0123456789ABCDEF".toCharArray();
    char[] hexChars = new char[bytes.length * 2];
    for (int j = 0; j < bytes.length; j++) {
      int v = bytes[j] & 0xFF;
      hexChars[j * 2] = hexArray[v >>> 4];
      hexChars[j * 2 + 1] = hexArray[v & 0x0F];
    }
    return new String(hexChars);
  }

  public static void main(String[] args) throws IOException {
    MidiToSynthSequence2 mtss = new MidiToSynthSequence2();
    mtss.run();
  }

  public void run() {
    PrintWriter out = null;
    try {
      out = new PrintWriter(new BufferedWriter(new FileWriter("data.txt")));
    } catch(IOException e) {
      System.err.println("Failed to create output file!");
      System.exit(1);
    }

    Sequence sequence = openSequence();
    double resolution_const = 60.0 / sequence.getResolution();
    double tempo = 120.0, currentTime = 0.0;
    long prevTick = 0;

    int trackNum = 0;
    for (Track track : sequence.getTracks()) {
      trackNum++;
      for (int i = 0; i < track.size(); i++) {
        MidiEvent event = track.get(i);
        MidiMessage message = event.getMessage();
        
        currentTime += resolution_const / tempo * (event.getTick() - prevTick);
        prevTick = event.getTick();

        if (message instanceof ShortMessage) {
          ShortMessage sm = (ShortMessage) message;
          int note = sm.getData1();
          int velocity = sm.getData2();
          int cmd = sm.getCommand();

          if (cmd == ShortMessage.NOTE_OFF || (cmd == ShortMessage.NOTE_ON && velocity == 0)) {
            String eventID = Integer.toString(trackNum) + Integer.toString(note);
            out.println("- " + (currentTime + 4) + " " + eventID);

            String eventID2 = "99" + Integer.toString(trackNum) + Integer.toString(note);
            out.println("- " + (currentTime) + " " + eventID2);
          }
          else if (cmd == ShortMessage.NOTE_ON) {
            String eventID = Integer.toString(trackNum) + Integer.toString(note);
            double amplitude = velocity / (128.0 * sequence.getTracks().length * 3);
            String sineEnvParams = "SineEnv " + amplitude + " " + midiNoteToFreq(note) + " 0.0 0.0 0.0";
            out.println("+ " + (currentTime + 4) + " " + eventID + " " + sineEnvParams);
            
            String eventID2 = "99" + Integer.toString(trackNum) + Integer.toString(note);
            String sineEnvParams2 = "SineEnv 0.0 " + midiNoteToFreq(note) + " 0.0 0.0 0.0";
            out.println("+ " + currentTime + " " + eventID2 + " " + sineEnvParams2);
          }
        }
        else if (message instanceof MetaMessage) {
          MetaMessage mm = (MetaMessage) message;
          String msg = bytesToHex(mm.getMessage());

          if (msg.substring(0, 6).equals("FF5103"))
            tempo = Math.round(60000000.0 / Long.decode("0x" + bytesToHex(mm.getData())));
        }
      }
      out.println();
    }
    out.close();
  }

  public Sequence openSequence() {
    try {
      return MidiSystem.getSequence(new File(songName + ".mid"));
    } catch (InvalidMidiDataException e) {
      System.err.println("MIDI file is corrupt!");
      System.exit(2);
    } catch (IOException e) {
      System.err.println("File not found!");
      System.exit(3);
    }
    return null;
  }
}