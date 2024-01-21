#pragma once

#include <cstdint>

// Taken from watchdog/watchdog.py
constexpr static int watchdogPort = 5505;

// Taken from watchdog/watchdog.py
enum class DashcamState : uint8_t {
    DEAD = 0,
    ERROR = 1,
    STARTING = 2,
    RECORDING = 3,
    FALLING_BEHIND = 4,
    CONVERTING = 5,
    UPLOADING = 6
};

bool initializeStatus();
void shutdownStatus();
void setState(const DashcamState state);
