#ifndef APRSCLIENT_H
#define APRSCLIENT_H

#include <Arduino.h>
#include <WiFi.h>
#include <functional>

#include "Position.h"

class AprsClient {
public:
    AprsClient(bool _bidirectionnel = false);
    ~AprsClient();

    void connectToServer(const String& host, int port);
    void authenticate(const String& callsign, const String& filter);
    void sendLine(const String& line);
    void sendPosition(Position& pos,bool compressed,bool altitude);
    void retransmitFrame(const String& loraFrame);

    void disconnect();

    void startListening(std::function<void(const String&)> onMessage);
    void stopListening();

    bool isConnected();

private:
    WiFiClient client;

    bool bidirectionnel;
    volatile bool connected;
    volatile bool running;

    TaskHandle_t listenerTask;
    std::function<void(const String&)> messageCallback;

    String serverHost;
    int serverPort;
    String callsign;
    String filterOption;

    String receiveAsync(uint32_t timeoutMs);
    bool reconnect();

    int computePasscode(const String& callsign);

    static void listenerTaskFunction(void* parameter);
    void listenerLoop();
};

#endif
