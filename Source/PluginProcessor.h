#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "ChordTheory.h"

namespace vch
{
constexpr int kMaxSlots = 16;
inline const juce::StringArray& patternNames() { static juce::StringArray a { "Sustain", "Quarters", "Eighths", "House Offbeats", "Syncopated 3-3-2", "Arp Up", "Arp Down", "Arp Up/Down" }; return a; }
struct NoteEvent { double beat, len; int note, vel, ch; };
struct RenderSettings
{
    int pattern = 0; float gate = 0.9f, strumMs = 0; int strumDir = 0; float humanMs = 0, humanVel = 0; int velocity = 96;
    int chordCh = 1, bassCh = 1; bool bass = false; double bpm = 120;
};
/** Turn voiced chords into timed note events for one loop of the progression. */
std::vector<NoteEvent> renderProgression (const std::vector<std::vector<int>>& voiced, const std::vector<int>& bassNotes,
                                          const std::vector<int>& beats, const RenderSettings& s);
}

class VegaChordsProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    VegaChordsProcessor();
    ~VegaChordsProcessor() override;
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Vega Productions Chords"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 1.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // ---- progression (UI writes, audio reads; lock‑free)
    struct Slot { std::atomic<int> root { -1 }, quality { 0 }, beats { 4 }; juce::String label; };
    std::array<Slot, vch::kMaxSlots> slots;
    std::atomic<int> numSlots { 4 };
    std::atomic<int> editVersion { 0 };
    void setSlot (int i, const vch::Chord& c, int beats = -1);
    vch::Chord getSlot (int i) const;              // message thread (includes label)
    vch::Chord getSlotRT (int i) const { return { slots[(size_t) i].root.load(), slots[(size_t) i].quality.load(), {} }; }   // audio thread
    void touch() { editVersion.fetch_add (1); }

    // ---- UI helpers
    vch::VoicingSettings voicing() const;
    vch::RenderSettings renderSettings (double bpm) const;
    int key() const { return juce::roundToInt (p ("key")); }
    int scale() const { return juce::roundToInt (p ("scale")); }
    std::vector<vch::Chord> palette() const;              // 7 diatonic + 7 borrowed
    void audition (const vch::Chord& c);                   // play a chord now (UI click)
    void generate (int style);
    void randomizeColour();
    juce::File exportMidi (const juce::File& dest = {}) const;
    static juce::File presetFolder();
    bool savePreset (const juce::File& f);
    bool loadPreset (const juce::File& f);

    std::atomic<bool> internalPlay { false };
    std::atomic<int> playingSlot { -1 };
    std::atomic<double> uiPpq { 0 };
    std::atomic<int> detectedRoot { -1 }, detectedQuality { 0 };
    std::array<std::atomic<float>, 12> inputHist;
    juce::MidiKeyboardState outKeyboard;                    // shows what the plugin is playing
    std::atomic<float> uiScale { 0.9f };

private:
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    void timerCallback() override;
    void rebuildEvents (double bpm);
    void emitOn (juce::MidiBuffer& out, int offset, int note, int vel, int ch);
    void emitOff (juce::MidiBuffer& out, int offset, int note, int ch);
    void allOff (juce::MidiBuffer& out, int offset);
    void renderPreview (juce::AudioBuffer<float>& buf, const juce::MidiBuffer& events);

    double fs = 48000; juce::int64 sampleClock = 0;
    std::vector<vch::NoteEvent> loopEvents; double loopBeats = 16; int builtVersion = -1; double builtBpm = 0;
    std::vector<int> slotStartBeat;
    struct Pending { juce::int64 at; int note, ch; bool on; int vel; };
    std::vector<Pending> pending;
    std::array<std::array<int, 128>, 16> activeCount {};
    double lastPpq = -1; bool wasPlaying = false, wasInternal = false; double internalPpq = 0;
    // single‑finger triggers
    std::map<int, std::vector<std::pair<int, int>>> triggered;   // key -> (note, ch)
    std::vector<int> lastTriggerVoicing;
    std::vector<int> heldInput;
    // audition
    std::atomic<int> auditionRoot { -1 }, auditionQuality { 0 };
    std::vector<int> lastAuditionVoicing;
    // preview electric piano
    struct EpVoice { bool on = false; int note = 0; float vel = 0, phC = 0, phM = 0, env = 0, modEnv = 0, inc = 0; bool releasing = false; juce::int64 age = 0; };
    std::array<EpVoice, 24> ep; juce::int64 epAge = 0;
    juce::Random rng;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VegaChordsProcessor)
};
