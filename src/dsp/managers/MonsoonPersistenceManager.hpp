#pragma once
#include <rack.hpp>

struct Monsoon; // Forward declaration

/**
 * PersistenceManager handles saving and loading the module's state to/from JSON.
 * It encapsulates the serialization of various modes, settings, and the
 * internal DNA random buffers to ensure pattern stability across sessions.
 */
class PersistenceManager {
public:
    /// Convert module and engine state to a JSON object
    static json_t* toJson(Monsoon* module);

    /// Restore module and engine state from a JSON object
    static void fromJson(Monsoon* module, json_t* root);
};