#ifndef PIN_HANDLER_FACTORY_H
#define PIN_HANDLER_FACTORY_H

#include "IO/IPinHandler.h"
#include <memory>

namespace IO {

class PinHandlerFactory {
public:
    static std::unique_ptr<IPinHandler> create(const PinConfiguration& config);
    static std::unique_ptr<IPinHandler> create(PinType type, uint8_t pin);
    
private:
    PinHandlerFactory() = default;
};

} // namespace IO

#endif // PIN_HANDLER_FACTORY_H