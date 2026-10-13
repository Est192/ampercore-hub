#include <Arduino.h>

void setup()
{
    Serial.begin(115200);

    Serial.println();
    Serial.println("=================================");
    Serial.println("AMPERCORE HUB INICIADO");
    Serial.println("ESP32 Funcionando Correctamente");
    Serial.println("=================================");
}

void loop()
{
    Serial.println("Heartbeat");
    delay(5000);
}