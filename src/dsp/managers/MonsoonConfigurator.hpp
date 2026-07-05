#pragma once

struct Monsoon;

/**
 * MonsoonConfigurator
 * Helper to centralize boilerplate parameter and IO configuration.
 */
struct MonsoonConfigurator {
    static void setup(Monsoon* m);
};