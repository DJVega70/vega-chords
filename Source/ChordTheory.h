#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include <array>
#include <algorithm>
#include <random>

// =====================================================================
//  Vega Productions Chords — music theory engine
// =====================================================================
namespace vch
{
enum Quality { Maj = 0, Min, Dim, Aug, Sus2, Sus4, Maj7, Min7, Dom7, HalfDim7, Dim7, MinMaj7, Maj9, Min9, Dom9, Add9, MinAdd9, Min11, Dom13, Maj6, Min6, Dom7Sus4, NumQualities };

inline const std::vector<int>& intervals (int q)
{
    static const std::vector<int> t[NumQualities] {
        { 0, 4, 7 }, { 0, 3, 7 }, { 0, 3, 6 }, { 0, 4, 8 }, { 0, 2, 7 }, { 0, 5, 7 },
        { 0, 4, 7, 11 }, { 0, 3, 7, 10 }, { 0, 4, 7, 10 }, { 0, 3, 6, 10 }, { 0, 3, 6, 9 }, { 0, 3, 7, 11 },
        { 0, 4, 7, 11, 14 }, { 0, 3, 7, 10, 14 }, { 0, 4, 7, 10, 14 }, { 0, 4, 7, 14 }, { 0, 3, 7, 14 }, { 0, 3, 7, 10, 14, 17 },
        { 0, 4, 7, 10, 14, 21 }, { 0, 4, 7, 9 }, { 0, 3, 7, 9 }, { 0, 5, 7, 10 } };
    return t[juce::jlimit (0, NumQualities - 1, q)];
}
inline const char* suffix (int q)
{
    static const char* s[NumQualities] { "", "m", "dim", "aug", "sus2", "sus4", "maj7", "m7", "7", "m7b5", "dim7", "m(maj7)",
                                         "maj9", "m9", "9", "add9", "m(add9)", "m11", "13", "6", "m6", "7sus4" };
    return s[juce::jlimit (0, NumQualities - 1, q)];
}
inline bool isMinorish (int q) { return q == Min || q == Min7 || q == MinMaj7 || q == Min9 || q == MinAdd9 || q == Min11 || q == Min6; }
inline bool isDim (int q) { return q == Dim || q == Dim7 || q == HalfDim7; }

inline const juce::StringArray& keyNames() { static juce::StringArray a { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; return a; }
inline const juce::StringArray& scaleNames() { static juce::StringArray a { "Major", "Minor", "Dorian", "Mixolydian", "Lydian", "Phrygian", "Harmonic Minor", "Melodic Minor" }; return a; }
inline const std::array<int, 7>& scaleSteps (int s)
{
    static const std::array<int, 7> t[8] { { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 3, 5, 7, 9, 10 }, { 0, 2, 4, 5, 7, 9, 10 },
                                           { 0, 2, 4, 6, 7, 9, 11 }, { 0, 1, 3, 5, 7, 8, 10 }, { 0, 2, 3, 5, 7, 8, 11 }, { 0, 2, 3, 5, 7, 9, 11 } };
    return t[juce::jlimit (0, 7, s)];
}
inline bool scaleIsMinor (int s) { return s == 1 || s == 2 || s == 5 || s == 6 || s == 7; }

inline juce::String pcName (int pc, int key, int scale)
{
    static const char* sharps[12] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* flats[12]  { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    // flat keys: major F Bb Eb Ab Db Gb ; minor D G C F Bb Eb
    const int majorEquivalent = scaleIsMinor (scale) ? (key + 3) % 12 : key;
    const bool useFlats = majorEquivalent == 5 || majorEquivalent == 10 || majorEquivalent == 3 || majorEquivalent == 8 || majorEquivalent == 1 || majorEquivalent == 6;
    return (useFlats ? flats : sharps)[((pc % 12) + 12) % 12];
}

struct Chord
{
    int root = -1, quality = Maj;      // root = pitch class, -1 = empty
    juce::String label;                // optional Roman numeral override (e.g. "V/V")
    bool valid() const { return root >= 0; }
    juce::String name (int key, int scale) const
    {
        if (! valid()) return "-";
        static const char* flats[12] { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
        const int semis = ((root - key) % 12 + 12) % 12;
        // chromatic "flat side" chords (bII, bIII, bVI, bVII) are spelled with flats; diatonic ones follow the key
        const bool flatDegree = semis == 1 || semis == 3 || semis == 8 || semis == 10;
        const bool diatonicHere = [&] { for (int st : scaleSteps (scale)) if (st == semis) return true; return false; }();
        return juce::String (flatDegree && ! diatonicHere ? flats[root] : pcName (root, key, scale).toRawUTF8()) + suffix (quality);
    }
};

// ---------------------------------------------------------------- Roman numerals
inline juce::String numeral (const Chord& c, int key)
{
    if (! c.valid()) return "";
    if (c.label.isNotEmpty()) return c.label;
    static const char* up[12] { "I", "bII", "II", "bIII", "III", "IV", "#IV", "V", "bVI", "VI", "bVII", "VII" };
    juce::String n (up[((c.root - key) % 12 + 12) % 12]);
    if (isMinorish (c.quality) || isDim (c.quality)) n = n.toLowerCase();
    switch (c.quality)
    {
        case Dim: n << juce::String::fromUTF8 ("\xc2\xb0"); break;
        case Dim7: n << juce::String::fromUTF8 ("\xc2\xb0" "7"); break;
        case HalfDim7: n << juce::String::fromUTF8 ("\xc3\xb8" "7"); break;
        case Aug: n << "+"; break;
        case Maj7: n << "maj7"; break; case Maj9: n << "maj9"; break;
        case Min7: case Dom7: n << "7"; break;
        case Min9: case Dom9: n << "9"; break;
        case Min11: n << "11"; break; case Dom13: n << "13"; break;
        case Sus2: n << "sus2"; break; case Sus4: n << "sus4"; break; case Dom7Sus4: n << "7sus4"; break;
        case Add9: case MinAdd9: n << "add9"; break; case Maj6: case Min6: n << "6"; break;
        case MinMaj7: n << "(maj7)"; break;
        default: break;
    }
    return n;
}

/** Parse "ii9", "bVImaj7", "V13", "vii°", "iv7", "bVII", "Imaj9" relative to key -> chord. */
inline Chord parseNumeral (juce::String s, int key)
{
    Chord c; int acc = 0;
    while (s.startsWithChar ('b') || s.startsWithChar ('#')) { acc += s[0] == 'b' ? -1 : 1; s = s.substring (1); }
    static const char* romans[7] { "VII", "VI", "IV", "V", "III", "II", "I" };   // longest first
    static const int degSemis[7] { 11, 9, 5, 7, 4, 2, 0 };
    int semis = -1; bool lower = false;
    for (int i = 0; i < 7; ++i)
    {
        const juce::String r (romans[i]);
        if (s.startsWith (r)) { semis = degSemis[i]; s = s.substring (r.length()); break; }
        if (s.startsWith (r.toLowerCase())) { semis = degSemis[i]; lower = true; s = s.substring (r.length()); break; }
    }
    if (semis < 0) return c;
    c.root = ((key + semis + acc) % 12 + 12) % 12;
    const juce::String suf = s;
    if (suf.contains (juce::String::fromUTF8 ("\xc3\xb8")) || suf == "m7b5") c.quality = HalfDim7;
    else if (suf.contains (juce::String::fromUTF8 ("\xc2\xb0"))) c.quality = suf.contains ("7") ? Dim7 : Dim;
    else if (suf == "maj7") c.quality = lower ? MinMaj7 : Maj7;
    else if (suf == "maj9") c.quality = Maj9;
    else if (suf == "7") c.quality = lower ? Min7 : Dom7;
    else if (suf == "9") c.quality = lower ? Min9 : Dom9;
    else if (suf == "11") c.quality = Min11;
    else if (suf == "13") c.quality = Dom13;
    else if (suf == "sus2") c.quality = Sus2;
    else if (suf == "sus4") c.quality = Sus4;
    else if (suf == "7sus4") c.quality = Dom7Sus4;
    else if (suf == "add9") c.quality = lower ? MinAdd9 : Add9;
    else if (suf == "6") c.quality = lower ? Min6 : Maj6;
    else if (suf == "+") c.quality = Aug;
    else c.quality = lower ? Min : Maj;
    return c;
}

// ---------------------------------------------------------------- palette
enum Extension { Triads = 0, Sevenths, Ninths, NeoSoul };
inline const juce::StringArray& extensionNames() { static juce::StringArray a { "Triads", "7ths", "9ths", "Neo-Soul" }; return a; }

inline int extend (int q, int level, bool dominantFunction)
{
    if (level <= Triads) return q;
    auto seventh = [&] { switch (q) { case Maj: return dominantFunction ? (int) Dom7 : (int) Maj7; case Min: return (int) Min7; case Dim: return (int) HalfDim7; default: return q; } };
    int s = seventh();
    if (level == Sevenths) return s;
    switch (s)
    {
        case Maj7: return Maj9;
        case Min7: return level == NeoSoul ? (int) Min11 : (int) Min9;
        case Dom7: return level == NeoSoul ? (int) Dom13 : (int) Dom9;
        default: return s;
    }
}

/** 7 diatonic chords for the key/scale at the requested extension level. */
inline std::vector<Chord> diatonic (int key, int scale, int level)
{
    const auto& st = scaleSteps (scale);
    std::vector<Chord> out;
    for (int d = 0; d < 7; ++d)
    {
        auto note = [&] (int k) { const int idx = d + k; return st[(size_t) (idx % 7)] + 12 * (idx / 7); };
        const int r = note (0), third = note (2) - r, fifth = note (4) - r, sev = note (6) - r, ninth = note (8) - r;
        int q = Maj;
        if (third == 4 && fifth == 7) q = Maj; else if (third == 3 && fifth == 7) q = Min;
        else if (third == 3 && fifth == 6) q = Dim; else if (third == 4 && fifth == 8) q = Aug;
        if (level >= Sevenths)
        {
            if (q == Maj) q = sev == 11 ? Maj7 : Dom7;
            else if (q == Min) q = sev == 10 ? Min7 : MinMaj7;
            else if (q == Dim) q = sev == 10 ? HalfDim7 : Dim7;
            else if (q == Aug) q = Maj7;
            if (level >= Ninths && ninth == 14)
            {
                if (q == Maj7) q = Maj9;
                else if (q == Min7) q = level == NeoSoul ? Min11 : Min9;
                else if (q == Dom7) q = level == NeoSoul ? Dom13 : Dom9;
            }
        }
        out.push_back ({ (key + r) % 12, q, {} });
    }
    return out;
}

/** 7 "colour" chords: modal interchange, secondary dominants, Neapolitan. */
inline std::vector<Chord> borrowed (int key, int scale, int level)
{
    std::vector<Chord> out;
    auto add = [&] (int semis, int q, const char* label = "") { out.push_back ({ (key + semis) % 12, q, juce::String::fromUTF8 (label) }); };
    const bool ext = level >= Sevenths;
    if (! scaleIsMinor (scale))
    {
        add (3, ext ? Maj7 : Maj);            // bIII
        add (5, ext ? Min7 : Min);            // iv
        add (8, ext ? Maj7 : Maj);            // bVI
        add (10, ext ? Dom7 : Maj);           // bVII (backdoor)
        add (2, Dom7, "V/V");                 // II7
        add (4, Dom7, "V/vi");                // III7
        add (1, ext ? Maj7 : Maj);            // bII Neapolitan
    }
    else
    {
        add (5, ext ? (level >= Ninths ? Dom9 : Dom7) : Maj);   // IV (dorian)
        add (7, ext ? Dom7 : Maj);            // V (harmonic)
        add (0, ext ? Maj7 : Maj);            // I (picardy)
        add (1, ext ? Maj7 : Maj);            // bII
        add (11, Dim7);                       // vii°7
        add (0, Dom7, "V/iv");                // I7
        add (2, ext ? Min7 : Min);            // ii (from major)
    }
    return out;
}

// ---------------------------------------------------------------- voicing
enum VoicingStyle { Close = 0, Drop2, Drop3, Spread, Rootless, Shell };
inline const juce::StringArray& voicingNames() { static juce::StringArray a { "Close", "Drop 2", "Drop 3", "Spread", "Rootless", "Shell" }; return a; }

struct VoicingSettings { int registerNote = 60, inversion = 0, style = Close; bool voiceLeading = true; };

inline std::vector<int> voiceRaw (const Chord& c, int registerNote, int inversion, int style)
{
    std::vector<int> iv = intervals (c.quality);
    if (style == Rootless && iv.size() >= 4) iv.erase (iv.begin());
    if (style == Shell)
    {   // root, 3rd (or sus), 7th (or 5th)
        std::vector<int> s { 0, iv.size() > 1 ? iv[1] : 4 };
        s.push_back (iv.size() > 3 ? iv[3] : 7);
        iv = s;
    }
    int r0 = registerNote + ((c.root - registerNote % 12) % 12 + 12) % 12;
    if (r0 > registerNote + 5) r0 -= 12;
    std::vector<int> n; for (int i : iv) n.push_back (r0 + i);
    std::sort (n.begin(), n.end());
    for (int k = 0; k < inversion % (int) n.size(); ++k) { n.front() += 12; std::sort (n.begin(), n.end()); }
    if (style == Drop2 && n.size() >= 4) { n[n.size() - 2] -= 12; }
    if (style == Drop3 && n.size() >= 4) { n[n.size() - 3] -= 12; }
    if (style == Spread && n.size() >= 3) for (size_t i = 1; i < n.size(); i += 2) n[i] += 12;
    if (style == Shell && n.size() == 3) { n[0] -= 12; }
    std::sort (n.begin(), n.end());
    return n;
}

/** Voice a chord; with voice leading, the inversion/octave closest to the previous voicing wins. */
inline std::vector<int> voice (const Chord& c, const VoicingSettings& s, const std::vector<int>& prev)
{
    if (! c.valid()) return {};
    if (! s.voiceLeading || prev.empty()) return voiceRaw (c, s.registerNote, s.inversion, s.style);
    const int nInv = (int) intervals (c.quality).size();
    double prevMean = 0; for (int p : prev) prevMean += p; prevMean /= (double) prev.size();
    std::vector<int> best; double bestCost = 1e18;
    for (int inv = 0; inv < nInv; ++inv)
        for (int oct : { -12, 0, 12 })
        {
            auto cand = voiceRaw (c, s.registerNote + oct, inv, s.style);
            double cost = 0;
            for (int n : cand) { int d = 999; for (int p : prev) d = std::min (d, std::abs (n - p)); cost += d; }
            double mean = 0; for (int n : cand) mean += n; mean /= (double) cand.size();
            cost += 0.35 * std::abs (mean - (s.registerNote + 6));          // stay near the chosen register
            if (cand.front() < 28 || cand.back() > 100) cost += 1000;
            if (cost < bestCost) { bestCost = cost; best = cand; }
        }
    return best;
}

inline int bassNote (const Chord& c, int bassRegister) { return bassRegister + ((c.root - bassRegister % 12) % 12 + 12) % 12; }

// ---------------------------------------------------------------- suggestions (functional harmony)
/** Returns semitone offsets (from the key) of chords that tend to follow a chord rooted at 'fromSemis'. */
inline std::vector<int> nextRoots (int fromSemis, bool minorKey)
{
    if (! minorKey)
        switch (fromSemis)
        {
            case 0:  return { 5, 7, 9, 2 };      // I  -> IV V vi ii
            case 2:  return { 7, 5, 11 };        // ii -> V IV vii
            case 4:  return { 9, 5 };            // iii -> vi IV
            case 5:  return { 7, 0, 2, 5 };      // IV -> V I ii iv
            case 7:  return { 0, 9 };            // V  -> I vi
            case 9:  return { 5, 2, 7 };         // vi -> IV ii V
            case 11: return { 0, 4 };
            case 8:  return { 10, 7 };           // bVI -> bVII V
            case 10: return { 0 };               // bVII -> I
            case 3:  return { 5, 8 };            // bIII -> IV bVI
            case 1:  return { 7, 0 };            // bII -> V I
            default: return { 0, 7 };
        }
    switch (fromSemis)
    {
        case 0:  return { 5, 8, 10, 7 };         // i -> iv bVI bVII V
        case 2:  return { 7 };                   // ii° -> V
        case 3:  return { 8, 10, 5 };            // bIII -> bVI bVII iv
        case 5:  return { 7, 0, 10 };            // iv -> V i bVII
        case 7:  return { 0, 8 };                // v/V -> i bVI
        case 8:  return { 10, 3, 5 };            // bVI -> bVII bIII iv
        case 10: return { 0, 3 };                // bVII -> i bIII
        case 1:  return { 7, 0 };
        default: return { 0, 5 };
    }
}

// ---------------------------------------------------------------- generator
inline const juce::StringArray& styleNames() { static juce::StringArray a { "Soulful House", "Deep House", "Neo-Soul", "Pop", "Jazz", "Cinematic" }; return a; }
inline const std::vector<std::vector<const char*>>& templates (int style)
{
    static const std::vector<std::vector<const char*>> t[6] {
        { { "Imaj7", "vi9", "ii9", "V13" }, { "ii9", "V13", "Imaj9", "IVmaj9" }, { "IVmaj7", "iii7", "ii7", "Imaj7" }, { "vi9", "ii9", "V7sus4", "Imaj9" },
          { "Imaj9", "IVmaj9", "iii7", "vi9" }, { "ii7", "iii7", "IVmaj7", "V13" } },
        { { "i9", "bVImaj7", "bIIImaj7", "bVII" }, { "i7", "iv7", "i7", "iv7" }, { "i9", "bVII", "bVImaj7", "v7" }, { "i7", "bVImaj7", "iv9", "v7" },
          { "i11", "iv9", "bVIImaj7", "bIIImaj7" } },
        { { "ii9", "V13", "Imaj9", "Imaj9" }, { "IVmaj9", "iii7", "vi9", "ii9" }, { "Imaj9", "iii7", "IVmaj9", "iv7" }, { "vi11", "ii9", "V7sus4", "Imaj9" } },
        { { "I", "V", "vi", "IV" }, { "vi", "IV", "I", "V" }, { "I", "vi", "IV", "V" }, { "IV", "I", "V", "vi" }, { "I", "iii", "IV", "V" } },
        { { "ii7", "V7", "Imaj7", "vi7" }, { "iii7", "VI7", "ii7", "V7" }, { "Imaj7", "vi7", "ii7", "V7" }, { "IVmaj7", "bVII7", "Imaj7", "Imaj7" } },
        { { "i", "bVI", "bIII", "bVII" }, { "i", "iv", "bVI", "V" }, { "bVI", "bVII", "i", "i" }, { "i", "bVII", "bVI", "bVII" } } };
    return t[juce::jlimit (0, 5, style)];
}

// ---------------------------------------------------------------- detection
/** Name the chord formed by a set of MIDI notes (best match over roots and qualities). */
inline Chord detectChord (const std::vector<int>& notes)
{
    Chord best; if (notes.size() < 2) return best;
    int mask = 0; for (int n : notes) mask |= 1 << (n % 12);
    const int lowest = *std::min_element (notes.begin(), notes.end()) % 12;
    int bestScore = -100;
    for (int root = 0; root < 12; ++root)
    {
        if (! (mask & (1 << root))) continue;
        for (int q = 0; q < NumQualities; ++q)
        {
            int qm = 0; for (int i : intervals (q)) qm |= 1 << ((root + i) % 12);
            const int hit = juce::countNumberOfBits ((juce::uint32) (qm & mask)), miss = juce::countNumberOfBits ((juce::uint32) (qm & ~mask)), extra = juce::countNumberOfBits ((juce::uint32) (mask & ~qm));
            int score = hit * 3 - miss * 4 - extra * 3 + (root == lowest ? 2 : 0) - (int) intervals (q).size() / 3;
            if (score > bestScore) { bestScore = score; best = { root, q, {} }; }
        }
    }
    return best;
}

/** Krumhansl key estimate from a pitch‑class histogram; returns {key, isMinor}. */
inline std::pair<int, bool> detectKey (const std::array<float, 12>& h)
{
    static const float maj[12] { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
    static const float mnr[12] { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };
    float sum = 0; for (float v : h) sum += v;
    if (sum <= 0) return { -1, false };
    auto corr = [&] (const float* p, int k)
    {
        float mh = sum / 12, mp = 0; for (int i = 0; i < 12; ++i) mp += p[i]; mp /= 12;
        float num = 0, a2 = 0, b2 = 0;
        for (int i = 0; i < 12; ++i) { const float a = h[(size_t) ((i + k) % 12)] - mh, b = p[i] - mp; num += a * b; a2 += a * a; b2 += b * b; }
        return num / std::sqrt (std::max (1e-9f, a2 * b2));
    };
    float best = -2; int bk = 0; bool minor = false;
    for (int k = 0; k < 12; ++k) { if (corr (maj, k) > best) { best = corr (maj, k); bk = k; minor = false; } if (corr (mnr, k) > best) { best = corr (mnr, k); bk = k; minor = true; } }
    return { bk, minor };
}
} // namespace vch
