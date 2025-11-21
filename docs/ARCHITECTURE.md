# Arduino Portenta MQTT Controller - System Architecture

## 1. Architecture Overview

### 1.1 High-Level Architecture
```
┌──────────────────────────────────────────────────────────────┐
│                         External Systems                      │
│  ┌─────────────────┐  ┌──────────────┐  ┌────────────────┐  │
│  │   Web Browser   │  │ MQTT Broker  │  │  WiFi Router   │  │
│  └────────┬────────┘  └──────┬───────┘  └───────┬────────┘  │
└───────────┼──────────────────┼──────────────────┼───────────┘
            │                  │                  │
┌───────────┼──────────────────┼──────────────────┼───────────┐
│           │                  │                  │            │
│  ┌────────▼────────┐  ┌──────▼───────┐  ┌──────▼────────┐  │
│  │   Web Server    │  │ MQTT Client  │  │ WiFi Manager  │  │
│  │   (HTTP/WS)     │  │              │  │  (STA/AP)     │  │
│  └────────┬────────┘  └──────┬───────┘  └───────┬───────┘  │
│           │                  │                  │            │
│  ┌────────▼──────────────────▼──────────────────▼────────┐  │
│  │              Application Core Controller               │  │
│  │  ┌─────────────┐  ┌──────────────┐  ┌──────────────┐  │  │
│  │  │   Config    │  │ I/O Manager  │  │Event Manager │  │  │
│  │  │  Manager    │  │              │  │              │  │  │
│  │  └─────────────┘  └──────────────┘  └──────────────┘  │  │
│  └───────────────────────────┬───────────────────────────┘  │
│                              │                               │
│  ┌───────────────────────────▼───────────────────────────┐  │
│  │               Hardware Abstraction Layer               │  │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────┐  │  │
│  │  │Digital IO│  │Analog IO │  │   WiFi   │  │Flash │  │  │
│  │  └──────────┘  └──────────┘  └──────────┘  └──────┘  │  │
│  └────────────────────────────────────────────────────────┘  │
│                     Arduino Portenta Hardware                 │
└──────────────────────────────────────────────────────────────┘
```

### 1.2 Component Responsibilities

#### Application Layer
- **Main Controller**: Orchestrates all system components
- **Configuration Manager**: Handles all system configurations
- **I/O Manager**: Manages all input/output operations
- **Event Manager**: Handles event-driven architecture

#### Service Layer  
- **Network Service**: WiFi connectivity and AP management
- **MQTT Service**: MQTT broker communication
- **Web Service**: HTTP server and WebSocket handling
- **Persistence Service**: Configuration and data storage

#### HAL (Hardware Abstraction Layer)
- **Pin Handlers**: Abstract hardware pin operations
- **Storage Handler**: Abstract flash/EEPROM operations
- **Network HAL**: Abstract WiFi hardware interface

## 2. Class Design

### 2.1 Core Classes Hierarchy
```
IComponent (Interface)
├── NetworkManager
│   ├── WiFiStationManager
│   └── WiFiAccessPointManager
├── MqttManager
│   ├── MqttPublisher
│   └── MqttSubscriber
├── IoController
│   ├── DigitalPinHandler
│   ├── AnalogPinHandler
│   └── RtdPinHandler
├── WebServer
│   ├── ApiHandler
│   └── WebSocketHandler
└── ConfigurationManager
    ├── NetworkConfig
    ├── MqttConfig
    └── IoConfig

IObserver (Interface)
├── MqttEventObserver
├── IoStateObserver
└── NetworkStateObserver

ISubject (Interface)
├── IoSubject
├── NetworkSubject
└── MqttSubject
```

### 2.2 Key Design Patterns

#### Factory Pattern
```cpp
class PinHandlerFactory {
public:
    static std::unique_ptr<IPinHandler> create(PinType type, uint8_t pin) {
        switch(type) {
            case PinType::DIGITAL_INPUT:
                return std::make_unique<DigitalInputHandler>(pin);
            case PinType::ANALOG_INPUT:
                return std::make_unique<AnalogInputHandler>(pin);
            // ... other types
        }
    }
};
```

#### Observer Pattern
```cpp
class IObserver {
public:
    virtual void update(const Event& event) = 0;
};

class ISubject {
public:
    virtual void attach(IObserver* observer) = 0;
    virtual void detach(IObserver* observer) = 0;
    virtual void notify(const Event& event) = 0;
};
```

#### Singleton Pattern
```cpp
class SystemManager {
private:
    static SystemManager* instance;
    SystemManager() = default;
    
public:
    static SystemManager* getInstance() {
        if (!instance) {
            instance = new SystemManager();
        }
        return instance;
    }
};
```

## 3. Data Flow

### 3.1 Input Data Flow
```
Physical Input → HAL → Pin Handler → I/O Controller → Event Manager
                                                           ↓
Web Interface ← API Handler ← Web Server ← ─ ─ ─ ─ ─ MQTT Manager
```

### 3.2 Configuration Flow
```
Web UI → REST API → Config Manager → Persistence → Flash Storage
                           ↓
                    Component Update → Runtime Configuration
```

### 3.3 MQTT Communication Flow
```
I/O State Change → Event → MQTT Manager → Publish → MQTT Broker
                                ↑
MQTT Broker → Subscribe → Command Handler → I/O Controller
```

## 4. State Management

### 4.1 System States
```cpp
enum class SystemState {
    INITIALIZING,     // System startup
    AP_MODE,          // Access Point mode for configuration
    CONNECTING,       // Attempting network connection
    OPERATIONAL,      // Normal operation
    ERROR,           // Error state
    MAINTENANCE      // Firmware update or configuration
};
```

### 4.2 State Transitions
```
INITIALIZING → AP_MODE (no config)
            ↓
INITIALIZING → CONNECTING (has config)
            ↓
      OPERATIONAL ← → ERROR
            ↓
      MAINTENANCE
```

## 5. Memory Architecture

### 5.1 Memory Layout
```
┌─────────────────────┐ 0x00000000
│    Boot Loader      │
├─────────────────────┤ 0x00010000
│  Application Code   │
├─────────────────────┤ 0x00100000
│   Web Resources     │
├─────────────────────┤ 0x00180000
│   Configuration     │
├─────────────────────┤ 0x001C0000
│    Error Logs       │
├─────────────────────┤ 0x001E0000
│     Reserved        │
└─────────────────────┘ 0x00200000

RAM Usage:
├── Static Allocations (40%)
├── Heap (40%)
├── Stack (15%)
└── Reserved (5%)
```

### 5.2 Object Pools
```cpp
template<typename T, size_t Size>
class ObjectPool {
private:
    std::array<T, Size> pool;
    std::bitset<Size> used;
    
public:
    T* allocate();
    void deallocate(T* obj);
};
```

## 6. Communication Protocols

### 6.1 Internal Communication
- **Event Bus**: Publish-subscribe for loose coupling
- **Direct Call**: Time-critical operations
- **Message Queue**: Buffered async communication

### 6.2 External Communication
- **HTTP/1.1**: Web interface and REST API
- **WebSocket**: Real-time updates
- **MQTT 3.1.1/5.0**: IoT communication
- **mDNS**: Device discovery

## 7. Security Architecture

### 7.1 Security Layers
```
Application Security
├── Authentication (Web UI)
├── Authorization (API Access)
└── Input Validation

Communication Security
├── TLS/SSL (MQTT & HTTPS)
├── WiFi Security (WPA2/3)
└── Token Management

Data Security
├── Configuration Encryption
├── Secure Boot (future)
└── Secure Storage
```

### 7.2 Security Implementation
```cpp
class SecurityManager {
public:
    bool authenticateUser(const String& username, const String& password);
    String generateToken(const String& userId);
    bool validateToken(const String& token);
    void encryptData(uint8_t* data, size_t length);
    void decryptData(uint8_t* data, size_t length);
};
```

## 8. Scalability Considerations

### 8.1 Modular Design
- Plugin architecture for I/O handlers
- Protocol adapters for future protocols
- Extensible configuration system

### 8.2 Performance Optimization
- Interrupt-driven I/O handling
- DMA for data transfers
- Async processing for network operations
- Event batching for efficiency

## 9. Deployment Architecture

### 9.1 Docker Build Environment
```dockerfile
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y \
    python3 pip git \
    && pip install platformio
WORKDIR /app
CMD ["platformio", "run"]
```

### 9.2 CI/CD Pipeline
```
Source Code → Git Push → Docker Build → Unit Tests → Integration Tests
                                             ↓
                                    Firmware Binary → Release
```

## 10. Monitoring and Diagnostics

### 10.1 System Metrics
- CPU usage
- Memory utilization
- Network statistics
- I/O operation counts
- Error rates

### 10.2 Diagnostic Features
```cpp
class DiagnosticsManager {
public:
    SystemMetrics getMetrics();
    void runSelfTest();
    void generateReport();
    void enableDebugMode(DebugLevel level);
};
```

## 11. Future Architecture Extensions

### 11.1 Planned Extensions
- Modbus support module
- Cloud connector module
- Edge computing framework
- Machine learning inference

### 11.2 Extension Points
- Protocol adapters
- I/O handler plugins
- Authentication providers
- Storage backends