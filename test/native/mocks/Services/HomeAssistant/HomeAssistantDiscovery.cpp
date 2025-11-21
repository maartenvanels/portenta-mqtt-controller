#include "Services/HomeAssistant/HomeAssistantDiscovery.h"

namespace HA {

bool HADiscovery::publishDiscovery(PubSubClient& mqttClient, IO::IoController& ioController, const char* deviceId, const char* deviceName) {
    return true;
}

bool HADiscovery::publishAvailability(PubSubClient& mqttClient, const char* deviceId, bool available) {
    return true;
}

}
