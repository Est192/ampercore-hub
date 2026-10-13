#pragma once

#include <Arduino.h>

#include "Telemetry.h"

struct Device
{
    String id;

    String name;

    String type;

    String state =
        "online";

    Telemetry telemetry;
};