#pragma once
#include "Arduino.h"
#include "Client.h"
#include <functional>
#include <string>
#include <vector>

class PubSubClient {
public:
    // Callback signature
    using Callback = std::function<void(char*, uint8_t*, unsigned int)>;

    PubSubClient() : client_(nullptr), connected_(false) {}
    PubSubClient(Client& client) : client_(&client), connected_(false) {}

    PubSubClient& setServer(const char* ip, uint16_t port) {
        serverIp_ = ip;
        serverPort_ = port;
        return *this;
    }

    PubSubClient& setCallback(Callback cb) {
        callback_ = cb;
        return *this;
    }

    PubSubClient& setClient(Client& client) {
        client_ = &client;
        return *this;
    }

    bool connect(const char* id, const char* user, const char* pass, const char* willTopic, uint8_t willQos, bool willRetain, const char* willMessage) {
        clientId_ = id;
        username_ = user;
        password_ = pass;
        connected_ = true;
        return true;
    }

    bool connect(const char* id) {
        clientId_ = id;
        connected_ = true;
        return true;
    }

    void disconnect() {
        connected_ = false;
    }

    bool connected() {
        return connected_;
    }

    bool loop() {
        return true;
    }

    int state() {
        return connected_ ? 0 : -1;
    }

    bool publish(const char* topic, const char* payload, bool retained = false) {
        lastTopic_ = topic;
        lastPayload_ = payload;
        lastRetained_ = retained;
        return true;
    }

    bool subscribe(const char* topic) {
        subscriptions_.push_back(topic);
        return true;
    }

    // Test Helpers
    std::string serverIp_;
    uint16_t serverPort_;
    std::string clientId_;
    std::string username_;
    std::string password_;
    std::string lastTopic_;
    std::string lastPayload_;
    bool lastRetained_;
    std::vector<std::string> subscriptions_;
    bool connected_;
    Client* client_;
    Callback callback_;
};
