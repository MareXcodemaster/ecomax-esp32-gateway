#pragma once
// Generated from PyPlumIO 033c15781, MIT; see THIRD_PARTY.md.
namespace protocol {
inline const char *reference_name(unsigned type) {
    switch (type) {
        case 24: return "REQUEST_STOP_MASTER";
        case 25: return "REQUEST_START_MASTER";
        case 48: return "REQUEST_CHECK_DEVICE";
        case 49: return "REQUEST_ECOMAX_PARAMETERS";
        case 50: return "REQUEST_MIXER_PARAMETERS";
        case 51: return "REQUEST_SET_ECOMAX_PARAMETER";
        case 52: return "REQUEST_SET_MIXER_PARAMETER";
        case 54: return "REQUEST_SCHEDULES";
        case 55: return "REQUEST_SET_SCHEDULE";
        case 56: return "REQUEST_ECOMAX_PARAMETER_CHANGES";
        case 57: return "REQUEST_UID";
        case 58: return "REQUEST_PASSWORD";
        case 59: return "REQUEST_ECOMAX_CONTROL";
        case 61: return "REQUEST_ALERTS";
        case 64: return "REQUEST_PROGRAM_VERSION";
        case 85: return "REQUEST_REGULATOR_DATA_SCHEMA";
        case 92: return "REQUEST_THERMOSTAT_PARAMETERS";
        case 93: return "REQUEST_SET_THERMOSTAT_PARAMETER";
        case 176: return "RESPONSE_DEVICE_AVAILABLE";
        case 177: return "RESPONSE_ECOMAX_PARAMETERS";
        case 178: return "RESPONSE_MIXER_PARAMETERS";
        case 179: return "RESPONSE_SET_ECOMAX_PARAMETER";
        case 180: return "RESPONSE_SET_MIXER_PARAMETER";
        case 182: return "RESPONSE_SCHEDULES";
        case 185: return "RESPONSE_UID";
        case 186: return "RESPONSE_PASSWORD";
        case 187: return "RESPONSE_ECOMAX_CONTROL";
        case 189: return "RESPONSE_ALERTS";
        case 192: return "RESPONSE_PROGRAM_VERSION";
        case 212: return "RESPONSE_ECOMAX_PARAMETER_CHANGES";
        case 213: return "RESPONSE_REGULATOR_DATA_SCHEMA";
        case 220: return "RESPONSE_THERMOSTAT_PARAMETERS";
        case 221: return "RESPONSE_SET_THERMOSTAT_PARAMETER";
        case 8: return "MESSAGE_REGULATOR_DATA";
        case 53: return "MESSAGE_SENSOR_DATA";
        default: return nullptr;
    }
}
}
