// test_send_tap.cpp — the per-voice send-tap fix (P9b global-tap regression).
//
// THE BUG: the Macro→East SEND is per-voice (getMacroSend(slot,lane,item)) but the PRE/POST
// TAP that shapes it is Macro-GLOBAL (getGlobalTap, baked into the single published
// macroSendDelta) — so every receiving voice is forced onto one tap. The fix publishes BOTH
// endpoints (macroPreDelta / macroPostDelta) and each voice selects via its own tap; the
// lerp lives in SpreadResolver (the pure seam) so the contract is testable without Rack.
// See docs/design/CC_PROMPT_per_voice_send_tap.md (the spec) and CC_PROMPT_01 (ordering).
//
// The 4 contract asserts:
//   1. BUG:      att≠1, two voices equal send, OPPOSITE taps ⟹ blend_A ≠ blend_B.
//                (RED on the global-tap code — effective() ignores macroTap, both voices
//                get the same baked delta. That red is the point of step 0.)
//   2. IDENTITY: att==1 ⟹ blend_A == blend_B (PRE and POST coincide at unity atten).
//   3. ENDPOINTS: tap=1 == the old POST delta (cv·att·span); tap=0 == PRE (raw cv·span).
//   4. PERSISTENCE: per-voice taps {0.0, 0.3, 1.0} round-trip save→load exactly; a patch
//                with NO macroTap key loads all voices at 1.0 (POST — old patches identical).
//
// Pure floats/bools/json in → floats out; builds and runs OUTSIDE Rack (no SDK needed),
// modelled on test_spread_resolver.cpp (same TEST/EXPECT style; SpreadResolver + the pure
// MacroTapBank round-trip are the seams).
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include "SpreadResolver.hpp"
#include "MacroTapBank.hpp"

using redDot::SpreadResolver;
using dotModular::MacroTapBank;

static int g_pass = 0, g_fail = 0;
#define SUITE(n) std::cout << "\n\033[1;34m[" << (n) << "]\033[0m\n"
#define TEST(desc, ...) do { try { __VA_ARGS__; \
    std::cout << "  \033[32mok\033[0m  " << desc << "\n"; ++g_pass; } \
    catch (const std::exception& e) { \
    std::cout << "  \033[31mFAIL\033[0m " << desc << "  — " << e.what() << "\n"; ++g_fail; } } while(0)
#define EXPECT_NEAR(a,b) do { double _d=(double)(a)-(double)(b); if(_d<0)_d=-_d; if(_d>1e-6){ \
    std::ostringstream _s; _s << "EXPECT_NEAR(" #a "," #b ") : " << (a) << " != " << (b); \
    throw std::runtime_error(_s.str()); } } while(0)
#define EXPECT_DIFF(a,b) do { double _d=(double)(a)-(double)(b); if(_d<0)_d=-_d; if(_d<=1e-6){ \
    std::ostringstream _s; _s << "EXPECT_DIFF(" #a "," #b ") : both " << (a) \
        << " (per-voice taps must change the blend)"; \
    throw std::runtime_error(_s.str()); } } while(0)
#define EXPECT_TRUE(x) do { if(!(x)) { \
    std::ostringstream _s; _s << "EXPECT_TRUE(" #x ") failed"; \
    throw std::runtime_error(_s.str()); } } while(0)

// The blend a receiving voice gets from Macro's send, driven through the resolver's
// per-voice contract: eff = clamp(base + lerp(pre,post,tap)·send). Never touches the old
// macroSendDelta field — the fix deletes it, and this test must survive that deletion.
static float voiceBlend(float base, float pre, float post, float tap, float send) {
    SpreadResolver::Inputs in;
    in.base = base;
    in.macroPresent = true;
    in.macroPreDelta  = pre;
    in.macroPostDelta = post;
    in.macroTap       = tap;
    in.macroSend      = send;
    return SpreadResolver::effective(in);
}

int main() {
    // Setup per the spec: Macro CV connected, attenuverter att = 0.5 (≠1), so PRE ≠ POST.
    // Concrete numbers (span = the (hi-lo) scale the publish multiplies in):
    const float cv   = 0.4f;
    const float att  = 0.5f;
    const float span = 2.f;
    const float pre  = cv * span;         // tap=0 endpoint: raw cv, PRE the attenuverter
    const float post = cv * att * span;   // tap=1 endpoint: cv·att — the old tappedDelta(tap=1)
    const float send = 0.7f;              // EQUAL send for both voices

    SUITE("ASSERT 1 (the bug): att≠1, equal send, opposite taps ⟹ blends differ");
    TEST("voiceA tap=0 (PRE) vs voiceB tap=1 (POST), equal send ⟹ blend_A ≠ blend_B", {
        // RED on the global-tap code: effective() ignores macroTap, so both voices receive
        // the same (globally-baked) delta and the blends collide. Green only when the
        // per-voice lerp is wired into effective().
        EXPECT_DIFF(voiceBlend(0.f, pre, post, 0.f, send),
                    voiceBlend(0.f, pre, post, 1.f, send));
    });

    SUITE("ASSERT 2 (identity): att==1 ⟹ PRE and POST coincide");
    TEST("unity atten: tap=0 and tap=1 give identical blends", {
        const float unityPre  = cv * span;          // cv·1·span — same value both endpoints
        const float unityPost = cv * 1.f * span;
        EXPECT_NEAR(voiceBlend(0.f, unityPre, unityPost, 0.f, send),
                    voiceBlend(0.f, unityPre, unityPost, 1.f, send));
    });

    SUITE("ASSERT 3 (endpoints): the lerp endpoints match the pre-regression semantics");
    TEST("tap=1 blend == base + cv·att·span·send (the old POST tappedDelta)", {
        EXPECT_NEAR(voiceBlend(0.1f, pre, post, 1.f, send), 0.1f + post * send);
    });
    TEST("tap=0 blend == base + cv·span·send (raw cv, PRE the attenuverter)", {
        EXPECT_NEAR(voiceBlend(0.1f, pre, post, 0.f, send), 0.1f + pre * send);
    });
    TEST("mid tap 0.3 lerps linearly between the endpoints", {
        const float mid = pre + (post - pre) * 0.3f;
        EXPECT_NEAR(voiceBlend(0.f, pre, post, 0.3f, send), mid * send);
    });
    TEST("send=0 gates the whole macro term (absent send adds nothing)", {
        EXPECT_NEAR(voiceBlend(0.25f, pre, post, 1.f, 0.f), 0.25f);
    });

    SUITE("ASSERT 4 (persistence): per-voice taps round-trip; missing key ⟹ POST default");
    TEST("taps {0.0, 0.3, 1.0} across 3 voices survive save→load exactly", {
        float taps[MacroTapBank::N];
        for (int i = 0; i < MacroTapBank::N; ++i) taps[i] = MacroTapBank::kDefault;
        const int iA = MacroTapBank::index(0, 2, 0);   // voice 0, lane 2, LOR kind
        const int iB = MacroTapBank::index(1, 2, 0);   // voice 1, same lane
        const int iC = MacroTapBank::index(2, 2, 0);   // voice 2, same lane
        taps[iA] = 0.0f;
        taps[iB] = 0.3f;
        taps[iC] = 1.0f;
        json_t* root = json_object();
        MacroTapBank::serialize(taps, root, "macroTap");
        float loaded[MacroTapBank::N];
        const bool full = MacroTapBank::parse(root, "macroTap", loaded);
        json_decref(root);
        EXPECT_TRUE(full);
        EXPECT_NEAR(loaded[iA], 0.0f);
        EXPECT_NEAR(loaded[iB], 0.3f);
        EXPECT_NEAR(loaded[iC], 1.0f);
        bool othersDefault = true;
        for (int i = 0; i < MacroTapBank::N; ++i)
            if (i != iA && i != iB && i != iC && loaded[i] != MacroTapBank::kDefault) othersDefault = false;
        EXPECT_TRUE(othersDefault);
    });
    TEST("a patch with NO macroTap key loads all voices at 1.0 (POST — old patches identical)", {
        json_t* root = json_object();   // no macroTap key at all
        float loaded[MacroTapBank::N];
        MacroTapBank::parse(root, "macroTap", loaded);
        json_decref(root);
        bool allPost = true;
        for (int i = 0; i < MacroTapBank::N; ++i)
            if (loaded[i] != MacroTapBank::kDefault) allPost = false;
        EXPECT_TRUE(allPost);
    });
    TEST("a short/partial macroTap array defaults the missing entries to POST", {
        json_t* root = json_object();
        json_t* ja = json_array();
        for (int i = 0; i < 10; ++i) json_array_append_new(ja, json_real(0.0));
        json_object_set_new(root, "macroTap", ja);
        float loaded[MacroTapBank::N];
        MacroTapBank::parse(root, "macroTap", loaded);
        json_decref(root);
        EXPECT_NEAR(loaded[9], 0.0f);
        EXPECT_NEAR(loaded[10], MacroTapBank::kDefault);
        EXPECT_NEAR(loaded[MacroTapBank::N - 1], MacroTapBank::kDefault);
    });

    std::cout << "\n" << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail == 0 ? 0 : 1;
}
