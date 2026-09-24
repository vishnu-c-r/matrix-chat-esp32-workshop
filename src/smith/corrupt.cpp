// =============================================================
//  corrupt.cpp — Deterministic text corruption for Smith.
//  Pure C++, no Arduino deps — unit-testable on native.
// =============================================================
#include "corrupt.h"
#include <string.h>

// XorShift32 — fast, seedable, good enough for visual noise.
static uint32_t xorshift(uint32_t& state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

static const char SYMBOLS[] = "01#@!%^&*[]{}|\\?/~`<>_";
static constexpr int N_SYMBOLS = (int)(sizeof(SYMBOLS) - 1);

void corrupt(char* text, int level, uint32_t seed)
{
    if (!text || *text == '\0' || level < 1)
    {
        return;
    }

    uint32_t rng = seed ^ 0xDEADBEEFu;
    if (rng == 0) rng = 1;  // XorShift must not start at 0

    const size_t len = strlen(text);

    // ---- Level 1: a few symbol substitutions --------------------
    int n_subs = level * 3;
    for (int i = 0; i < n_subs; ++i)
    {
        size_t pos = xorshift(rng) % len;
        text[pos]  = SYMBOLS[xorshift(rng) % (uint32_t)N_SYMBOLS];
    }

    // ---- Level 2+: replace a short span with "01" runs ----------
    if (level >= 2)
    {
        size_t start = xorshift(rng) % len;
        size_t run   = 2 + (xorshift(rng) % (uint32_t)(level * 2));
        for (size_t j = 0; j < run && (start + j) < len; ++j)
        {
            text[start + j] = (char)('0' + (j % 2));
        }
    }

    // ---- Level 3+: duplicate a short fragment in-place ----------
    if (level >= 3 && len > 4)
    {
        size_t src  = xorshift(rng) % (len / 2);
        size_t dst  = len / 2 + xorshift(rng) % (len / 2);
        size_t flen = 1 + xorshift(rng) % 4;
        for (size_t j = 0; j < flen && (dst + j) < len; ++j)
        {
            text[dst + j] = text[src + j % flen];
        }
    }

    // ---- Level 4+: heavier symbol sweep -------------------------
    if (level >= 4)
    {
        for (size_t j = 0; j < len; j += 3 + (xorshift(rng) % 3))
        {
            text[j] = SYMBOLS[xorshift(rng) % (uint32_t)N_SYMBOLS];
        }
    }

    // ---- Level 5: final scramble — swap pairs -------------------
    if (level >= 5)
    {
        for (size_t j = 0; j + 1 < len; j += 2)
        {
            char tmp    = text[j];
            text[j]     = text[j + 1];
            text[j + 1] = tmp;
        }
    }

    // Guarantee null-termination was not overwritten.
    text[len] = '\0';
}
