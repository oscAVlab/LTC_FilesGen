// osc[AV]lab LTC — utilidades de timecode SMPTE
#pragma once
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

namespace tc
{
enum RateId { R2398 = 0, R24, R25, R2997DF, R2997ND, R30, NumRates };

struct Rate
{
    int n;            // frames por segundo nominales (24/25/30)
    double real;      // frames por segundo reales
    bool df;          // drop frame
    int mtcCode;      // código MTC: 0=24 1=25 2=29.97DF 3=30
    const char* label;
};

inline const Rate& rate (int id)
{
    static const Rate r[NumRates] = {
        { 24, 24000.0 / 1001.0, false, 0, "23.976" },
        { 24, 24.0,             false, 0, "24" },
        { 25, 25.0,             false, 1, "25" },
        { 30, 30000.0 / 1001.0, true,  2, "29.97 DF" },
        { 30, 30000.0 / 1001.0, false, 3, "29.97 ND" },
        { 30, 30.0,             false, 3, "30" },
    };
    return r[(id >= 0 && id < NumRates) ? id : R25];
}

struct Label { int h = 0, m = 0, s = 0, f = 0; };

inline int64_t dayFrames (const Rate& r) { return r.df ? (int64_t) 17982 * 144 : (int64_t) 24 * 3600 * r.n; }

inline int64_t labelToAbs (const Label& l, const Rate& r)
{
    int64_t a = ((int64_t) l.h * 3600 + l.m * 60 + l.s) * r.n + l.f;
    if (r.df) { const int64_t tm = l.h * 60 + l.m; a -= 2 * (tm - tm / 10); }
    return a;
}

inline Label absToLabel (int64_t a, const Rate& r)
{
    const int64_t day = dayFrames (r);
    a = ((a % day) + day) % day;
    if (r.df)
    {
        const int64_t D = a / 17982, M = a % 17982;
        a += 18 * D + (M > 1 ? 2 * ((M - 2) / 1798) : 0);
    }
    const int n = r.n;
    return { (int) ((a / ((int64_t) n * 3600)) % 24), (int) ((a / ((int64_t) n * 60)) % 60), (int) ((a / n) % 60), (int) (a % n) };
}

// envuelve una diferencia de frames al rango [-día/2, día/2) (cruce de medianoche)
inline double wrapDiff (double d, const Rate& r)
{
    const double day = (double) dayFrames (r);
    d = std::fmod (d, day);
    if (d >= day / 2) d -= day;
    if (d < -day / 2) d += day;
    return d;
}

inline std::string format (const Label& l, bool df)
{
    char b[16];
    std::snprintf (b, sizeof (b), "%02d:%02d:%02d%c%02d", l.h, l.m, l.s, df ? ';' : ':', l.f);
    return b;
}

// ---------- MTC ----------
inline void fullFrame (const Label& l, int mtcCode, uint8_t out[10])
{
    const uint8_t m[10] = { 0xF0, 0x7F, 0x7F, 0x01, 0x01, (uint8_t) ((mtcCode << 5) | l.h), (uint8_t) l.m, (uint8_t) l.s, (uint8_t) l.f, 0xF7 };
    for (int i = 0; i < 10; ++i) out[i] = m[i];
}

// q = índice absoluto de quarter frame (frame*4 + sub). Pieza 0 siempre en frame par.
inline void quarterFrame (int64_t q, const Rate& r, int mtcCode, uint8_t out[2])
{
    const int piece = (int) (((q % 8) + 8) % 8);
    const int64_t baseQ = q - piece;
    const Label l = absToLabel (baseQ / 4, r);
    int nib = 0;
    switch (piece)
    {
        case 0: nib = l.f & 15; break;
        case 1: nib = l.f >> 4; break;
        case 2: nib = l.s & 15; break;
        case 3: nib = l.s >> 4; break;
        case 4: nib = l.m & 15; break;
        case 5: nib = l.m >> 4; break;
        case 6: nib = l.h & 15; break;
        default: nib = ((l.h >> 4) & 1) | (mtcCode << 1); break;
    }
    out[0] = 0xF1;
    out[1] = (uint8_t) ((piece << 4) | nib);
}
} // namespace tc
