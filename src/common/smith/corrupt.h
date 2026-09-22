// =============================================================
//  corrupt.h — Deterministic text corruption for Smith firmware.
//  Pure C++, no Arduino deps — unit-testable on native.
// =============================================================
#pragma once
#include <stdint.h>

// Corrupt text in-place.
//   level : 1 (mild) … 5 (nearly unreadable)
//   seed  : any value; same seed + same text = same output (deterministic)
// The function never writes past the original null terminator.
void corrupt(char* text, int level, uint32_t seed);
