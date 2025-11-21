#pragma once
#include <PubSubClient.h>
#include "IO/IoController.h"

namespace HA {

class HADiscovery {
public:
    static bool publishDiscovery(PubSubClient& mqttClient, IO::IoController& ioController, const char* deviceId, const char* deviceName);
    static bool publishAvailability(PubSubClient& mqttClient, const char* deviceId, bool available);
};

}
