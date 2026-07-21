/**
 * test_change_alley_transforms.cpp — §10 restructure-transform invariants.
 * Compile: g++ -std=c++17 -Itest test/test_change_alley_transforms.cpp -o /tmp/t_cat && /tmp/t_cat
 */
#include "../src/dsp/ChangeAlleyTransforms.hpp"
#include <cstdio>
#include <cstring>

static int pass = 0, fail = 0;
#define CHK(c, m) do { if (c) ++pass; else { ++fail; std::printf("  FAIL: %s\n", m); } } while (0)
using namespace dotModular::ca;

static void fillIdent(uint8_t* s) { for (int v = 0; v < 16; ++v) s[v] = (uint8_t)v; }

int main() {
    // Collapse
    { uint8_t s[16]; fillIdent(s); collapse(s, 16, 1);
      bool id = true; for (int v = 0; v < 16; ++v) id &= (s[v] == v);
      CHK(id, "collapse@1 == identity"); }
    { uint8_t s[16]; fillIdent(s); collapse(s, 16, 4);
      CHK(s[0]==0 && s[3]==0 && s[4]==4 && s[7]==4 && s[15]==12, "collapse@4 = quartet leaders"); }
    { uint8_t s[16]; fillIdent(s); collapse(s, 16, 16);
      bool uni = true; for (int v = 0; v < 16; ++v) uni &= (s[v] == 0);
      CHK(uni, "collapse@16 = total unison on voice 0"); }
    { uint8_t s[16]; fillIdent(s); collapse(s, 6, 4);          // active pool 6, partial block {4,5}
      CHK(s[4]==4 && s[5]==4, "collapse partial trailing block -> its own leader");
      CHK(s[8]==8 && s[15]==15, "rows beyond active pool untouched"); }

    // Rotate
    { uint8_t s[16]; fillIdent(s); rotate(s, 16, 16);
      CHK(s[0]==1 && s[15]==0, "rotate@16: +1 with wrap");
      rotate(s, 16, 16);
      CHK(s[0]==2, "rotate composes per trigger (march)"); }
    { uint8_t s[16]; fillIdent(s); rotate(s, 16, 4);
      CHK(s[0]==1 && s[3]==0 && s[4]==5 && s[7]==4, "rotate@4 wraps within blocks"); }
    { uint8_t s[16]; fillIdent(s); rotate(s, 6, 4);            // partial block {4,5}: span 2
      CHK(s[4]==5 && s[5]==4, "rotate partial block wraps within its live span"); }

    // Scatter
    { uint8_t a[16], b2[16]; fillIdent(a); fillIdent(b2);
      scatter(a, 16, 4, 1234); scatter(b2, 16, 4, 1234);
      CHK(std::memcmp(a, b2, 16) == 0, "scatter deterministic per seed");
      bool inBlock = true;
      for (int v = 0; v < 16; ++v) { int base = (v/4)*4; inBlock &= (a[v] >= base && a[v] < base+4); }
      CHK(inBlock, "scatter@4 sources stay within each block"); }
    { uint8_t s[16]; fillIdent(s); scatter(s, 6, 16, 77);
      bool live = true; for (int v = 0; v < 6; ++v) live &= (s[v] < 6);
      CHK(live, "scatter never sources a dead voice (active-pool tiling)");
      CHK(s[10]==10, "scatter leaves rows beyond active pool untouched"); }

    // Reflect
    { uint8_t s[16]; fillIdent(s); reflect(s, 16, 4);
      CHK(s[0]==3 && s[3]==0 && s[4]==7 && s[7]==4, "reflect@4 = retrograde within blocks");
      reflect(s, 16, 4);
      bool id = true; for (int v = 0; v < 16; ++v) id &= (s[v] == v);
      CHK(id, "reflect is self-inverse (double trigger restores)"); }
    { uint8_t s[16]; fillIdent(s); reflect(s, 5, 4);           // partial block {4}: centre self-maps
      CHK(s[4]==4, "reflect odd/1-wide partial block centre self-maps"); }

    // One-pin-per-row is structural for all: each s[v] is a single value — assert range.
    { uint8_t s[16]; fillIdent(s);
      collapse(s,16,8); rotate(s,16,8); scatter(s,16,8,9); reflect(s,16,8);
      bool ok = true; for (int v = 0; v < 16; ++v) ok &= (s[v] < 16);
      CHK(ok, "chained transforms keep sources in range"); }

    std::printf(fail ? "\n%d passed, %d FAILED\n" : "\n%d passed, 0 failed\n", pass, fail);
    return fail ? 1 : 0;
}
