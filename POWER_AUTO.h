#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <MD5.h>
#include <PubSubClient.h>
#include <string.h>

// ===================== Cấu hình =====================
#define KEY "control_power" // Key MD5

extern const char data_MQTT[100];

inline float powerJsonRoundToDecimals(float value, uint8_t decimals)
{
    float factor = 1.0f;
    for (uint8_t i = 0; i < decimals; ++i)
    {
        factor *= 10.0f;
    }

    if (factor <= 0.0f)
    {
        return value;
    }

    if (value >= 0.0f)
    {
        return (long)(value * factor + 0.5f) / factor;
    }

    return (long)(value * factor - 0.5f) / factor;
}

inline void powerJsonTrimLeadingSpaces(char *buffer)
{
    if (buffer == nullptr)
    {
        return;
    }

    size_t length = strlen(buffer);
    size_t index = 0;
    while (index < length && buffer[index] == ' ')
    {
        ++index;
    }

    if (index == 0)
    {
        return;
    }

    for (size_t i = index; i <= length; ++i)
    {
        buffer[i - index] = buffer[i];
    }
}

inline void powerJsonFormatFloat(float value, uint8_t decimals, char *buffer, size_t bufferSize)
{
    if (buffer == nullptr || bufferSize == 0)
    {
        return;
    }

    dtostrf(value, 0, decimals, buffer);
    powerJsonTrimLeadingSpaces(buffer);

    if (buffer[0] == '\0' && bufferSize > 1U)
    {
        buffer[0] = '0';
        buffer[1] = '\0';
    }
}

// Tính MD5 và trả về chuỗi hex 32 ký tự (chữ thường)
inline String calculateMd5Hex(const String &input)
{
    unsigned char* digest = MD5::make_hash(input.c_str()); // 16 byte
    char* hex = MD5::make_digest(digest, 16);              // 32 ký tự hex
    String out(hex);
    free(digest);
    free(hex);
    return out;
}

inline String signState(const char *dev, int ch, const char *mode, int on, const String &V,
                        const String &A, const String &kWh, uint32_t seq, uint32_t ts)
{
    if (dev == nullptr || mode == nullptr)
    {
        return String();
    }

    String payload;
    payload.reserve(96);
    payload += dev;
    payload += '|';
    payload += ch;
    payload += '|';
    payload += mode;
    payload += '|';
    payload += on;
    payload += '|';
    payload += V;
    payload += '|';
    payload += A;
    payload += '|';
    payload += kWh;
    payload += '|';
    payload += seq;
    payload += '|';
    payload += ts;

    return calculateMd5Hex(payload);
}

inline bool powerJsonBuildAutoPayload(const char *deviceId, uint8_t outputNumber, const char *mode, int onState,
                                      int voltage, float current, float energy, uint32_t seq, unsigned long timestamp,
                                      char *outBuffer, size_t outBufferSize)
{
    if (deviceId == nullptr || mode == nullptr || outBuffer == nullptr || outBufferSize == 0U)
    {
        return false;
    }

    StaticJsonDocument<256> doc;

    float currentRounded = powerJsonRoundToDecimals(current, 1);
    float energyRounded = powerJsonRoundToDecimals(energy, 2);

    char currentBuffer[16];
    char energyBuffer[16];
    powerJsonFormatFloat(currentRounded, 1, currentBuffer, sizeof(currentBuffer));
    powerJsonFormatFloat(energyRounded, 2, energyBuffer, sizeof(energyBuffer));

    doc["Device_ID"] = deviceId;
    doc["OutPut"] = static_cast<unsigned int>(outputNumber);
    doc["Mode"] = mode;
    doc["On"] = onState;
    doc["V"] = voltage;
    doc["A"] = serialized(currentBuffer);
    doc["Kwh"] = serialized(energyBuffer);
    doc["Seq"] = seq;
    doc["Time"] = timestamp;

    String voltageString = String(voltage);
    String currentString(currentBuffer);
    String energyString(energyBuffer);

    String signature = signState(deviceId, static_cast<int>(outputNumber), mode, onState, voltageString, currentString, energyString,
                                 seq, static_cast<uint32_t>(timestamp));

    // data_MQTT = signature;    
    sprintf(data_MQTT, "%s",signature);                         
    if (signature.length() == 0)
    {
        return false;
    }

    doc["KEY"] = signature;

    size_t written = serializeJson(doc, outBuffer, outBufferSize);
    return written > 0U;
}

inline bool powerJsonPublishAutoStatus(PubSubClient &client, const char *topic, const char *deviceId, const int *relayStates,
                                       uint8_t relayCount, const int *voltages, size_t measurementCount, const float *currents,
                                       const float *energies, uint32_t *sequence, unsigned long (*timeProvider)(), bool isAutoMode)
{
    if (!isAutoMode)
    {
        return false;
    }

    if (topic == nullptr || topic[0] == '\0' || deviceId == nullptr || relayStates == nullptr || voltages == nullptr ||
        currents == nullptr || energies == nullptr || sequence == nullptr || timeProvider == nullptr)
    {
        return false;
    }

    bool published = false;

    for (uint8_t index = 0; index < relayCount; ++index)
    {
        size_t measurementIndex = static_cast<size_t>(index) + 1U; // Skip aggregate channel at index 0
        if (measurementIndex >= measurementCount)
        {
            continue;
        }

        int onState = (relayStates[index] == HIGH) ? 1 : 0;
        int voltage = voltages[measurementIndex];
        float current = currents[measurementIndex];
        float energy = energies[measurementIndex];

        if (current != current)
        {
            current = 0.0f;
        }
        if (energy != energy)
        {
            energy = 0.0f;
        }

        uint32_t seq = ++sequence[index];
        unsigned long timestamp = timeProvider();

        char jsonBuffer[256];
        if (powerJsonBuildAutoPayload(deviceId,
                                      static_cast<uint8_t>(measurementIndex),
                                      isAutoMode ? "AUTO" : "MAN",
                                      onState,
                                      voltage,
                                      current,
                                      energy,
                                      seq,
                                      timestamp,
                                      jsonBuffer,
                                      sizeof(jsonBuffer)))
        {
            client.publish(topic, jsonBuffer);
            published = true;
        }
    }

    return published;
}