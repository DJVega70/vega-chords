#include "../Source/PluginProcessor.h"
#include <iostream>
using namespace vch;
static int fails = 0;
static void check (bool ok, const juce::String& m) { std::cout << (ok ? "PASS " : "FAIL ") << m << "\n"; if (! ok) ++fails; }
static juce::String names (const std::vector<Chord>& cs, int key, int scale) { juce::StringArray a; for (auto& c : cs) a.add (c.name (key, scale)); return a.joinIntoString (" "); }

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    // ---- theory
    check (names (diatonic (0, 0, Triads), 0, 0) == "C Dm Em F G Am Bdim", "C major triads: " + names (diatonic (0, 0, Triads), 0, 0));
    check (names (diatonic (9, 1, Sevenths), 9, 1) == "Am7 Bm7b5 Cmaj7 Dm7 Em7 Fmaj7 G7", "A minor 7ths: " + names (diatonic (9, 1, Sevenths), 9, 1));
    check (names (diatonic (5, 0, Ninths), 5, 0).startsWith ("Fmaj9 Gm9 Am7 Bbmaj9 C9"), "F major 9ths (flats, iii stays m7): " + names (diatonic (5, 0, Ninths), 5, 0));
    { auto b = borrowed (0, 0, Sevenths); check (numeral (b[4], 0) == "V/V" && b[3].name (0, 0) == "Bb7" && numeral (b[1], 0) == "iv7", "C major colour chords: " + names (b, 0, 0)); }
    { auto c = parseNumeral ("ii9", 0); check (c.name (0, 0) == "Dm9" && numeral (c, 0) == "ii9", "parse ii9 in C -> " + c.name (0, 0)); }
    { auto c = parseNumeral ("bVImaj7", 9); check (c.name (9, 1) == "Fmaj7", "parse bVImaj7 in A -> " + c.name (9, 1)); }
    { auto c = detectChord ({ 57, 60, 64, 67 }); check (c.name (0, 0) == "Am7", "detect A C E G -> " + c.name (0, 0)); }
    { auto c = detectChord ({ 50, 53, 57, 60, 64 }); check (c.name (0, 0) == "Dm9", "detect D F A C E -> " + c.name (0, 0)); }
    {
        std::array<float, 12> h {}; for (int pc : { 0, 2, 4, 5, 7, 9, 11, 0, 4, 7, 0, 7 }) h[(size_t) pc] += 1;
        auto [k, minor] = detectKey (h); check (k == 0 && ! minor, "key from C major melody -> " + keyNames()[k] + (minor ? " minor" : " major"));
    }
    {   // voice leading keeps voices close
        VoicingSettings vs; vs.registerNote = 60; vs.voiceLeading = true;
        std::vector<int> prev; int maxMove = 0;
        for (const char* r : { "I", "IV", "V", "vi", "IV", "I" })
        {
            auto v = voice (parseNumeral (r, 0), vs, prev);
            if (! prev.empty()) for (size_t i = 0; i < std::min (v.size(), prev.size()); ++i) maxMove = std::max (maxMove, std::abs (v[i] - prev[i]));
            prev = v;
        }
        check (maxMove <= 5, "voice leading: largest voice move " + juce::String (maxMove) + " semitones");
    }
    { auto v = voiceRaw ({ 0, Maj7, {} }, 60, 0, Drop2); check (v == std::vector<int> { 55, 60, 64, 71 }, "Cmaj7 drop-2 voicing G3 C4 E4 B4"); }

    // ---- processor: transport playback
    std::unique_ptr<juce::AudioProcessor> base (createPluginFilter());
    auto& p = dynamic_cast<VegaChordsProcessor&> (*base);
    const double fs = 48000; const int blk = 512;
    p.setPlayConfigDetails (0, 2, fs, blk); p.prepareToPlay (fs, blk);
    auto set = [&] (const char* id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); };
    set ("key", 0); set ("scale", 0); set ("extension", 0); set ("pattern", 0); set ("bass", 0); set ("humanTime", 0); set ("strum", 0);
    p.numSlots.store (4);
    const char* prog[] { "I", "V", "vi", "IV" };
    for (int i = 0; i < 4; ++i) p.setSlot (i, parseNumeral (prog[i], 0), 4);

    struct Head : juce::AudioPlayHead
    {
        double ppq = 0; bool playing = true;
        juce::Optional<PositionInfo> getPosition() const override { PositionInfo i; i.setBpm (120); i.setPpqPosition (ppq); i.setIsPlaying (playing); return i; }
    } head;
    p.setPlayHead (&head);
    const double spb = fs * 60.0 / 120.0;
    int ons = 0, offs = 0; bool sawFirstChordOnBeat1 = false; std::vector<int> firstChord;
    juce::AudioBuffer<float> buf (2, blk); bool finite = true; float peak = 0;
    for (int b = 0; b < (int) (32 * spb / blk); ++b)          // 2 loops of 16 beats
    {
        head.ppq = b * blk / spb;
        juce::MidiBuffer m; p.processBlock (buf, m);
        for (const auto meta : m)
        {
            const auto msg = meta.getMessage();
            if (msg.isNoteOn()) { ++ons; if (b == 0) { firstChord.push_back (msg.getNoteNumber()); sawFirstChordOnBeat1 = meta.samplePosition == 0; } }
            if (msg.isNoteOff()) ++offs;
        }
        for (int i = 0; i < blk; ++i) { const float v = buf.getSample (0, i); finite &= std::isfinite (v); peak = std::max (peak, std::abs (v)); }
    }
    head.playing = false; { juce::MidiBuffer m; p.processBlock (buf, m); for (const auto meta : m) if (meta.getMessage().isNoteOff()) ++offs; }
    std::sort (firstChord.begin(), firstChord.end());
    juce::String fc; for (int n : firstChord) fc << n << " ";
    check (ons == 24, "2 loops of I-V-vi-IV (triads, sustain) -> " + juce::String (ons) + " note-ons (want 24)");
    check (offs == ons, "every note-on gets a note-off (" + juce::String (offs) + ")");
    check (sawFirstChordOnBeat1 && detectChord (firstChord).name (0, 0) == "C", "first chord on beat 1 is C (" + fc + ")");
    check (finite && peak > 0.01f && peak < 1.5f, "built-in sound plays (peak " + juce::String (peak, 2) + ")");

    // ---- one-finger chord: press D (white key 2) -> ii chord
    set ("trigger", 1);
    {
        juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (1, 62, (juce::uint8) 100), 0);
        p.processBlock (buf, m);
        std::vector<int> got; for (const auto meta : m) if (meta.getMessage().isNoteOn()) got.push_back (meta.getMessage().getNoteNumber());
        check (detectChord (got).name (0, 0) == "Dm", "one-finger: key D plays " + detectChord (got).name (0, 0));
    }
    // ---- MIDI export
    {
        auto f = p.exportMidi (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vega_test.mid"));
        juce::MidiFile mf; juce::FileInputStream in (f); const bool ok = mf.readFrom (in);
        int n = 0; for (int t = 0; t < mf.getNumTracks(); ++t) for (auto* e : *mf.getTrack (t)) if (e->message.isNoteOn()) ++n;
        check (ok && n == 12, "MIDI export: " + juce::String (n) + " notes for one loop (want 12)");
    }
    p.setPlayHead (nullptr);
    std::cout << (fails == 0 ? "ALL PASSED" : "FAILURES: " + std::to_string (fails)) << std::endl;
    return fails;
}
