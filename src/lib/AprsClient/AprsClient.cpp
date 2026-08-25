#include "AprsClient.h"

AprsClient::AprsClient(bool _bidirectionnel) :
    bidirectionnel(_bidirectionnel),
    connected(false),
    running(false),
    listenerTask(nullptr),
    serverPort(0)
{
}

AprsClient::~AprsClient()
{
    stopListening();
    disconnect();
}

void AprsClient::connectToServer(const String& host, int port)
{
    if (client.connected())
        client.stop();

    serverHost = host;
    serverPort = port;

    Serial.print("[APRS] Connexion à ");
    Serial.print(host);
    Serial.print(":");
    Serial.println(port);

    if (!client.connect(host.c_str(), port)) {
        connected = false;
        throw String("Impossible de se connecter à ") + host;
    }

    connected = true;

    Serial.println("[APRS] Connecté");
}

void AprsClient::authenticate(const String& call, const String& filter)
{
    if (!connected || !client.connected())
        throw String("Non connecté au serveur APRS-IS");

    int pass = computePasscode(call);

    String login = "user " + call + " pass " + String(pass) + " vers ESP32-APRS 1.0";

    if (filter.length() > 0)
        login += " filter " + filter;

    login += "\n";

    client.print(login);

    callsign = call;
    filterOption = filter;

    Serial.print("[APRS] Authentifié en tant que ");
    Serial.print(call);
    Serial.print(" pass=");
    Serial.println(pass);
}

void AprsClient::sendLine(const String& line)
{
    if (!connected || !client.connected())
        throw String("Non connecté au serveur APRS-IS");

    String msg = line;

    if (!msg.endsWith("\n"))
        msg += "\n";

    if (client.print(msg) == 0) {
        connected = false;
        throw String("Erreur lors de l'envoi : ") + line;
    }
}

void AprsClient::sendPosition(Position& pos,bool compressed,bool altitude)
{
    String headAprs = callsign + ">APRS:";
    sendLine(headAprs + pos.getPduAprs(compressed,altitude));
}



void AprsClient::disconnect()
{
    if (client.connected())
        client.stop();

    connected = false;

    Serial.println("[APRS] Déconnecté du serveur");
}

void AprsClient::startListening(std::function<void(const String&)> onMessage)
{
    if (!connected || !client.connected())
        throw String("Non connecté au serveur APRS-IS");

    if (running)
        return;

    messageCallback = onMessage;
    running = true;

    xTaskCreatePinnedToCore(
        listenerTaskFunction,
        "AprsListener",
        8192,
        this,
        1,
        &listenerTask,
        1
    );

    Serial.println("[APRS] Thread d'écoute démarré");
}

void AprsClient::stopListening()
{
    if (!running)
        return;

    running = false;

    uint32_t timeout = millis();

    while (listenerTask != nullptr &&
           millis() - timeout < 3000) {
        delay(10);
    }

    listenerTask = nullptr;

    Serial.println("[APRS] Thread d'écoute arrêté");
}

void AprsClient::listenerTaskFunction(void* parameter)
{
    AprsClient* aprs = static_cast<AprsClient*>(parameter);

    aprs->listenerLoop();

    aprs->listenerTask = nullptr;

    vTaskDelete(nullptr);
}

void AprsClient::listenerLoop()
{
    while (running) {

        if (!connected || !client.connected()) {

            connected = false;

            Serial.println("[APRS] Connexion perdue");
            Serial.println("[APRS] Tentative de reconnexion...");

            if (reconnect()) {
                Serial.println("[APRS] Reconnecté avec succès");
            } else {
                Serial.println("[APRS] Échec reconnexion");
                delay(10000);
                continue;
            }
        }

        String data = receiveAsync(1000);

        if (data.length() > 0) {

            if (data.indexOf("disconnected") >= 0 ||
                data.indexOf("lost") >= 0) {

                connected = false;
                continue;
            }

            if (messageCallback)
                messageCallback(data);
        }

        delay(20);
    }
}

String AprsClient::receiveAsync(uint32_t timeoutMs)
{
    uint32_t start = millis();

    while (millis() - start < timeoutMs) {

        if (!client.connected()) {
            connected = false;
            return "";
        }

        if (client.available()) {

            String data = client.readStringUntil('\n');

            data.trim();

            return data;
        }

        delay(10);
    }

    return "";
}

bool AprsClient::reconnect()
{
    if (serverHost.length() == 0 || serverPort == 0)
        return false;

    disconnect();

    try {

        connectToServer(serverHost, serverPort);
        authenticate(callsign, filterOption);

        return true;

    } catch (...) {

        connected = false;
        return false;
    }
}

bool AprsClient::isConnected()
{
    return connected && client.connected();
}


int AprsClient::computePasscode(const String& input)
{
    String call = input;

    int separator = call.indexOf('-');

    if (separator >= 0)
        call = call.substring(0, separator);

    call.toUpperCase();

    int hash = 0x73E2;

    for (size_t i = 0; i < call.length(); i++) {
        hash ^= call[i] << ((i & 1) ? 0 : 8);
    }

    return hash & 0x7FFF;
}

void AprsClient::retransmitFrame(const String& loraFrame)
{
    if (!connected || !client.connected())
        throw String("Non connecté au serveur APRS-IS");

    if (loraFrame.length() == 0 ||
        loraFrame[0] == '#')
        throw String("retransmitFrame vide");

    int posGt = loraFrame.indexOf('>');
    int posColon = loraFrame.indexOf(':');

    if (posGt < 0 || posColon < 0)
        throw String("Trame invalide ignorée");

    if (loraFrame.indexOf("qAR") >= 0 ||
        loraFrame.indexOf("qAO") >= 0 ||
        loraFrame.indexOf("qAS") >= 0)
        throw String("Trame déjà marquée iGate, ignorée");

    String head = loraFrame.substring(0, posColon);
    String data = loraFrame.substring(posColon);

    String tag = bidirectionnel ? "qAO" : "qAR";

    String frameWithTag =
        head + "," +
        tag + "," +
        callsign +
        data;

    sendLine(frameWithTag);

    Serial.print("[LoRa→APRS-IS] ");
    Serial.println(frameWithTag);
}


