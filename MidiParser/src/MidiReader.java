import javax.sound.midi.*;
import java.io.*;

public class MidiReader {
  public final String[] NOTE_NAMES = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
  };
  public String songName = "src/Purgatory/beatmap";

  public static void main(String[] args) throws IOException {
    MidiReader mr = new MidiReader();
    mr.run();
  }

  public void run() {
    PrintWriter out = null;
    try {
      out = new PrintWriter(new BufferedWriter(new FileWriter("data2.txt")));
    } catch(IOException e) {
      System.err.println("Failed to create output file!");
      System.exit(3);
    }
    Sequence sequence = openSequence();

    int trackNumber = 0;
    for (Track track : sequence.getTracks()) {
      trackNumber++;
      out.println("\nTrack " + trackNumber + ": size = " + track.size() + "\n");
      for (int i = 0; i < track.size(); i++) {
        MidiEvent event = track.get(i);
        MidiMessage message = event.getMessage();
        out.print("@" + event.getTick() + "\t");

        if (message instanceof ShortMessage) {
          ShortMessage sm = (ShortMessage) message;
          int data1 = sm.getData1();
          int data2 = sm.getData2();
          int cmd = sm.getCommand();
          String command = "OTHER\t";

          switch (cmd) {
            case ShortMessage.NOTE_ON:
              command = "NOTE ON\t";
              break;
            case ShortMessage.NOTE_OFF:
              command = "NOTE OFF\t";
              break;
            case ShortMessage.PITCH_BEND:
              command = "PITCH BEND\t";
              break;
            case ShortMessage.CONTROL_CHANGE:
              command = "CONTROL CHANGE\t";
              break;
            case ShortMessage.PROGRAM_CHANGE:
              command = "PROGRAM CHANGE\t";
              break;
            case ShortMessage.POLY_PRESSURE:
              command = "POLY PRESSURE\t";
              break;
            case ShortMessage.CHANNEL_PRESSURE:
              command = "CHANNEL PRESSURE\t";
              break;
          }

          //to check if the track number doesnt match with the channel number oops
          if (trackNumber - 1 != sm.getChannel()) {
            out.print((trackNumber - 1) + "-NO MATCH-" + sm.getChannel() + " ");
          }
          if (cmd == ShortMessage.NOTE_ON || cmd == ShortMessage.NOTE_OFF) {
            int octave = (data1 / 12) - 1;
            String noteName = NOTE_NAMES[data1 % 12];
            out.print(command + noteName + octave);
            out.println(cmd == ShortMessage.NOTE_ON ? " velocity: " + data2 : " " + data2);
          }
          else if (cmd == ShortMessage.PROGRAM_CHANGE) {
            out.println("INSTRUMENT: " + data1);
          }
          else if (cmd == ShortMessage.CONTROL_CHANGE) {
            switch (data1) {
              case 1:
                out.println("MODULATION: " + data2);
                break;
              case 6:
                out.println("DATA ENTRY MSB: " + data2);
                break;
              case 7:
                out.println("CHANNEL VOLUME: " + data2);
                break;
              case 10:
                out.println("PAN: " + data2);
                break;
              case 11:
                out.println("EXPRESSION: " + data2);
                break;
              case 38:
                out.println("LSB: " + data2);
                break;
              case 100:
                out.println("REGISTERED LSB: " + data2);
                break;
              case 101:
                out.println("REGISTERED MSB: " + data2);
                break;
              default:
                out.println(command + data1 + " " + data2);
                break;
            }
          }
          else {
            out.println(command + data1 + " " + data2);
          }
        }
        else if (message instanceof SysexMessage) {
          SysexMessage sm = (SysexMessage) message;
          out.print("SYSEX MSG:\t");
          String msg = bytesToHex(sm.getMessage());
          msg = msg.substring(0, 6) + " " + msg.substring(6);
          out.println(msg);
        }
        else if (message instanceof MetaMessage) {
          MetaMessage mm = (MetaMessage) message;
          String msg = bytesToHex(mm.getMessage());

          //END OF TRACK
          if (msg.substring(0, 6).equals("FF2F00")) {
            out.print("END OF TRACK:\tFF2F00");
          }
          //TEMPO
          else if (msg.substring(0, 6).equals("FF5103")) {
            out.print("TEMPO:\t");
            double tempo = 60000000.0 / Long.decode("0x" + bytesToHex(mm.getData()));
            out.println(tempo);
          }
          //TRACK NAME
          else if (msg.substring(0, 4).equals("FF03")) {
            String name = new String(mm.getData());
            out.println(name);
          }
          else {
            String data = bytesToHex(mm.getData());
            String output = msg.substring(0, msg.length() - data.length()) + " " + data;
            out.print("META MSG:\t" + output);
          }
        }
      }
      out.println();
    }

    try {
      runSynth(sequence);
    } catch (Exception e) {
      System.err.println("Synth failed.");
      System.exit(2);
    }
  }

  public Sequence openSequence() {
    try {
      return MidiSystem.getSequence(new File(songName + ".mid"));
    } catch (InvalidMidiDataException e) {
      System.err.println("MIDI file is corrupt!");
      System.exit(1);
    } catch (IOException e) {
      System.err.println("File not found!");
      System.exit(1);
    }
    return null;
  }

  public void runSynth(Sequence sequence) throws Exception {
    Synthesizer synthesizer = MidiSystem.getSynthesizer();
    synthesizer.open();
    synthesizer.unloadAllInstruments(synthesizer.getDefaultSoundbank());
    synthesizer.loadAllInstruments(MidiSystem.getSoundbank(new File(songName + ".sf2")));

    Sequencer sequencer = MidiSystem.getSequencer(false);
    sequencer.open();
    sequencer.getTransmitter().setReceiver(synthesizer.getReceiver());
    sequencer.setSequence(sequence);
    sequencer.start();
  }

  private final char[] hexArray = "0123456789ABCDEF".toCharArray();

  public String bytesToHex(byte[] bytes) {
    char[] hexChars = new char[bytes.length * 2];
    for (int j = 0; j < bytes.length; j++) {
      int v = bytes[j] & 0xFF;
      hexChars[j * 2] = hexArray[v >>> 4];
      hexChars[j * 2 + 1] = hexArray[v & 0x0F];
    }
    return new String(hexChars);
  }
}
