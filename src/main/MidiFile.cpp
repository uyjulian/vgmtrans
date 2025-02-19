#include "pch.h"
#include "MidiFile.h"
#include "VGMSeq.h"
#include "Root.h"

using namespace std;


MidiFile::MidiFile(VGMSeq *theAssocSeq)
    : assocSeq(theAssocSeq),
      globalTrack(this, false),
      globalTranspose(0),
      bMonophonicTracks(false) {
  this->bMonophonicTracks = assocSeq->bMonophonicTracks;
  this->globalTrack.bMonophonic = this->bMonophonicTracks;

  this->ppqn = assocSeq->ppqn;
}


MidiFile::~MidiFile(void) {
  DeleteVect<MidiTrack>(aTracks);
}

MidiTrack *MidiFile::AddTrack(void) {
  aTracks.push_back(new MidiTrack(this, bMonophonicTracks));
  return aTracks.back();
}

MidiTrack *MidiFile::InsertTrack(uint32_t trackNum) {
  if (trackNum + 1 > aTracks.size())
    aTracks.resize(trackNum + 1, NULL);
  aTracks[trackNum] = new MidiTrack(this, bMonophonicTracks);
  return aTracks[trackNum];
}

int MidiFile::GetMidiTrackIndex(MidiTrack *midiTrack) {
  std::vector<MidiTrack *>::iterator it = std::find(aTracks.begin(), aTracks.end(), midiTrack);
  if (it != aTracks.end()) {
    return (int) std::distance(aTracks.begin(), it);
  }
  else {
    return -1;
  }
}

void MidiFile::SetPPQN(uint16_t ppqn) {
  this->ppqn = ppqn;
}

uint32_t MidiFile::GetPPQN() {
  return ppqn;
}

void MidiFile::Sort(void) {
  for (uint32_t i = 0; i < aTracks.size(); i++) {
    if (aTracks[i]) {
      if (aTracks[i]->aEvents.size() == 0) {
        delete aTracks[i];
        aTracks.erase(aTracks.begin() + i--);
      }
      else
        aTracks[i]->Sort();
    }
  }
}


bool MidiFile::SaveMidiFile(const std::wstring &filepath) {
  vector<uint8_t> midiBuf;
  WriteMidiToBuffer(midiBuf);
  return pRoot->UI_WriteBufferToFile(filepath, &midiBuf[0], (uint32_t) midiBuf.size());
}

void MidiFile::WriteMidiToBuffer(vector<uint8_t> &buf) {
  size_t nNumTracks = aTracks.size();
  buf.push_back('M');
  buf.push_back('T');
  buf.push_back('h');
  buf.push_back('d');
  buf.push_back(0);
  buf.push_back(0);
  buf.push_back(0);
  buf.push_back(6);                //MThd length - always 6
  buf.push_back(0);
  buf.push_back(1);                //Midi format - type 1
  buf.push_back((nNumTracks & 0xFF00) >> 8);        //num tracks hi
  buf.push_back(nNumTracks & 0x00FF);                //num tracks lo
  buf.push_back((ppqn & 0xFF00) >> 8);
  buf.push_back(ppqn & 0xFF);

  Sort();

  for (uint32_t i = 0; i < aTracks.size(); i++) {
    if (aTracks[i]) {
      vector<uint8_t> trackBuf;
      globalTranspose = 0;
      aTracks[i]->WriteTrack(trackBuf);
      buf.insert(buf.end(), trackBuf.begin(), trackBuf.end());
    }
  }
  globalTranspose = 0;
}


//  *********
//  MidiTrack
//  *********

MidiTrack::MidiTrack(MidiFile *theParentSeq, bool monophonic)
    : parentSeq(theParentSeq),
      bMonophonic(monophonic),
      DeltaTime(0),
      prevDurEvent(NULL),
      prevKey(0),
      channelGroup(0),
      bHasEndOfTrack(false) {

}

MidiTrack::~MidiTrack(void) {
  DeleteVect<MidiEvent>(aEvents);
}

void MidiTrack::Sort(void) {
  stable_sort(aEvents.begin(), aEvents.end(), PriorityCmp());    //Sort all the events by priority
  stable_sort(aEvents.begin(),
              aEvents.end(),
              AbsTimeCmp());    //Sort all the events by absolute time, so that delta times can be recorded correctly
  if (!bHasEndOfTrack && aEvents.size()) {
    aEvents.push_back(new EndOfTrackEvent(this, aEvents.back()->AbsTime));
    bHasEndOfTrack = true;
  }
}

void MidiTrack::WriteTrack(vector<uint8_t> &buf) {
  buf.push_back('M');
  buf.push_back('T');
  buf.push_back('r');
  buf.push_back('k');
  buf.push_back(0);
  buf.push_back(0);
  buf.push_back(0);
  buf.push_back(0);
  uint32_t time = 0;                //start at 0 ticks

  vector<MidiEvent *> finalEvents(aEvents);
  vector<MidiEvent *> &globEvents = parentSeq->globalTrack.aEvents;
  finalEvents.insert(finalEvents.end(), globEvents.begin(), globEvents.end());

  stable_sort(finalEvents.begin(), finalEvents.end(), PriorityCmp());    //Sort all the events by priority
  stable_sort(finalEvents.begin(),
              finalEvents.end(),
              AbsTimeCmp());    //Sort all the events by absolute time, so that delta times can be recorded correctly

  size_t numEvents = finalEvents.size();

  for (size_t i = 0; i < numEvents; i++)
    time = finalEvents[i]->WriteEvent(buf, time);        //write all events into the buffer

  uint32_t trackSize = (uint32_t) (buf.size() - 8);        //-8 for MTrk and size that shouldn't be accounted for
  buf[4] = (uint8_t) ((trackSize & 0xFF000000) >> 24);
  buf[5] = (uint8_t) ((trackSize & 0x00FF0000) >> 16);
  buf[6] = (uint8_t) ((trackSize & 0x0000FF00) >> 8);
  buf[7] = (uint8_t) (trackSize & 0x000000FF);
}

void MidiTrack::SetChannelGroup(int theChannelGroup) {
  channelGroup = theChannelGroup;
}

//Delta Time Functions
uint32_t MidiTrack::GetDelta(void) {
  return DeltaTime;
}

void MidiTrack::SetDelta(uint32_t NewDelta) {
  DeltaTime = NewDelta;
}

void MidiTrack::AddDelta(uint32_t AddDelta) {
  DeltaTime += AddDelta;
}

void MidiTrack::SubtractDelta(uint32_t SubtractDelta) {
  DeltaTime -= SubtractDelta;
}

void MidiTrack::ResetDelta(void) {
  DeltaTime = 0;
}


void MidiTrack::AddNoteOn(uint8_t channel, int8_t key, int8_t vel) {
  aEvents.push_back(new NoteEvent(this, channel, GetDelta(), true, key, vel));
}

void MidiTrack::InsertNoteOn(uint8_t channel, int8_t key, int8_t vel, uint32_t absTime) {
  aEvents.push_back(new NoteEvent(this, channel, absTime, true, key, vel));
}

void MidiTrack::AddNoteOff(uint8_t channel, int8_t key) {
  aEvents.push_back(new NoteEvent(this, channel, GetDelta(), false, key));
}

void MidiTrack::InsertNoteOff(uint8_t channel, int8_t key, uint32_t absTime) {
  aEvents.push_back(new NoteEvent(this, channel, absTime, false, key));
}

void MidiTrack::AddNoteByDur(uint8_t channel, int8_t key, int8_t vel, uint32_t duration) {
  PurgePrevNoteOffs(GetDelta());
  aEvents.push_back(new NoteEvent(this, channel, GetDelta(), true, key, vel));        //add note on
  NoteEvent *prevDurNoteOff = new NoteEvent(this, channel, GetDelta() + duration, false, key);
  prevDurNoteOffs.push_back(prevDurNoteOff);
  aEvents.push_back(prevDurNoteOff);    //add note off at end of dur
}

//TODO: MOVE! This definitely doesn't belong here.
void MidiTrack::AddNoteByDur_TriAce(uint8_t channel, int8_t key, int8_t vel, uint32_t duration) {
  uint32_t CurDelta = GetDelta();
  size_t nNumEvents = aEvents.size();
  NoteEvent *ContNote;    // Continuted Note

  ContNote = NULL;
  for (size_t curEvt = 0; curEvt < nNumEvents; curEvt++) {
    // Check for a event on this track with the following conditions:
    //	1. Its Event Delta Time is > current Delta Time.
    //	2. It's a Note Off event
    //	3. Its key matches the key of the new note.
    // If so, we're restarting an already played note. In the case of the TriAce driver that means,
    // that we resume the note, so we need to move the NoteOff event.

    // Note: In previous TriAce drivers (like MegaDrive and SNES versions),
    //       a Note gets extended by a Note On event at the tick where another note expires.
    //       Valkyrie Profile: 225 Fragments of the Heart confirms, that this is NOT the case in the PS1 version.
    if (aEvents[curEvt]->AbsTime > CurDelta && aEvents[curEvt]->GetEventType() == MIDIEVENT_NOTEON) {
      NoteEvent *NoteEvt = (NoteEvent *) aEvents[curEvt];
      if (NoteEvt->key == key && !NoteEvt->bNoteDown) {
        ContNote = NoteEvt;
        break;
      }
    }
  }

  if (ContNote == NULL) {
    PurgePrevNoteOffs(CurDelta);
    aEvents.push_back(new NoteEvent(this, channel, CurDelta, true, key, vel));        //add note on
    NoteEvent *prevDurNoteOff = new NoteEvent(this, channel, CurDelta + duration, false, key);
    prevDurNoteOffs.push_back(prevDurNoteOff);
    aEvents.push_back(prevDurNoteOff);    //add note off at end of dur
  }
  else {
    ContNote->AbsTime = CurDelta + duration;    // fix DeltaTime of the already inserted NoteOff event
  }
}

void MidiTrack::InsertNoteByDur(uint8_t channel, int8_t key, int8_t vel, uint32_t duration, uint32_t absTime) {
  PurgePrevNoteOffs(max(GetDelta(), absTime));
  aEvents.push_back(new NoteEvent(this, channel, absTime, true, key, vel));        //add note on
  NoteEvent *prevDurNoteOff = new NoteEvent(this, channel, absTime + duration, false, key);
  prevDurNoteOffs.push_back(prevDurNoteOff);
  aEvents.push_back(prevDurNoteOff);    //add note off at end of dur
}

void MidiTrack::PurgePrevNoteOffs() {
  prevDurNoteOffs.clear();
}

void MidiTrack::PurgePrevNoteOffs(uint32_t absTime) {
  auto it = prevDurNoteOffs.begin();
  while (it != prevDurNoteOffs.end()) {
    if ((*it)->AbsTime <= absTime) {
      it = prevDurNoteOffs.erase(it);
    }
    else {
      ++it;
    }
  }
}

/*void MidiTrack::AddVolMarker(uint8_t channel, uint8_t vol, int8_t priority)
{
	MidiEvent* newEvent = new VolMarkerEvent(this, channel, GetDelta(), vol);
	aEvents.push_back(newEvent);
}

void MidiTrack::InsertVolMarker(uint8_t channel, uint8_t vol, uint32_t absTime, int8_t priority)
{
	MidiEvent* newEvent = new VolMarkerEvent(this, channel, absTime, vol);
	aEvents.push_back(newEvent);
}*/

void MidiTrack::AddControllerEvent(uint8_t channel, uint8_t controllerNum, uint8_t theDataByte) {
  aEvents.push_back(new ControllerEvent(this, channel, GetDelta(), controllerNum, theDataByte));
}

void MidiTrack::InsertControllerEvent(uint8_t channel, uint8_t controllerNum, uint8_t theDataByte, uint32_t absTime) {
  aEvents.push_back(new ControllerEvent(this, channel, absTime, controllerNum, theDataByte));
}

void MidiTrack::AddVol(uint8_t channel, uint8_t vol) {
  aEvents.push_back(new VolumeEvent(this, channel, GetDelta(), vol));
}

void MidiTrack::InsertVol(uint8_t channel, uint8_t vol, uint32_t absTime) {
  aEvents.push_back(new VolumeEvent(this, channel, absTime, vol));
}

//mast volume has a full byte of resolution
void MidiTrack::AddMasterVol(uint8_t channel, uint8_t mastVol) {
  MidiEvent *newEvent = new MastVolEvent(this, channel, GetDelta(), mastVol);
  aEvents.push_back(newEvent);
}

void MidiTrack::InsertMasterVol(uint8_t channel, uint8_t mastVol, uint32_t absTime) {
  MidiEvent *newEvent = new MastVolEvent(this, channel, absTime, mastVol);
  aEvents.push_back(newEvent);
}

void MidiTrack::AddExpression(uint8_t channel, uint8_t expression) {
  aEvents.push_back(new ExpressionEvent(this, channel, GetDelta(), expression));
}

void MidiTrack::InsertExpression(uint8_t channel, uint8_t expression, uint32_t absTime) {
  aEvents.push_back(new ExpressionEvent(this, channel, absTime, expression));
}

void MidiTrack::AddSustain(uint8_t channel, uint8_t depth) {
  aEvents.push_back(new SustainEvent(this, channel, GetDelta(), depth));
}

void MidiTrack::InsertSustain(uint8_t channel, uint8_t depth, uint32_t absTime) {
  aEvents.push_back(new SustainEvent(this, channel, absTime, depth));
}

void MidiTrack::AddPortamento(uint8_t channel, bool bOn) {
  aEvents.push_back(new PortamentoEvent(this, channel, GetDelta(), bOn));
}

void MidiTrack::InsertPortamento(uint8_t channel, bool bOn, uint32_t absTime) {
  aEvents.push_back(new PortamentoEvent(this, channel, absTime, bOn));
}

void MidiTrack::AddPortamentoTime(uint8_t channel, uint8_t time) {
  aEvents.push_back(new PortamentoTimeEvent(this, channel, GetDelta(), time));
}

void MidiTrack::InsertPortamentoTime(uint8_t channel, uint8_t time, uint32_t absTime) {
  aEvents.push_back(new PortamentoTimeEvent(this, channel, absTime, time));
}

void MidiTrack::AddPortamentoTimeFine(uint8_t channel, uint8_t time) {
  aEvents.push_back(new PortamentoTimeFineEvent(this, channel, GetDelta(), time));
}

void MidiTrack::InsertPortamentoTimeFine(uint8_t channel, uint8_t time, uint32_t absTime) {
  aEvents.push_back(new PortamentoTimeFineEvent(this, channel, absTime, time));
}

void MidiTrack::AddPortamentoControl(uint8_t channel, uint8_t key) {
  aEvents.push_back(new PortamentoControlEvent(this, channel, GetDelta(), key));
}

void MidiTrack::AddMono(uint8_t channel) {
  aEvents.push_back(new MonoEvent(this, channel, GetDelta()));
}

void MidiTrack::InsertMono(uint8_t channel, uint32_t absTime) {
  aEvents.push_back(new MonoEvent(this, channel, absTime));
}

void MidiTrack::AddPan(uint8_t channel, uint8_t pan) {
  aEvents.push_back(new PanEvent(this, channel, GetDelta(), pan));
}

void MidiTrack::InsertPan(uint8_t channel, uint8_t pan, uint32_t absTime) {
  aEvents.push_back(new PanEvent(this, channel, absTime, pan));
}

void MidiTrack::AddReverb(uint8_t channel, uint8_t reverb) {
  aEvents.push_back(new ControllerEvent(this, channel, GetDelta(), 91, reverb));
}

void MidiTrack::InsertReverb(uint8_t channel, uint8_t reverb, uint32_t absTime) {
  aEvents.push_back(new ControllerEvent(this, channel, absTime, 91, reverb));
}

void MidiTrack::AddModulation(uint8_t channel, uint8_t depth) {
  aEvents.push_back(new ModulationEvent(this, channel, GetDelta(), depth));
}

void MidiTrack::InsertModulation(uint8_t channel, uint8_t depth, uint32_t absTime) {
  aEvents.push_back(new ControllerEvent(this, channel, absTime, 1, depth));
}

void MidiTrack::AddBreath(uint8_t channel, uint8_t depth) {
  aEvents.push_back(new BreathEvent(this, channel, GetDelta(), depth));
}

void MidiTrack::InsertBreath(uint8_t channel, uint8_t depth, uint32_t absTime) {
  aEvents.push_back(new ControllerEvent(this, channel, absTime, 2, depth));
}

void MidiTrack::AddPitchBend(uint8_t channel, int16_t bend) {
  aEvents.push_back(new PitchBendEvent(this, channel, GetDelta(), bend));
}

void MidiTrack::InsertPitchBend(uint8_t channel, int16_t bend, uint32_t absTime) {
  aEvents.push_back(new PitchBendEvent(this, channel, absTime, bend));
}

void MidiTrack::AddPitchBendRange(uint8_t channel, uint8_t semitones, uint8_t cents) {
  InsertPitchBendRange(channel, semitones, cents, GetDelta());
}

void MidiTrack::InsertPitchBendRange(uint8_t channel, uint8_t semitones, uint8_t cents, uint32_t absTime) {
  // We push the LSB controller event first as somee virtual instruments only react upon receiving MSB
  aEvents.push_back(new ControllerEvent(this, channel, absTime, 101, 0, PRIORITY_HIGHER - 1));
  aEvents.push_back(new ControllerEvent(this, channel, absTime, 100, 0, PRIORITY_HIGHER - 1));
  aEvents.push_back(new ControllerEvent(this, channel, absTime, 38, cents, PRIORITY_HIGHER - 1));
  aEvents.push_back(new ControllerEvent(this, channel, absTime, 6, semitones, PRIORITY_HIGHER - 1));
}

void MidiTrack::AddFineTuning(uint8_t channel, uint8_t msb, uint8_t lsb) {
  InsertFineTuning(channel, msb, lsb, GetDelta());
}

void MidiTrack::InsertFineTuning(uint8_t channel, uint8_t msb, uint8_t lsb, uint32_t absTime) {
  // We push the LSB controller event first as somee virtual instruments only react upon receiving MSB
  aEvents.push_back(new ControllerEvent(this, channel, absTime, 101, 0, PRIORITY_HIGHER - 1));
  aEvents.push_back(new ControllerEvent(this, channel, absTime, 100, 1, PRIORITY_HIGHER - 1));
  aEvents.push_back(new ControllerEvent(this, channel, absTime, 38, lsb, PRIORITY_HIGHER - 1));
  aEvents.push_back(new ControllerEvent(this, channel, absTime, 6, msb, PRIORITY_HIGHER - 1));
}

void MidiTrack::AddFineTuning(uint8_t channel, double cents) {
  InsertFineTuning(channel, cents, GetDelta());
}

void MidiTrack::InsertFineTuning(uint8_t channel, double cents, uint32_t absTime) {
  double semitones = max(-1.0, min(1.0, cents / 100.0));
  int16_t midiTuning = min((int) (8192 * semitones + 0.5), 8191) + 8192;
  InsertFineTuning(channel, midiTuning >> 7, midiTuning & 0x7f, absTime);
}

void MidiTrack::AddCoarseTuning(uint8_t channel, uint8_t msb, uint8_t lsb) {
  InsertCoarseTuning(channel, msb, lsb, GetDelta());
}

void MidiTrack::InsertCoarseTuning(uint8_t channel, uint8_t msb, uint8_t lsb, uint32_t absTime) {
  aEvents.push_back(new ControllerEvent(this, channel, absTime, 101, 0, PRIORITY_HIGHER - 1));
  aEvents.push_back(new ControllerEvent(this, channel, absTime, 100, 2, PRIORITY_HIGHER - 1));
  aEvents.push_back(new ControllerEvent(this, channel, absTime, 38, lsb, PRIORITY_HIGHER - 1));
  aEvents.push_back(new ControllerEvent(this, channel, absTime, 6, msb, PRIORITY_HIGHER - 1));
}

void MidiTrack::AddCoarseTuning(uint8_t channel, double semitones) {
  InsertCoarseTuning(channel, semitones, GetDelta());
}

void MidiTrack::InsertCoarseTuning(uint8_t channel, double semitones, uint32_t absTime) {
  semitones = max(-64.0, min(64.0, semitones));
  int16_t midiTuning = min((int) (128 * semitones + 0.5), 8191) + 8192;
  InsertFineTuning(channel, midiTuning >> 7, midiTuning & 0x7f, absTime);
}

void MidiTrack::AddModulationDepthRange(uint8_t channel, uint8_t msb, uint8_t lsb) {
  InsertModulationDepthRange(channel, msb, lsb, GetDelta());
}

void MidiTrack::InsertModulationDepthRange(uint8_t channel, uint8_t msb, uint8_t lsb, uint32_t absTime) {
  aEvents.push_back(new ControllerEvent(this, channel, absTime, 101, 0, PRIORITY_HIGHER - 1));
  aEvents.push_back(new ControllerEvent(this, channel, absTime, 100, 5, PRIORITY_HIGHER - 1));
  aEvents.push_back(new ControllerEvent(this, channel, absTime, 38, lsb, PRIORITY_HIGHER - 1));
  aEvents.push_back(new ControllerEvent(this, channel, absTime, 6, msb, PRIORITY_HIGHER - 1));
}

void MidiTrack::AddModulationDepthRange(uint8_t channel, double semitones) {
  InsertModulationDepthRange(channel, semitones, GetDelta());
}

void MidiTrack::InsertModulationDepthRange(uint8_t channel, double semitones, uint32_t absTime) {
  semitones = max(-64.0, min(64.0, semitones));
  int16_t midiTuning = min((int) (128 * semitones + 0.5), 8191) + 8192;
  InsertFineTuning(channel, midiTuning >> 7, midiTuning & 0x7f, absTime);
}

void MidiTrack::AddProgramChange(uint8_t channel, uint8_t progNum) {
  aEvents.push_back(new ProgChangeEvent(this, channel, GetDelta(), progNum));
//  aEvents.push_back(new ControllerEvent(this, channel, GetDelta(), 115, progNum));
}

void MidiTrack::AddBankSelect(uint8_t channel, uint8_t bank) {
  aEvents.push_back(new BankSelectEvent(this, channel, GetDelta(), bank));
}

void MidiTrack::AddBankSelectFine(uint8_t channel, uint8_t lsb) {
  aEvents.push_back(new BankSelectFineEvent(this, channel, GetDelta(), lsb));
}

void MidiTrack::InsertBankSelect(uint8_t channel, uint8_t bank, uint32_t absTime) {
  aEvents.push_back(new ControllerEvent(this, channel, absTime, 0, bank));
}


void MidiTrack::AddTempo(uint32_t microSeconds) {
  aEvents.push_back(new TempoEvent(this, GetDelta(), microSeconds));
  //bAddedTempo = true;
}

void MidiTrack::AddTempoBPM(double BPM) {
  uint32_t microSecs = (uint32_t) roundi((double) 60000000 / BPM);
  aEvents.push_back(new TempoEvent(this, GetDelta(), microSecs));
  //bAddedTempo = true;
}

void MidiTrack::InsertTempo(uint32_t microSeconds, uint32_t absTime) {
  aEvents.push_back(new TempoEvent(this, absTime, microSeconds));
  //bAddedTempo = true;
}

void MidiTrack::InsertTempoBPM(double BPM, uint32_t absTime) {
  uint32_t microSecs = (uint32_t) roundi((double) 60000000 / BPM);
  aEvents.push_back(new TempoEvent(this, absTime, microSecs));
  //bAddedTempo = true;
}

void MidiTrack::AddMidiPort(uint8_t port) {
  aEvents.push_back(new MidiPortEvent(this, GetDelta(), port));
}

void MidiTrack::InsertMidiPort(uint8_t port, uint32_t absTime) {
  aEvents.push_back(new MidiPortEvent(this, absTime, port));
}

void MidiTrack::AddTimeSig(uint8_t numer, uint8_t denom, uint8_t ticksPerQuarter) {
  aEvents.push_back(new TimeSigEvent(this, GetDelta(), numer, denom, ticksPerQuarter));
  //bAddedTimeSig = true;
}

void MidiTrack::InsertTimeSig(uint8_t numer, uint8_t denom, uint8_t ticksPerQuarter, uint32_t absTime) {
  aEvents.push_back(new TimeSigEvent(this, absTime, numer, denom, ticksPerQuarter));
  //bAddedTimeSig = true;
}

void MidiTrack::AddEndOfTrack(void) {
  aEvents.push_back(new EndOfTrackEvent(this, GetDelta()));
  bHasEndOfTrack = true;
}

void MidiTrack::InsertEndOfTrack(uint32_t absTime) {
  aEvents.push_back(new EndOfTrackEvent(this, absTime));
  bHasEndOfTrack = true;
}

void MidiTrack::AddText(const std::wstring &wstr) {
  aEvents.push_back(new TextEvent(this, GetDelta(), wstr));
}

void MidiTrack::InsertText(const std::wstring &wstr, uint32_t absTime) {
  aEvents.push_back(new TextEvent(this, absTime, wstr));
}

void MidiTrack::AddSeqName(const std::wstring &wstr) {
  aEvents.push_back(new SeqNameEvent(this, GetDelta(), wstr));
}

void MidiTrack::InsertSeqName(const std::wstring &wstr, uint32_t absTime) {
  aEvents.push_back(new SeqNameEvent(this, absTime, wstr));
}

void MidiTrack::AddTrackName(const std::wstring &wstr) {
  aEvents.push_back(new TrackNameEvent(this, GetDelta(), wstr));
}

void MidiTrack::InsertTrackName(const std::wstring &wstr, uint32_t absTime) {
  aEvents.push_back(new TrackNameEvent(this, absTime, wstr));
}

// SPECIAL NON-MIDI EVENTS

// Transpose events offset the key when we write the Midi file.
//  used to implement global transpose events found in QSound

//void MidiTrack::AddTranspose(int8_t semitones)
//{
//	aEvents.push_back(new TransposeEvent(this, GetDelta(), semitones));
//}

void MidiTrack::InsertGlobalTranspose(uint32_t absTime, int8_t semitones) {
  aEvents.push_back(new GlobalTransposeEvent(this, absTime, semitones));
}


void MidiTrack::AddMarker(uint8_t channel,
                          const string &markername,
                          uint8_t databyte1,
                          uint8_t databyte2,
                          int8_t priority) {
  aEvents.push_back(new MarkerEvent(this, channel, GetDelta(), markername, databyte1, databyte2, priority));
}



//  *********
//  MidiEvent
//  *********

MidiEvent::MidiEvent(MidiTrack *thePrntTrk, uint32_t absoluteTime, uint8_t theChannel, int8_t thePriority)
    : prntTrk(thePrntTrk), AbsTime(absoluteTime), channel(theChannel), priority(thePriority) {
}

MidiEvent::~MidiEvent(void) {
}

bool MidiEvent::IsMetaEvent() {
  MidiEventType type = GetEventType();
  return type == MIDIEVENT_TEMPO ||
         type == MIDIEVENT_TEXT ||
         type == MIDIEVENT_MIDIPORT ||
         type == MIDIEVENT_TIMESIG ||
         type == MIDIEVENT_ENDOFTRACK;
}

bool MidiEvent::IsSysexEvent() {
  MidiEventType type = GetEventType();
  return type == MIDIEVENT_MASTERVOL ||
         type == MIDIEVENT_RESET;
}

void MidiEvent::WriteVarLength(vector<uint8_t> &buf, uint32_t value) {
  unsigned long buffer;
  buffer = value & 0x7F;

  while ((value >>= 7)) {
    buffer <<= 8;
    buffer |= ((value & 0x7F) | 0x80);
  }

  while (true) {
    buf.push_back((uint8_t) buffer);
    if (buffer & 0x80)
      buffer >>= 8;
    else
      break;
  }
}

uint32_t MidiEvent::WriteSysexEvent(vector<uint8_t> &buf, uint32_t time, uint8_t *data, size_t dataSize) {
  WriteVarLength(buf, AbsTime - time);
  buf.push_back(0xF0);
  WriteVarLength(buf, (uint32_t) dataSize);
  for (size_t dataIndex = 0; dataIndex < dataSize; dataIndex++) {
    buf.push_back(data[dataIndex]);
  }
  return AbsTime;
}

uint32_t MidiEvent::WriteMetaEvent(vector<uint8_t> &buf,
                                   uint32_t time,
                                   uint8_t metaType,
                                   uint8_t *data,
                                   size_t dataSize) {
  WriteVarLength(buf, AbsTime - time);
  buf.push_back(0xFF);
  buf.push_back(metaType);
  WriteVarLength(buf, (uint32_t) dataSize);
  for (size_t dataIndex = 0; dataIndex < dataSize; dataIndex++) {
    buf.push_back(data[dataIndex]);
  }
  return AbsTime;
}

uint32_t MidiEvent::WriteMetaTextEvent(vector<uint8_t> &buf, uint32_t time, uint8_t metaType, wstring wstr) {
  string str = wstring2string(wstr);
  return WriteMetaEvent(buf, time, metaType, (uint8_t *) str.c_str(), str.length());
}

//void MidiEvent::PrepareWrite(vector<MidiEvent*> & aEvents)
//{
//aEvents.push_back(MakeCopy());
//}

std::wstring MidiEvent::GetNoteName(int noteNumber) {
  const wchar_t *noteNames[12] = {L"C", L"C#", L"D", L"D#", L"E", L"F", L"F#", L"G", L"G#", L"A", L"A#", L"B"};

  int octave;
  int key;
  if (noteNumber >= 0) {
    octave = noteNumber / 12;
    key = noteNumber % 12;
  }
  else {
    octave = -(-noteNumber / 12) - 1;
    key = 12 - (-(noteNumber + 1) % 12) - 1;
  }
  octave--;

  std::wstringstream name;
  name << noteNames[key] << " " << octave;
  return name.str();
}


bool MidiEvent::operator<(const MidiEvent &theMidiEvent) const {
  return (AbsTime < theMidiEvent.AbsTime);
}

bool MidiEvent::operator>(const MidiEvent &theMidiEvent) const {
  return (AbsTime > theMidiEvent.AbsTime);
}


//  *********
//  NoteEvent
//  *********


NoteEvent::NoteEvent(MidiTrack *prntTrk,
                     uint8_t channel,
                     uint32_t absoluteTime,
                     bool bnoteDown,
                     uint8_t theKey,
                     uint8_t theVel)
    : MidiEvent(prntTrk, absoluteTime, channel, PRIORITY_LOWER),
      bNoteDown(bnoteDown),
      key(theKey),
      vel(theVel) {
}

uint32_t NoteEvent::WriteEvent(vector<uint8_t> &buf, uint32_t time) {
  WriteVarLength(buf, AbsTime - time);
  if (bNoteDown)
    buf.push_back(0x90 + channel);
  else
    buf.push_back(0x80 + channel);

  prntTrk->prevKey = key + ((channel == 9) ? 0 : prntTrk->parentSeq->globalTranspose);
  buf.push_back(prntTrk->prevKey);

  buf.push_back(vel);

  return AbsTime;
}

//  ************
//  DurNoteEvent
//  ************

//DurNoteEvent::DurNoteEvent(MidiTrack* prntTrk, uint8_t channel, uint32_t absoluteTime, uint8_t theKey, uint8_t theVel, uint32_t theDur)
//: MidiEvent(prntTrk, absoluteTime, channel, PRIORITY_LOWER), key(theKey), vel(theVel), duration(theDur)
//{
//}
/*
DurNoteEvent* DurNoteEvent::MakeCopy()
{
	return new DurNoteEvent(prntTrk, channel, AbsTime, key, vel, duration);
}*/

/*void DurNoteEvent::PrepareWrite(vector<MidiEvent*> & aEvents)
{
	prntTrk->aEvents.push_back(new NoteEvent(prntTrk, channel, AbsTime, true, key, vel));	//add note on
	prntTrk->aEvents.push_back(new NoteEvent(prntTrk, channel, AbsTime+duration, false, key, vel));  //add note off at end of dur
}*/

//uint32_t DurNoteEvent::WriteEvent(vector<uint8_t> & buf, uint32_t time)		//we do note use WriteEvent on DurNoteEvents... this is what PrepareWrite is for, to create NoteEvents in substitute
//{
//	return false;
//}


//  ********
//  VolEvent
//  ********

/*VolEvent::VolEvent(MidiTrack *prntTrk, uint8_t channel, uint32_t absoluteTime, uint8_t theVol, int8_t thePriority)
: ControllerEvent(prntTrk, channel, absoluteTime, 7), vol(theVol)
{
}

VolEvent* VolEvent::MakeCopy()
{
	return new VolEvent(prntTrk, channel, AbsTime, vol, priority);
}*/


//  ************
//  MastVolEvent
//  ************

MastVolEvent::MastVolEvent(MidiTrack *prntTrk, uint8_t channel, uint32_t absoluteTime, uint8_t theMastVol)
    : MidiEvent(prntTrk, absoluteTime, channel, PRIORITY_HIGHER), mastVol(theMastVol) {
}

uint32_t MastVolEvent::WriteEvent(vector<uint8_t> &buf, uint32_t time) {
  uint8_t data[7] = {0x7F, 0x7F, 0x04, 0x01, /*LSB*/0, (uint8_t) (mastVol & 0x7F), 0xF7};
  return WriteSysexEvent(buf, time, data, 7);
}

//  ***************
//  ControllerEvent
//  ***************

ControllerEvent::ControllerEvent(MidiTrack *prntTrk,
                                 uint8_t channel,
                                 uint32_t absoluteTime,
                                 uint8_t controllerNum,
                                 uint8_t theDataByte,
                                 int8_t thePriority)
    : MidiEvent(prntTrk, absoluteTime, channel, thePriority), controlNum(controllerNum), dataByte(theDataByte) {
}

uint32_t ControllerEvent::WriteEvent(vector<uint8_t> &buf, uint32_t time) {
  WriteVarLength(buf, AbsTime - time);
  buf.push_back(0xB0 + channel);
  buf.push_back(controlNum & 0x7F);
  buf.push_back(dataByte);
  return AbsTime;
}

//  ***************
//  ProgChangeEvent
//  ***************

ProgChangeEvent::ProgChangeEvent(MidiTrack *prntTrk, uint8_t channel, uint32_t absoluteTime, uint8_t progNum)
    : MidiEvent(prntTrk, absoluteTime, channel, PRIORITY_HIGH), programNum(progNum) {
}

uint32_t ProgChangeEvent::WriteEvent(vector<uint8_t> &buf, uint32_t time) {
  WriteVarLength(buf, AbsTime - time);
  buf.push_back(0xC0 + channel);
  buf.push_back(programNum & 0x7F);
  return AbsTime;
}


//  **************
//  PitchBendEvent
//  **************

PitchBendEvent::PitchBendEvent(MidiTrack *prntTrk, uint8_t channel, uint32_t absoluteTime, int16_t bendAmt)
    : MidiEvent(prntTrk, absoluteTime, channel, PRIORITY_MIDDLE), bend(bendAmt) {
}

uint32_t PitchBendEvent::WriteEvent(vector<uint8_t> &buf, uint32_t time) {
  uint8_t loByte = (bend + 0x2000) & 0x7F;
  uint8_t hiByte = ((bend + 0x2000) & 0x3F80) >> 7;
  WriteVarLength(buf, AbsTime - time);
  buf.push_back(0xE0 + channel);
  buf.push_back(loByte);
  buf.push_back(hiByte);
  return AbsTime;
}

//  **********
//  TempoEvent
//  **********

TempoEvent::TempoEvent(MidiTrack *prntTrk, uint32_t absoluteTime, uint32_t microSeconds)
    : MidiEvent(prntTrk, absoluteTime, 0, PRIORITY_HIGHEST), microSecs(microSeconds) {
}

uint32_t TempoEvent::WriteEvent(vector<uint8_t> &buf, uint32_t time) {
  uint8_t data[3] = {
      (uint8_t) ((microSecs & 0xFF0000) >> 16),
      (uint8_t) ((microSecs & 0x00FF00) >> 8),
      (uint8_t) (microSecs & 0x0000FF)
  };
  return WriteMetaEvent(buf, time, 0x51, data, 3);
}


//  *************
//  MidiPortEvent
//  *************

MidiPortEvent::MidiPortEvent(MidiTrack *prntTrk, uint32_t absoluteTime, uint8_t port)
    : MidiEvent(prntTrk, absoluteTime, 0, PRIORITY_HIGHEST), port(port) {
}

uint32_t MidiPortEvent::WriteEvent(vector<uint8_t> &buf, uint32_t time) {
  return WriteMetaEvent(buf, time, 0x21, &port, 1);
}


//  ************
//  TimeSigEvent
//  ************

TimeSigEvent::TimeSigEvent(MidiTrack *prntTrk,
                           uint32_t absoluteTime,
                           uint8_t numerator,
                           uint8_t denominator,
                           uint8_t clicksPerQuarter)
    : MidiEvent(prntTrk, absoluteTime, 0, PRIORITY_HIGHEST), numer(numerator), denom(denominator),
      ticksPerQuarter(clicksPerQuarter) {
}

uint32_t TimeSigEvent::WriteEvent(vector<uint8_t> &buf, uint32_t time) {
  //denom is expressed in power of 2... so if we have 6/8 time.  it's 6 = 2^x  ==  ln6 / ln2
  uint8_t data[4] = {
      numer,
      (uint8_t) (log((double) denom) / 0.69314718055994530941723212145818),
      ticksPerQuarter,
      8
  };
  return WriteMetaEvent(buf, time, 0x58, data, 4);
}


//  ***************
//  EndOfTrackEvent
//  ***************

EndOfTrackEvent::EndOfTrackEvent(MidiTrack *prntTrk, uint32_t absoluteTime)
    : MidiEvent(prntTrk, absoluteTime, 0, PRIORITY_LOWEST) {
}


uint32_t EndOfTrackEvent::WriteEvent(vector<uint8_t> &buf, uint32_t time) {
  return WriteMetaEvent(buf, time, 0x2F, NULL, 0);
}

//  *********
//  TextEvent
//  *********

TextEvent::TextEvent(MidiTrack *prntTrk, uint32_t absoluteTime, const std::wstring &wstr)
    : MidiEvent(prntTrk, absoluteTime, 0, PRIORITY_LOWEST), text(wstr) {
}

uint32_t TextEvent::WriteEvent(vector<uint8_t> &buf, uint32_t time) {
  return WriteMetaTextEvent(buf, time, 0x01, text);
}

//  ************
//  SeqNameEvent
//  ************

SeqNameEvent::SeqNameEvent(MidiTrack *prntTrk, uint32_t absoluteTime, const std::wstring &wstr)
    : MidiEvent(prntTrk, absoluteTime, 0, PRIORITY_LOWEST), text(wstr) {
}

uint32_t SeqNameEvent::WriteEvent(vector<uint8_t> &buf, uint32_t time) {
  return WriteMetaTextEvent(buf, time, 0x03, text);
}

//  **************
//  TrackNameEvent
//  **************

TrackNameEvent::TrackNameEvent(MidiTrack *prntTrk, uint32_t absoluteTime, const std::wstring &wstr)
    : MidiEvent(prntTrk, absoluteTime, 0, PRIORITY_LOWEST), text(wstr) {
}

uint32_t TrackNameEvent::WriteEvent(vector<uint8_t> &buf, uint32_t time) {
  return WriteMetaTextEvent(buf, time, 0x03, text);
}

//  ************
//  GMResetEvent
//  ************

GMResetEvent::GMResetEvent(MidiTrack *prntTrk, uint32_t absoluteTime)
    : MidiEvent(prntTrk, absoluteTime, 0, PRIORITY_HIGHEST) {
}

uint32_t GMResetEvent::WriteEvent(vector<uint8_t> &buf, uint32_t time) {
  uint8_t data[5] = {0x7E, 0x7F, 0x09, 0x01, 0xF7};
  return WriteSysexEvent(buf, time, data, 5);
}

void MidiTrack::AddGMReset() {
  aEvents.push_back(new GMResetEvent(this, GetDelta()));
}

void MidiTrack::InsertGMReset(uint32_t absTime) {
  aEvents.push_back(new GMResetEvent(this, absTime));
}

//  *************
//  GM2ResetEvent
//  *************

GM2ResetEvent::GM2ResetEvent(MidiTrack *prntTrk, uint32_t absoluteTime)
    : MidiEvent(prntTrk, absoluteTime, 0, PRIORITY_HIGHEST) {
}

uint32_t GM2ResetEvent::WriteEvent(vector<uint8_t> &buf, uint32_t time) {
  uint8_t data[5] = {0x7E, 0x7F, 0x09, 0x03, 0xF7};
  return WriteSysexEvent(buf, time, data, 5);
}

void MidiTrack::AddGM2Reset() {
  aEvents.push_back(new GM2ResetEvent(this, GetDelta()));
}

void MidiTrack::InsertGM2Reset(uint32_t absTime) {
  aEvents.push_back(new GM2ResetEvent(this, absTime));
}

//  ************
//  GSResetEvent
//  ************

GSResetEvent::GSResetEvent(MidiTrack *prntTrk, uint32_t absoluteTime)
    : MidiEvent(prntTrk, absoluteTime, 0, PRIORITY_HIGHEST) {
}

uint32_t GSResetEvent::WriteEvent(vector<uint8_t> &buf, uint32_t time) {
  uint8_t data[10] = {0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7F, 0x00, 0x41, 0xF7};
  return WriteSysexEvent(buf, time, data, 10);
}

void MidiTrack::AddGSReset() {
  aEvents.push_back(new GSResetEvent(this, GetDelta()));
}

void MidiTrack::InsertGSReset(uint32_t absTime) {
  aEvents.push_back(new GSResetEvent(this, absTime));
}

//  ************
//  XGResetEvent
//  ************

XGResetEvent::XGResetEvent(MidiTrack *prntTrk, uint32_t absoluteTime)
    : MidiEvent(prntTrk, absoluteTime, 0, PRIORITY_HIGHEST) {
}

uint32_t XGResetEvent::WriteEvent(vector<uint8_t> &buf, uint32_t time) {
  uint8_t data[8] = {0x43, 0x10, 0x4C, 0x00, 0x00, 0x7E, 0x00, 0xF7};
  return WriteSysexEvent(buf, time, data, 8);
}

void MidiTrack::AddXGReset() {
  aEvents.push_back(new XGResetEvent(this, GetDelta()));
}

void MidiTrack::InsertXGReset(uint32_t absTime) {
  aEvents.push_back(new XGResetEvent(this, absTime));
}

//***************
// SPECIAL EVENTS
//***************

//  **************
//  TransposeEvent
//  **************


GlobalTransposeEvent::GlobalTransposeEvent(MidiTrack *prntTrk, uint32_t absoluteTime, int8_t theSemitones)
    : MidiEvent(prntTrk, absoluteTime, 0, PRIORITY_HIGHEST) {
  semitones = theSemitones;
}

uint32_t GlobalTransposeEvent::WriteEvent(vector<uint8_t> &buf, uint32_t time) {
  this->prntTrk->parentSeq->globalTranspose = this->semitones;
  return time;
}
