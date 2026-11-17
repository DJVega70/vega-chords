#include "PluginProcessor.h"
#include "PluginEditor.h"
using namespace vch;

// =====================================================================  event generator (shared by playback + MIDI export)
std::vector<NoteEvent> vch::renderProgression (const std::vector<std::vector<int>>& voiced, const std::vector<int>& bassNotes,
                                               const std::vector<int>& beats, const RenderSettings& s)
{
    std::vector<NoteEvent> ev;
    const double msToBeats = s.bpm / 60000.0;
    double start = 0; int hitCounter = 0;
    auto hrand = [] (int a, int b) { juce::uint32 h = (juce::uint32) (a * 73856093) ^ (juce::uint32) (b * 19349663); h ^= h >> 13; h *= 0x5bd1e995; h ^= h >> 15; return (float) (h & 0xffff) / 65535.0f * 2.0f - 1.0f; };
    for (size_t si = 0; si < voiced.size(); ++si)
    {
        const auto& notes = voiced[si];
        const double len = beats[si];
        if (notes.empty()) { start += len; continue; }
        std::vector<double> hits; double hitLen = len;
        switch (s.pattern)
        {
            case 1: for (double b = 0; b < len - 1e-9; b += 1.0) hits.push_back (b); hitLen = 1.0; break;
            case 2: for (double b = 0; b < len - 1e-9; b += 0.5) hits.push_back (b); hitLen = 0.5; break;
            case 3: for (double b = 0.5; b < len - 1e-9; b += 1.0) hits.push_back (b); hitLen = 0.5; break;
            case 4: for (double bar = 0; bar < len - 1e-9; bar += 4.0) for (double o : { 0.0, 0.75, 1.5, 2.0, 2.75, 3.5 }) if (bar + o < len - 1e-9) hits.push_back (bar + o); hitLen = 0.5; break;
            case 5: case 6: case 7: for (double b = 0; b < len - 1e-9; b += 0.25) hits.push_back (b); hitLen = 0.25; break;
            default: hits.push_back (0); hitLen = len; break;
        }
        const bool arp = s.pattern >= 5;
        for (size_t h = 0; h < hits.size(); ++h, ++hitCounter)
        {
            if (arp)
            {
                const int n = (int) notes.size(); int idx = (int) h % n;
                if (s.pattern == 6) idx = n - 1 - idx;
                if (s.pattern == 7) { const int cyc = std::max (1, 2 * n - 2); const int k = (int) h % cyc; idx = k < n ? k : cyc - k; }
                const double t = start + hits[h] + hrand (hitCounter, 1) * s.humanMs * msToBeats;
                const int vel = juce::jlimit (1, 127, juce::roundToInt (s.velocity + hrand (hitCounter, 2) * s.humanVel));
                ev.push_back ({ std::max (0.0, t), hitLen * s.gate, notes[(size_t) idx], vel, s.chordCh });
                continue;
            }
            const bool down = s.strumDir == 1 || (s.strumDir == 2 && (h % 2 == 1));
            for (size_t ni = 0; ni < notes.size(); ++ni)
            {
                const size_t order = down ? notes.size() - 1 - ni : ni;
                const double strum = notes.size() > 1 ? s.strumMs * msToBeats * (double) order / (double) (notes.size() - 1) : 0.0;
                const double t = start + hits[h] + strum + hrand (hitCounter, (int) ni + 10) * s.humanMs * msToBeats;
                const int vel = juce::jlimit (1, 127, juce::roundToInt (s.velocity + hrand (hitCounter, (int) ni + 40) * s.humanVel - (double) order * 2));
                ev.push_back ({ std::max (0.0, t), std::max (0.05, hitLen * s.gate - strum), notes[ni], vel, s.chordCh });
            }
        }
        if (s.bass && si < bassNotes.size() && bassNotes[si] >= 0)
            ev.push_back ({ start, len * std::max (0.5f, s.gate), bassNotes[si], juce::jlimit (1, 127, s.velocity + 6), s.bassCh });
        start += len;
    }
    std::sort (ev.begin(), ev.end(), [] (const NoteEvent& a, const NoteEvent& b) { return a.beat < b.beat; });
    return ev;
}

// =====================================================================  processor
static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto cho = [&] (const char* id, const char* name, const StringArray& c, int def) { l.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, c, def)); };
    auto in = [&] (const char* id, const char* name, int lo, int hi, int def, std::function<String (int, int)> f = {})
    { auto a = AudioParameterIntAttributes(); if (f) a = a.withStringFromValueFunction (f); l.add (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def, a)); };
    auto fl = [&] (const char* id, const char* name, NormalisableRange<float> r, float def, std::function<String (float, int)> f)
    { l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, AudioParameterFloatAttributes().withStringFromValueFunction (f))); };
    auto bo = [&] (const char* id, const char* name, bool def) { l.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
    cho ("key", "Key", keyNames(), 9);              // A
    cho ("scale", "Scale", scaleNames(), 1);        // minor
    cho ("extension", "Chord Colour", extensionNames(), 2);
    cho ("genStyle", "Generator Style", styleNames(), 0);
    cho ("register", "Register", StringArray { "C2", "C3", "C4", "C5" }, 1);
    in ("inversion", "Inversion", 0, 3, 0, [] (int v, int) { return v == 0 ? String ("Root") : String (v) + (v == 1 ? "st" : v == 2 ? "nd" : "rd"); });
    cho ("voicing", "Voicing", voicingNames(), 0);
    bo ("voiceLead", "Voice Leading", true);
    bo ("bass", "Bass Note", true);
    cho ("bassReg", "Bass Register", StringArray { "C1", "C2" }, 1);
    cho ("pattern", "Pattern", patternNames(), 0);
    fl ("gate", "Gate", { 10.0f, 100.0f, 0.1f }, 92.0f, [] (float v, int) { return String (roundToInt (v)) + "%"; });
    fl ("strum", "Strum", { 0.0f, 150.0f, 0.1f }, 12.0f, [] (float v, int) { return String (roundToInt (v)) + " ms"; });
    cho ("strumDir", "Strum Direction", StringArray { "Up", "Down", "Alternate" }, 0);
    fl ("humanTime", "Humanize Timing", { 0.0f, 30.0f, 0.1f }, 6.0f, [] (float v, int) { return String (v, 1) + " ms"; });
    fl ("humanVel", "Humanize Velocity", { 0.0f, 40.0f, 0.1f }, 8.0f, [] (float v, int) { return String (roundToInt (v)); });
    in ("velocity", "Velocity", 1, 127, 92);
    in ("midiCh", "Chord MIDI Channel", 1, 16, 1);
    in ("bassCh", "Bass MIDI Channel", 1, 16, 2);
    bo ("preview", "Built-in Sound", true);
    fl ("previewVol", "Sound Level", { -40.0f, 6.0f, 0.1f }, -8.0f, [] (float v, int) { return String (v, 1) + " dB"; });
    cho ("trigger", "Keyboard", StringArray { "Pass-through", "One-Finger Chords" }, 0);
    fl ("tempo", "Internal Tempo", { 60.0f, 180.0f, 0.1f }, 122.0f, [] (float v, int) { return String (v, 1) + " BPM"; });
    return l;
}

VegaChordsProcessor::VegaChordsProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VegaChords", makeLayout())
{
    for (auto& h : inputHist) h.store (0);
    generate (0);
    startTimerHz (20);
}
VegaChordsProcessor::~VegaChordsProcessor() { stopTimer(); }

void VegaChordsProcessor::setSlot (int i, const Chord& c, int beats)
{
    if (! juce::isPositiveAndBelow (i, kMaxSlots)) return;
    slots[(size_t) i].quality.store (c.quality); slots[(size_t) i].root.store (c.root); slots[(size_t) i].label = c.label;
    if (beats > 0) slots[(size_t) i].beats.store (beats);
    touch();
}
Chord VegaChordsProcessor::getSlot (int i) const
{
    const auto& s = slots[(size_t) i]; return { s.root.load(), s.quality.load(), s.label };
}

VoicingSettings VegaChordsProcessor::voicing() const
{
    static const int regs[] { 36, 48, 60, 72 };
    return { regs[juce::jlimit (0, 3, juce::roundToInt (p ("register")))], juce::roundToInt (p ("inversion")), juce::roundToInt (p ("voicing")), p ("voiceLead") > 0.5f };
}
RenderSettings VegaChordsProcessor::renderSettings (double bpm) const
{
    RenderSettings s;
    s.pattern = juce::roundToInt (p ("pattern")); s.gate = p ("gate") * 0.01f; s.strumMs = p ("strum"); s.strumDir = juce::roundToInt (p ("strumDir"));
    s.humanMs = p ("humanTime"); s.humanVel = p ("humanVel"); s.velocity = juce::roundToInt (p ("velocity"));
    s.chordCh = juce::roundToInt (p ("midiCh")); s.bassCh = juce::roundToInt (p ("bassCh")); s.bass = p ("bass") > 0.5f; s.bpm = bpm;
    return s;
}
std::vector<Chord> VegaChordsProcessor::palette() const
{
    auto d = diatonic (key(), scale(), juce::roundToInt (p ("extension")));
    auto b = borrowed (key(), scale(), juce::roundToInt (p ("extension")));
    d.insert (d.end(), b.begin(), b.end());
    return d;
}

void VegaChordsProcessor::generate (int style)
{
    const auto& ts = templates (style);
    const auto& t = ts[(size_t) rng.nextInt ((int) ts.size())];
    // Minor‑flavoured styles want a minor key, the others a major one (keep the tonic)
    const bool minorStyle = style == 1 || style == 5;
    if (minorStyle != scaleIsMinor (scale()))
        if (auto* sp = apvts.getParameter ("scale")) sp->setValueNotifyingHost (sp->convertTo0to1 (minorStyle ? 1.0f : 0.0f));
    for (int i = 0; i < kMaxSlots; ++i) slots[(size_t) i].root.store (-1);
    for (size_t i = 0; i < t.size(); ++i) setSlot ((int) i, parseNumeral (t[i], key()), 4);
    numSlots.store ((int) t.size());
    touch();
}

void VegaChordsProcessor::randomizeColour()
{
    // swap one chord for a related colour chord and refresh extensions
    const int n = numSlots.load(); if (n <= 0) return;
    const int i = rng.nextInt (n);
    const auto pal = palette();
    auto c = getSlot (i);
    if (rng.nextBool()) c = pal[(size_t) (7 + rng.nextInt (7))];
    else { const int qs[] { Maj7, Min9, Dom9, Add9, Min11, Dom13, Sus4, Maj9, Min7 }; const bool minor = isMinorish (c.quality); c.quality = qs[rng.nextInt (9)]; if (minor != isMinorish (c.quality) && c.quality != Sus4) c.quality = minor ? Min9 : Maj9; c.label = {}; }
    setSlot (i, c);
}

void VegaChordsProcessor::audition (const Chord& c) { auditionQuality.store (c.quality); auditionRoot.store (c.root); }

void VegaChordsProcessor::prepareToPlay (double sr, int)
{
    fs = sr; pending.clear(); for (auto& a : activeCount) a.fill (0);
    builtVersion = -1; lastPpq = -1;
    for (auto& v : ep) v.on = false;
}

void VegaChordsProcessor::rebuildEvents (double bpm)
{
    const int n = juce::jlimit (0, kMaxSlots, numSlots.load());
    std::vector<std::vector<int>> voiced; std::vector<int> bass, beats;
    const auto vs = voicing();
    static const int bassRegs[] { 24, 36 };
    const int br = bassRegs[juce::jlimit (0, 1, juce::roundToInt (p ("bassReg")))];
    std::vector<int> prev;
    slotStartBeat.clear(); int acc = 0;
    for (int i = 0; i < n; ++i)
    {
        const auto c = getSlotRT (i);
        auto v = voice (c, vs, prev); if (! v.empty()) prev = v;
        voiced.push_back (v); bass.push_back (c.valid() ? bassNote (c, br) : -1);
        beats.push_back (juce::jlimit (1, 16, slots[(size_t) i].beats.load()));
        slotStartBeat.push_back (acc); acc += beats.back();
    }
    loopBeats = std::max (1, acc);
    loopEvents = renderProgression (voiced, bass, beats, renderSettings (bpm));
    builtBpm = bpm;
}

void VegaChordsProcessor::emitOn (juce::MidiBuffer& out, int offset, int note, int vel, int ch)
{
    if (note < 0 || note > 127) return;
    auto& cnt = activeCount[(size_t) (ch - 1)][(size_t) note];
    if (cnt > 0) out.addEvent (juce::MidiMessage::noteOff (ch, note), offset);   // retrigger cleanly
    out.addEvent (juce::MidiMessage::noteOn (ch, note, (juce::uint8) vel), offset);
    cnt = 1;
}
void VegaChordsProcessor::emitOff (juce::MidiBuffer& out, int offset, int note, int ch)
{
    if (note < 0 || note > 127) return;
    auto& cnt = activeCount[(size_t) (ch - 1)][(size_t) note];
    if (cnt > 0) { out.addEvent (juce::MidiMessage::noteOff (ch, note), offset); cnt = 0; }
}
void VegaChordsProcessor::allOff (juce::MidiBuffer& out, int offset)
{
    for (int ch = 1; ch <= 16; ++ch) for (int n = 0; n < 128; ++n) emitOff (out, offset, n, ch);
    pending.clear();
}

void VegaChordsProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    buffer.clear();
    const int n = buffer.getNumSamples();
    juce::MidiBuffer out;

    double bpm = p ("tempo"), ppq = 0; bool hostPlaying = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto q = pos->getPpqPosition()) ppq = *q;
            hostPlaying = pos->getIsPlaying();
        }
    const bool internal = ! hostPlaying && internalPlay.load();
    if (internal) { if (! wasInternal) internalPpq = 0; ppq = internalPpq; bpm = p ("tempo"); internalPpq += n * bpm / 60.0 / fs; }
    const bool running = hostPlaying || internal;

    if (builtVersion != editVersion.load() || std::abs (builtBpm - bpm) > 0.5) { builtVersion = editVersion.load(); rebuildEvents (bpm); }

    // ---- MIDI input: one‑finger chords, chord detection, key histogram, pass‑through
    const bool oneFinger = juce::roundToInt (p ("trigger")) == 1;
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage(); const int off = meta.samplePosition;
        if (m.isNoteOn())
        {
            heldInput.push_back (m.getNoteNumber());
            auto& h = inputHist[(size_t) (m.getNoteNumber() % 12)]; h.store (h.load() + 1.0f);
            if (oneFinger)
            {
                // white keys -> diatonic chords I..VII, black keys -> colour chords
                static const int whiteIdx[12] { 0, -1, 1, -2, 2, 3, -3, 4, -4, 5, -5, 6 };
                const int w = whiteIdx[m.getNoteNumber() % 12];
                const auto pal = palette();
                const Chord c = w >= 0 ? pal[(size_t) w] : pal[(size_t) (6 - w)];
                auto v = voice (c, voicing(), lastTriggerVoicing); lastTriggerVoicing = v;
                const auto rs = renderSettings (bpm);
                std::vector<std::pair<int, int>> played;
                for (size_t i = 0; i < v.size(); ++i)
                {
                    const int strum = (int) (rs.strumMs * 0.001 * fs * (double) i / std::max<size_t> (1, v.size() - 1));
                    const int vel = juce::jlimit (1, 127, juce::roundToInt (m.getVelocity() - (int) i * 2));
                    if (off + strum < n) emitOn (out, off + strum, v[i], vel, rs.chordCh); else pending.push_back ({ sampleClock + off + strum, v[i], rs.chordCh, true, vel });
                    played.push_back ({ v[i], rs.chordCh });
                }
                if (rs.bass) { const int b = bassNote (c, p ("bassReg") > 0.5f ? 36 : 24); emitOn (out, off, b, m.getVelocity(), rs.bassCh); played.push_back ({ b, rs.bassCh }); }
                triggered[m.getNoteNumber()] = played;
                continue;
            }
        }
        else if (m.isNoteOff())
        {
            heldInput.erase (std::remove (heldInput.begin(), heldInput.end(), m.getNoteNumber()), heldInput.end());
            auto it = triggered.find (m.getNoteNumber());
            if (it != triggered.end()) { for (auto [note, ch] : it->second) emitOff (out, off, note, ch); triggered.erase (it); continue; }
        }
        out.addEvent (m, off);       // pass through anything we didn't consume
    }
    { const auto c = detectChord (heldInput); detectedRoot.store (c.root); detectedQuality.store (c.quality); }

    // ---- audition request from the UI
    const int ar = auditionRoot.exchange (-1);
    if (ar >= 0)
    {
        const Chord c { ar, auditionQuality.load(), {} };
        auto v = voice (c, voicing(), lastAuditionVoicing); lastAuditionVoicing = v;
        const auto rs = renderSettings (bpm);
        const juce::int64 len = (juce::int64) (0.9 * fs);
        for (size_t i = 0; i < v.size(); ++i)
        {
            const juce::int64 at = sampleClock + (juce::int64) (rs.strumMs * 0.001 * fs * (double) i / std::max<size_t> (1, v.size() - 1));
            pending.push_back ({ at, v[i], rs.chordCh, true, rs.velocity });
            pending.push_back ({ at + len, v[i], rs.chordCh, false, 0 });
        }
        if (rs.bass) { const int b = bassNote (c, p ("bassReg") > 0.5f ? 36 : 24); pending.push_back ({ sampleClock, b, rs.bassCh, true, rs.velocity }); pending.push_back ({ sampleClock + len, b, rs.bassCh, false, 0 }); }
    }

    // ---- transport‑synced progression
    if (running)
    {
        if (! (wasPlaying || wasInternal) || std::abs (ppq - lastPpq) > 0.6)   // started or jumped
            allOff (out, 0);
        const double spb = fs * 60.0 / bpm;
        const double blockBeats = n / spb;
        const double L = loopBeats;
        double pos = std::fmod (ppq, L); if (pos < 0) pos += L;
        auto scan = [&] (double from, double to, double sampleBase)
        {
            for (const auto& e : loopEvents)
            {
                if (e.beat < from || e.beat >= to) continue;
                const juce::int64 at = sampleClock + (juce::int64) std::llround (sampleBase + (e.beat - from) * spb);
                const juce::int64 offAt = at + (juce::int64) std::llround (e.len * spb);
                pending.push_back ({ at, e.note, e.ch, true, e.vel });
                pending.push_back ({ offAt, e.note, e.ch, false, 0 });
            }
        };
        if (pos + blockBeats <= L) scan (pos, pos + blockBeats, 0.0);
        else { scan (pos, L, 0.0); scan (0.0, pos + blockBeats - L, (L - pos) * spb); }
        int slot = 0; for (int i = 0; i < (int) slotStartBeat.size(); ++i) if (pos >= slotStartBeat[(size_t) i]) slot = i;
        playingSlot.store (slot); uiPpq.store (pos);
        lastPpq = ppq + blockBeats;
    }
    else
    {
        if (wasPlaying || wasInternal) { allOff (out, 0); playingSlot.store (-1); }
        lastPpq = -1;
    }
    wasPlaying = hostPlaying; wasInternal = internal;

    // ---- flush pending events that fall in this block (offs before ons at the same time)
    std::stable_sort (pending.begin(), pending.end(), [] (const Pending& a, const Pending& b) { return a.at != b.at ? a.at < b.at : (! a.on && b.on); });
    size_t k = 0;
    for (; k < pending.size() && pending[k].at < sampleClock + n; ++k)
    {
        const int off = (int) juce::jlimit<juce::int64> (0, n - 1, pending[k].at - sampleClock);
        if (pending[k].on) emitOn (out, off, pending[k].note, pending[k].vel, pending[k].ch); else emitOff (out, off, pending[k].note, pending[k].ch);
    }
    pending.erase (pending.begin(), pending.begin() + (long) k);

    // ---- built‑in electric piano + on‑screen keyboard
    if (p ("preview") > 0.5f) renderPreview (buffer, out);
    for (const auto meta : out)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn()) outKeyboard.noteOn (1, m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff()) outKeyboard.noteOff (1, m.getNoteNumber(), 0.0f);
    }
    midi.swapWith (out);
    sampleClock += n;
}

void VegaChordsProcessor::renderPreview (juce::AudioBuffer<float>& buf, const juce::MidiBuffer& events)
{
    const float gain = juce::Decibels::decibelsToGain (p ("previewVol"));
    float* L = buf.getWritePointer (0); float* R = buf.getWritePointer (1);
    auto it = events.begin();
    const float decay = std::exp (-1.0f / (1.6f * (float) fs)), modDecay = std::exp (-1.0f / (0.25f * (float) fs)), rel = std::exp (-1.0f / (0.12f * (float) fs));
    for (int i = 0; i < buf.getNumSamples(); ++i)
    {
        while (it != events.end() && (*it).samplePosition <= i)
        {
            const auto m = (*it).getMessage();
            if (m.isNoteOn())
            {
                EpVoice* v = nullptr;
                for (auto& e : ep) if (! e.on) { v = &e; break; }
                if (! v) { v = &ep[0]; for (auto& e : ep) if (e.age < v->age) v = &e; }
                v->on = true; v->releasing = false; v->note = m.getNoteNumber(); v->vel = m.getFloatVelocity(); v->env = 1; v->modEnv = 1; v->phC = v->phM = 0;
                v->inc = 440.0f * std::pow (2.0f, (v->note - 69) / 12.0f) / (float) fs; v->age = ++epAge;
            }
            else if (m.isNoteOff()) for (auto& e : ep) if (e.on && e.note == m.getNoteNumber() && ! e.releasing) e.releasing = true;
            ++it;
        }
        float s = 0;
        for (auto& v : ep)
        {
            if (! v.on) continue;
            v.env *= v.releasing ? rel : decay; v.modEnv *= modDecay;
            const float mod = std::sin (juce::MathConstants<float>::twoPi * v.phM) * (0.6f + 2.4f * v.modEnv * v.vel);
            s += std::sin (juce::MathConstants<float>::twoPi * v.phC + mod) * v.env * v.vel;
            v.phC += v.inc; if (v.phC >= 1) v.phC -= 1;
            v.phM += v.inc; if (v.phM >= 1) v.phM -= 1;
            if (v.env < 1.0e-4f) v.on = false;
        }
        s = std::tanh (s * 0.25f) * gain;
        L[i] += s; R[i] += s;
    }
}

void VegaChordsProcessor::timerCallback() {}

// =====================================================================  MIDI export + presets
juce::File VegaChordsProcessor::exportMidi (const juce::File& dest) const
{
    const double bpm = 120.0;
    const int n = numSlots.load();
    std::vector<std::vector<int>> voiced; std::vector<int> bass, beats; std::vector<int> prev;
    const auto vs = voicing(); const int br = p ("bassReg") > 0.5f ? 36 : 24;
    juce::String names;
    for (int i = 0; i < n; ++i)
    {
        const auto c = getSlot (i); auto v = voice (c, vs, prev); if (! v.empty()) prev = v;
        voiced.push_back (v); bass.push_back (c.valid() ? bassNote (c, br) : -1); beats.push_back (slots[(size_t) i].beats.load());
        if (c.valid()) names << (names.isEmpty() ? "" : " ") << c.name (key(), scale());
    }
    const auto rs = renderSettings (bpm);
    auto ev = renderProgression (voiced, bass, beats, rs);
    juce::MidiMessageSequence chords, bassSeq;
    constexpr int tpq = 960;
    for (auto& e : ev)
    {
        auto& seq = (rs.bass && e.ch == rs.bassCh && rs.bassCh != rs.chordCh) ? bassSeq : chords;
        seq.addEvent (juce::MidiMessage::noteOn (e.ch, e.note, (juce::uint8) e.vel), e.beat * tpq);
        seq.addEvent (juce::MidiMessage::noteOff (e.ch, e.note), (e.beat + e.len) * tpq);
    }
    chords.updateMatchedPairs(); bassSeq.updateMatchedPairs();
    juce::MidiFile mf; mf.setTicksPerQuarterNote (tpq);
    juce::MidiMessageSequence meta; meta.addEvent (juce::MidiMessage::tempoMetaEvent ((int) (60000000.0 / bpm)), 0);
    meta.addEvent (juce::MidiMessage::textMetaEvent (3, "Vega Chords"), 0);
    mf.addTrack (meta); mf.addTrack (chords); if (bassSeq.getNumEvents() > 0) mf.addTrack (bassSeq);
    auto file = dest != juce::File() ? dest
        : juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Vega Chords - " + juce::File::createLegalFileName (names.substring (0, 60)) + ".mid");
    file.deleteFile();
    if (auto os = file.createOutputStream()) mf.writeTo (*os);
    return file;
}

juce::File VegaChordsProcessor::presetFolder()
{
    auto d = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Vega Productions").getChildFile ("Chords Presets");
    d.createDirectory(); return d;
}

void VegaChordsProcessor::getStateInformation (juce::MemoryBlock& d)
{
    auto st = apvts.copyState();
    juce::ValueTree prog ("Progression"); prog.setProperty ("count", numSlots.load(), nullptr);
    for (int i = 0; i < kMaxSlots; ++i)
    {
        juce::ValueTree s ("Slot");
        s.setProperty ("root", slots[(size_t) i].root.load(), nullptr); s.setProperty ("q", slots[(size_t) i].quality.load(), nullptr);
        s.setProperty ("beats", slots[(size_t) i].beats.load(), nullptr); s.setProperty ("label", slots[(size_t) i].label, nullptr);
        prog.appendChild (s, nullptr);
    }
    st.removeChild (st.getChildWithName ("Progression"), nullptr);
    st.appendChild (prog, nullptr);
    st.setProperty ("uiScale", (double) uiScale.load(), nullptr);
    if (auto x = st.createXml()) copyXmlToBinary (*x, d);
}
void VegaChordsProcessor::setStateInformation (const void* data, int size)
{
    if (auto x = getXmlFromBinary (data, size))
        if (x->hasTagName (apvts.state.getType()))
        {
            auto st = juce::ValueTree::fromXml (*x);
            if (auto prog = st.getChildWithName ("Progression"); prog.isValid())
            {
                numSlots.store (juce::jlimit (0, kMaxSlots, (int) prog.getProperty ("count", 4)));
                for (int i = 0; i < kMaxSlots && i < prog.getNumChildren(); ++i)
                {
                    auto s = prog.getChild (i);
                    slots[(size_t) i].root.store (s.getProperty ("root", -1)); slots[(size_t) i].quality.store (s.getProperty ("q", 0));
                    slots[(size_t) i].beats.store (s.getProperty ("beats", 4)); slots[(size_t) i].label = s.getProperty ("label").toString();
                }
                touch();
            }
            uiScale.store ((float) (double) st.getProperty ("uiScale", 0.9));
            apvts.replaceState (st);
        }
}
bool VegaChordsProcessor::savePreset (const juce::File& f)
{
    juce::MemoryBlock mb; getStateInformation (mb);
    if (auto x = getXmlFromBinary (mb.getData(), (int) mb.getSize())) return x->writeTo (f);
    return false;
}
bool VegaChordsProcessor::loadPreset (const juce::File& f)
{
    if (auto x = juce::XmlDocument::parse (f)) { juce::MemoryBlock mb; copyXmlToBinary (*x, mb); setStateInformation (mb.getData(), (int) mb.getSize()); return true; }
    return false;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VegaChordsProcessor(); }
