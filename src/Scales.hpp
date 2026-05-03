#pragma once
#include <vector>
#include <string>

struct ScaleType {
    std::string name;
    std::vector<int> intervals; 
};

static const std::vector<ScaleType> BITWIG_SCALES = {
    {"Chromatic", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
    {"Major", {0, 2, 4, 5, 7, 9, 11}},
    {"Natural Minor", {0, 2, 3, 5, 7, 8, 10}},
    {"Harmonic Minor", {0, 2, 3, 5, 7, 8, 11}},
    {"Melodic Minor", {0, 2, 3, 5, 7, 9, 11}},
    {"Dorian", {0, 2, 3, 5, 7, 9, 10}},
    {"Phrygian", {0, 1, 3, 5, 7, 8, 10}},
    {"Lydian", {0, 2, 4, 6, 7, 9, 11}},
    {"Mixolydian", {0, 2, 4, 5, 7, 9, 10}},
    {"Locrian", {0, 1, 3, 5, 6, 8, 10}},
    {"Pentatonic Major", {0, 2, 4, 7, 9}},
    {"Pentatonic Minor", {0, 3, 5, 7, 10}},
    {"Blues", {0, 3, 5, 6, 7, 10}},
    {"Whole Tone", {0, 2, 4, 6, 8, 10}},
    {"Diminished", {0, 2, 3, 5, 6, 8, 9, 11}},
    {"Super Locrian", {0, 1, 3, 4, 6, 8, 10}},
    {"Bhairav", {0, 1, 4, 5, 7, 8, 11}},
    {"Hungarian Minor", {0, 2, 3, 6, 7, 8, 11}},
    {"Hirojoshi", {0, 2, 3, 7, 8}},
    {"In-Sen", {0, 1, 5, 7, 10}},
    {"Iwato", {0, 1, 5, 6, 10}},
    {"Kumoi", {0, 2, 3, 7, 9}},
    {"Pelog", {0, 1, 3, 7, 8}},
    {"Spanish", {0, 1, 4, 5, 7, 8, 10}}
};