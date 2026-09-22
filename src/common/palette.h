// =============================================================
//  palette.h — 12 distinguishable hues for node chat colors.
//  Red is RESERVED for Smith; nodes get color = node_id % N_COLORS.
// =============================================================
#pragma once
#include <stdint.h>

struct Color { uint8_t r, g, b; };

// 11 non-red hues that are visually distinct at low LED brightness.
static constexpr uint8_t N_COLORS = 11;

static constexpr Color PALETTE[N_COLORS] = {
    {  0, 255, 128},   //  0  mint green
    {  0, 200, 255},   //  1  cyan
    {128,   0, 255},   //  2  violet
    {255, 200,   0},   //  3  amber
    {  0, 128, 255},   //  4  azure
    {255, 128,   0},   //  5  orange
    {128, 255,   0},   //  6  lime
    {255,   0, 200},   //  7  magenta
    {  0, 255, 200},   //  8  seafoam
    {200, 128, 255},   //  9  lavender
    {255, 255,   0},   // 10  yellow
};

// Red is Smith's exclusive color — never assigned to a node.
static constexpr Color COLOR_SMITH = {255, 0, 0};
static constexpr Color COLOR_OFF   = {  0, 0, 0};

// Returns the palette color for a given node ID.
inline Color nodeColor(uint8_t node_id) {
    return PALETTE[node_id % N_COLORS];
}
