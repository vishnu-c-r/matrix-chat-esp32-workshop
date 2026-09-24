// =============================================================
//  corrupt.cpp — Deterministic text corruption for Smith.
//  Pure C++, no Arduino deps — unit-testable on native.
//
//  Design goal: Glitched cyberpunk anomaly aesthetic while
//  keeping the iconic quotes readable (subtle leet/symbol mods).
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

static char glitchChar(char c, uint32_t r)
{
    switch (c)
    {
        case 'a': case 'A': return (r % 2 == 0) ? '@' : '4';
        case 'b': case 'B': return '8';
        case 'c': case 'C': return '<';
        case 'e': case 'E': return '3';
        case 'g': case 'G': return '9';
        case 'i': case 'I': return '1';
        case 'l': case 'L': return '1';
        case 'o': case 'O': return '0';
        case 's': case 'S': return (r % 2 == 0) ? '5' : '$';
        case 't': case 'T': return '7';
        case 'z': case 'Z': return '2';
        default:
        {
            static const char SUB_SYMBOLS[] = "~_";
            return SUB_SYMBOLS[r % (sizeof(SUB_SYMBOLS) - 1)];
        }
    }
}

static bool isLeetTarget(char c)
{
    switch (c)
    {
        case 'a': case 'A':
        case 'b': case 'B':
        case 'c': case 'C':
        case 'e': case 'E':
        case 'g': case 'G':
        case 'i': case 'I':
        case 'l': case 'L':
        case 'o': case 'O':
        case 's': case 'S':
        case 't': case 'T':
        case 'z': case 'Z':
            return true;
        default:
            return false;
    }
}

void corrupt(char* text, int level, uint32_t seed)
{
    if (!text || *text == '\0' || level < 1)
    {
        return;
    }

    uint32_t rng = seed ^ 0xDEADBEEFu;
    if (rng == 0) rng = 1;  // XorShift must not start at 0

    const size_t len = strlen(text);

    // Number of characters to subtly glitch based on level:
    // Keeps 85%+ of text completely intact so words remain readable.
    size_t target = (size_t)level;
    if (target > len / 4 && len >= 4)
    {
        target = len / 4;
    }
    if (level >= 5 && len >= 12 && target < 4)
    {
        target = 4;
    }
    if (level == 1)
    {
        target = 1;
    }

    // Identify candidate indices: prioritize letters with natural leet replacements
    size_t leet_idx[128];
    size_t other_idx[128];
    size_t n_leet = 0;
    size_t n_other = 0;

    for (size_t i = 0; i < len && i < 128; ++i)
    {
        char c = text[i];
        if (isLeetTarget(c))
        {
            leet_idx[n_leet++] = i;
        }
        else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
        {
            other_idx[n_other++] = i;
        }
    }

    bool used[128] = {false};
    size_t n_glitched = 0;
    int attempts = 0;

    while (n_glitched < target && attempts < 100 && (n_leet + n_other) > n_glitched)
    {
        ++attempts;
        uint32_t r = xorshift(rng);
        size_t idx;
        if (n_leet > 0 && ((r % 10 < 8) || n_other == 0))
        {
            idx = leet_idx[r % n_leet];
        }
        else if (n_other > 0)
        {
            idx = other_idx[r % n_other];
        }
        else
        {
            break;
        }

        if (!used[idx])
        {
            char orig = text[idx];
            char replacement = glitchChar(orig, xorshift(rng));
            if (replacement != orig)
            {
                text[idx] = replacement;
                used[idx] = true;
                ++n_glitched;
            }
        }
    }

    // Guarantee null-termination
    text[len] = '\0';
}
