// osc[AV]lab LTC — decodificador y codificador LTC (SMPTE 12M, biphase mark)
#pragma once
#include "Timecode.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace tc
{
struct LtcFrame
{
    Label label;
    bool df = false;
    double endSample = 0;   // muestra absoluta donde termina el frame (= empieza el siguiente)
    double periodSamples = 0;
};

// Decodificador: alimentar muestras; devuelve frames por callback.
class LtcDecoder
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        Tmin = sr / (80.0 * 36.0);
        Tmax = sr / (80.0 * 20.0);
        reset();
    }
    void reset()
    {
        T = sr / (80.0 * 27.0); sign = 0; last = 0; half = false; peak = 0; lastSync = -1;
        bits.fill (0);
    }

    // start = índice absoluto de la primera muestra del bloque
    template <typename Fn>
    void process (const float* x, int n, int64_t start, Fn&& onFrame)
    {
        for (int i = 0; i < n; ++i)
        {
            const float v = x[i], a = std::abs (v);
            peak = a > peak ? a : peak * 0.99995f;
            if (a > level) level = a;
            const float th = std::max (0.002f, peak * 0.25f);
            bool flip = false;
            if (sign >= 0 && v < -th)      { sign = -1; flip = true; }
            else if (sign <= 0 && v > th)  { sign = 1;  flip = true; }
            if (! flip) continue;

            const int64_t idx = start + i;
            const double iv = (double) (idx - last);
            last = idx;
            if (iv < 0.3 * Tmin) continue;
            if (iv < 0.75 * T)
            {
                T += 0.05 * (2.0 * iv - T);
                if (half) { half = false; pushBit (1, idx, onFrame); }
                else half = true;
            }
            else if (iv < 1.5 * T)
            {
                T += 0.05 * (iv - T);
                half = false;
                pushBit (0, idx, onFrame);
            }
            else
            {
                half = false;
                if (iv < 2.5 * T) T += 0.05 * (iv - T);
                else bits.fill (2);   // hueco: lo acumulado ya no vale
            }
            T = std::clamp (T, Tmin, Tmax);
        }
    }

    float takeLevel() { const float l = level; level = 0; return l; }

private:
    template <typename Fn>
    void pushBit (int b, int64_t at, Fn& onFrame)
    {
        std::copy (bits.begin() + 1, bits.end(), bits.begin());
        bits[79] = (uint8_t) b;
        static const uint8_t S[16] = { 0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,1 };
        for (int i = 0; i < 16; ++i) if (bits[64 + i] != S[i]) return;
        auto v = [this] (int s, int l) { int x = 0; for (int i = 0; i < l; ++i) x |= bits[s + i] << i; return x; };
        LtcFrame fr;
        fr.label = { v (48, 4) + 10 * v (56, 2), v (32, 4) + 10 * v (40, 3), v (16, 4) + 10 * v (24, 3), v (0, 4) + 10 * v (8, 2) };
        fr.df = bits[10] != 0;
        fr.periodSamples = lastSync >= 0 ? (double) (at - lastSync) : 0.0;
        lastSync = at;
        if (fr.label.f > 29 || fr.label.s > 59 || fr.label.m > 59 || fr.label.h > 23) return;
        fr.endSample = (double) at;   // el bit 79 se cierra con el flanco que inicia el frame siguiente
        onFrame (fr);
    }

    double sr = 48000, T = 20, Tmin = 16, Tmax = 30;
    int sign = 0; int64_t last = 0, lastSync = -1; bool half = false;
    float peak = 0, level = 0;
    std::array<uint8_t, 80> bits {};
};

// 80 bits de un frame LTC
inline std::array<uint8_t, 80> ltcBits (int64_t absFrame, const Rate& r)
{
    std::array<uint8_t, 80> b {};
    const Label l = absToLabel (absFrame, r);
    auto bcd = [&b] (int val, int start, int tens)
    {
        const int u = val % 10, t = val / 10;
        for (int i = 0; i < 4; ++i) b[(size_t) (start + i)] = (uint8_t) ((u >> i) & 1);
        for (int i = 0; i < tens; ++i) b[(size_t) (start + 8 + i)] = (uint8_t) ((t >> i) & 1);
    };
    bcd (l.f, 0, 2); bcd (l.s, 16, 3); bcd (l.m, 32, 3); bcd (l.h, 48, 2);
    if (r.df) b[10] = 1;
    static const uint8_t S[16] = { 0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,1 };
    for (int i = 0; i < 16; ++i) b[(size_t) (64 + i)] = S[i];
    // bit de corrección de polaridad (bit 27 en 25 fps, bit 59 en el resto): número par de ceros
    int zeros = 0; for (auto x : b) zeros += x == 0;
    const int pc = r.n == 25 ? 59 : 27;
    if (zeros % 2 != 0) b[(size_t) pc] = 1;
    return b;
}

// Codificador continuo: sigue una posición en frames (double) que avanza a "fps por muestra".
class LtcEncoder
{
public:
    void reset() { haveLast = false; state = 0.5f; y = 0; }

    // render n muestras empezando en la posición pos (frames), avanzando step frames por muestra
    void render (float* out, int n, double pos, double step, const Rate& r, float gain = 0.5f)
    {
        for (int i = 0; i < n; ++i)
        {
            const double p = pos + step * i;
            const int64_t frame = (int64_t) std::floor (p);
            const int halfIdx = (int) std::floor ((p - (double) frame) * 160.0);
            if (frame != cachedFrame) { cachedBits = ltcBits (frame, r); cachedFrame = frame; }
            if (! haveLast || frame != lastFrame || halfIdx != lastHalf)
            {
                // nueva media celda: flanco al inicio de cada bit, y a mitad si el bit es 1
                if (halfIdx % 2 == 0) state = -state;
                else if (cachedBits[(size_t) (halfIdx / 2)] == 1) state = -state;
                lastFrame = frame; lastHalf = halfIdx; haveLast = true;
            }
            // flanco suavizado (~25 µs), como pide el estándar
            y += 0.45f * (state * 2.0f * gain - y);
            out[i] = y;
        }
    }

private:
    bool haveLast = false;
    int64_t lastFrame = -1, cachedFrame = INT64_MIN;
    int lastHalf = -1;
    float state = 0.5f, y = 0;
    std::array<uint8_t, 80> cachedBits {};
};
} // namespace tc
