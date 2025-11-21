#pragma once
#include "IO/IPinHandler.h"
#include <vector>
#include <ArduinoJson.h>

namespace IO {

class IoController {
public:
    static IoController& getInstance();
    
    std::vector<uint16_t> getAllPinNumbers() const;
    IPinHandler* getPin(uint16_t compositeKey);
    bool isHealthy() const { return true; }

    // Test helper to add a mock pin
    void addMockPin(uint16_t key, IPinHandler* handler);

private:
    IoController() = default;
    std::map<uint16_t, IPinHandler*> mockPins_;
};

}
