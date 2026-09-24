// =============================================================
//  corrupt.cpp — Deterministic text corruption for Smith.
//  Pure C++, no Arduino deps — unit-testable on native.
//
//  Design goal: Random cyberpunk anomaly aesthetic while
//  keeping the iconic quotes readable (scattered symbols & bits).
// =============================================================
#include "corrupt.h"
#include <string.h>

// XorShift32 — fast, seedable PRNG
static uint32_t xorshift(uint32_t& state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

static const char GLITCH_SYMBOLS[] = "01#@!$%~_<>738/?*";
static constexpr int N_SYMBOLS = (int)(sizeof(GLITCH_SYMBOLS) - 1);

void corrupt(char* text, int level, uint32_t seed)
{
    if (!text || *text == '\0' || level < 1)
    {
        return;
    }

    uint32_t rng = seed ^ 0xDEADBEEFu;
    if (rng == 0) rng = 1;

    const size_t len = strlen(text);
    if (len == 0) return;

    // Number of random characters to glitch based on level:
    // Level 1: ~2 chars, scaling up to ~8-12 chars at Level 5.
    // Leaves >70% of text intact so quotes remain readable.
    int n_subs = level + 1 + (int)(xorshift(rng) % (uint32_t)(level + 1));
    if (n_subs > (int)(len * 0.35f) && len >= 6)
    {
        n_subs = (int)(len * 0.35f);
    }
    if (n_subs < level)
    {
        n_subs = level;
    }

    bool used[128] = {false};
    int glitched = 0;
    int attempts = 0;

    while (glitched < n_subs && attempts < 100)
    {
        ++attempts;
        size_t pos = xorshift(rng) % len;
        // Avoid overwriting spaces or sentence punctuation
        if (!used[pos] && text[pos] != ' ' && text[pos] != '.' && text[pos] != ',' && text[pos] != '\'')
        {
            text[pos] = GLITCH_SYMBOLS[xorshift(rng) % (uint32_t)N_SYMBOLS];
            used[pos] = true;
            ++glitched;
        }
    }

    // In Level 3+, optionally inject a subtle '0' or '1' glitch
    if (level >= 3 && len > 8)
    {
        size_t bpos = xorshift(rng) % (len - 1);
        if (text[bpos] != ' ' && text[bpos] != '.')
        {
            text[bpos] = (xorshift(rng) % 2 == 0) ? '0' : '1';
        }
    }

    text[len] = '\0';
}
