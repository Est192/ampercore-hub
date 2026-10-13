#pragma once

#include <Arduino.h>

struct Telemetry
{
    float voltage = 0.0f;

    float current = 0.0f;

    float power = 0.0f;

    float energy = 0.0f;

    unsigned long timestamp = 0;
};