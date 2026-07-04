// test_scale_nondestructive.cpp
// Verifies the NON-DESTRUCTIVE scale enforcement logic (SHOPHOUSE_SPEC.md).
// The full ScaleManager needs a Monsoon module, so this replicates the PURE logic:
//   - calculateMask (root+scale → 12-bit in-scale mask)
//   - the getSemitoneWeight enforcement RULE (read-time gate to 0 when locked+out-of-scale)
//   - the NON-DESTRUCTIVE invariant: enforcement reads the STORE + mask; it must NEVER need to
//     mutate the store, so the stored weights survive lock toggles and scale changes unchanged.
#include <cstdint>
#include <cstdio>
#include <vector>
static int fails=0;
static void chk(bool c,const char*m){ if(!c){printf("FAIL: %s\n",m);++fails;} }

// Mirror of ScaleManager::calculateMask: bit i set if semitone i is in (root, intervals).
static uint16_t calcMask(int root, const std::vector<int>& intervals){
    uint16_t m=0;
    for(int iv: intervals) m |= (uint16_t)(1u << (((root+iv)%12+12)%12));
    return m;
}

// Mirror of the getSemitoneWeight ENFORCEMENT rule (the read-time, non-destructive gate):
// locked & out-of-scale → 0; else the stored weight. Note it reads `stored` and NEVER writes it.
static float weightRead(bool locked, uint16_t mask, int sem, const float* stored){
    if(sem<0||sem>11) return 0.f;
    if(locked && !(mask & (1u<<sem))) return 0.f;   // enforcement = read-time gate
    return stored[sem];
}

int main(){
    // Major scale intervals from C (root 0): 0,2,4,5,7,9,11.
    std::vector<int> major = {0,2,4,5,7,9,11};
    uint16_t cMaj = calcMask(0, major);
    chk(cMaj == 0b101010110101, "C major mask correct (C D E F G A B)");
    // D major (root 2): should include C#(1), F#(6), etc.
    uint16_t dMaj = calcMask(2, major);
    chk( (dMaj & (1<<1)) && (dMaj & (1<<6)), "D major includes C# and F#");
    chk(!(dMaj & (1<<0)), "D major excludes C natural");

    // A user's stored weights across all 12 semitones (the STORE). Includes out-of-scale values.
    float store[12] = {0.9f,0.3f,0.7f,0.4f,0.8f,0.5f,0.2f,0.6f,0.35f,0.75f,0.45f,0.85f};
    float storeBackup[12]; for(int i=0;i<12;i++) storeBackup[i]=store[i];

    // 1. UNLOCKED (guide): every semitone reads its stored weight, in or out of scale.
    for(int s=0;s<12;s++)
        chk(weightRead(false, cMaj, s, store)==store[s], "unlocked: reads stored weight (guide)");

    // 2. LOCKED (enforce): in-scale read stored, out-of-scale read 0 — WITHOUT touching store.
    for(int s=0;s<12;s++){
        bool inScale = cMaj & (1<<s);
        float w = weightRead(true, cMaj, s, store);
        chk(w == (inScale? store[s] : 0.f), "locked: in-scale=stored, out-of-scale=0");
    }

    // 3. NON-DESTRUCTIVE invariant: after all those locked reads, the STORE is UNCHANGED.
    for(int i=0;i<12;i++) chk(store[i]==storeBackup[i], "store unchanged by enforcement reads");

    // 4. Toggle lock OFF again → out-of-scale weights REAPPEAR at their original values (the
    //    whole point: destructive forcing would have zeroed them permanently).
    for(int s=0;s<12;s++)
        chk(weightRead(false, cMaj, s, store)==storeBackup[s], "unlock reveals original weights");

    // 5. Change scale (C major → D major) while locked → different notes gated, but the STORE
    //    is still intact; notes that were out-of-scale in C but in-scale in D read their stored
    //    value (would be LOST under destructive redistribution/forcing).
    //    C# (1): out in C major, IN in D major.
    chk(weightRead(true, cMaj, 1, store)==0.f,        "C#: gated under C major");
    chk(weightRead(true, dMaj, 1, store)==store[1],   "C#: reappears at stored value under D major");
    for(int i=0;i<12;i++) chk(store[i]==storeBackup[i], "store intact across scale change");

    printf(fails? "\n%d FAILED\n" : "\nALL PASS (non-destructive scale enforcement)\n", fails);
    return fails?1:0;
}
