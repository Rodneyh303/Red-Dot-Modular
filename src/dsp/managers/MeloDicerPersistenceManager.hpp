#pragma once
#include <rack.hpp>

struct MeloDicer;

/**
 * PersistenceManager handles saving and loading the module's state to/from JSON.
 * It encapsulates the serialization of various modes, settings, and the
 * internal DNA random buffers to ensure pattern stability across sessions.
 */
class PersistenceManager {
public:
    /// Convert module and engine state to a JSON object
    static json_t* toJson(MeloDicer* module);

    /// Restore module and engine state from a JSON object
    static void fromJson(MeloDicer* module, json_t* root);
};