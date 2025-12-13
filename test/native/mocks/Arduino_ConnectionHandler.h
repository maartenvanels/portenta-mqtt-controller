#pragma once

#include "Client.h"
#include "Udp.h"

// Minimal stub of Arduino_ConnectionHandler for native unit tests / IDE indexing.
// This project uses the real Arduino_ConnectionHandler library on target builds.

enum class NetworkAdapter {
  NONE,
  WIFI,
  ETHERNET
};

enum class NetworkConnectionState {
  INIT,
  CONNECTING,
  CONNECTED,
  DISCONNECTING,
  DISCONNECTED,
  ERROR
};

enum class NetworkConnectionEvent {
  CONNECTED,
  DISCONNECTED,
  ERROR
};

using OnNetworkEventCallback = void (*)();

class ConnectionHandler {
public:
  virtual ~ConnectionHandler() = default;

  virtual NetworkConnectionState check() { return NetworkConnectionState::CONNECTED; }

  virtual Client &getClient() { return _client; }
  virtual UDP &getUDP() { return _udp; }

  NetworkAdapter getInterface() { return NetworkAdapter::WIFI; }

  virtual void addCallback(NetworkConnectionEvent /*event*/, OnNetworkEventCallback /*cb*/) {}

private:
  Client _client;
  UDP _udp;
};

class WiFiConnectionHandler : public ConnectionHandler {
public:
  WiFiConnectionHandler() = default;
  WiFiConnectionHandler(const char * /*ssid*/, const char * /*pass*/, bool /*keep_alive*/ = true) {}
};

class EthernetConnectionHandler : public ConnectionHandler {
public:
  EthernetConnectionHandler(unsigned long /*timeout*/ = 15000, unsigned long /*responseTimeout*/ = 4000, bool /*keep_alive*/ = true) {}
};


