#include "IO/IoController.h"

namespace IO {

IoController& IoController::getInstance() {
    static IoController instance;
    return instance;
}

std::vector<uint16_t> IoController::getAllPinNumbers() const {
    std::vector<uint16_t> keys;
    for(auto const& pair : mockPins_) {
        keys.push_back(pair.first);
    }
    return keys;
}

IPinHandler* IoController::getPin(uint16_t compositeKey) {
    if (mockPins_.find(compositeKey) != mockPins_.end()) {
        return mockPins_[compositeKey];
    }
    return nullptr;
}

void IoController::addMockPin(uint16_t key, IPinHandler* handler) {
    mockPins_[key] = handler;
}

}
