#ifndef APRSFRAME_H
#define APRSFRAME_H

#include <Arduino.h>

class APRSFrame {
public:

    enum class FrameType {
        Message,
        Position,
        Status,
        Telemetry,
        Weather,
        Unknown
    };

    APRSFrame();
    APRSFrame(const String& frame);
    ~APRSFrame();

    void setRaw(const String& frame);

    String getSource() const;
    String getDestination() const;
    String getPath() const;
    String getAddressee() const;
    String getMessage() const;

    double getLatitude() const;
    double getLongitude() const;
    double getAltitude() const;

    String getSymbolDescription() const;
    FrameType getFrameType() const;

    void print() const;

    static String typeToString(FrameType type);
    static double parseCoordinate(const String& coord, char direction);
    static void rtrim(String& s);
    static long base91ToDecimal(const String& str);

private:
    void parse();
    void parseUncompressedPosition(const String& payload);
    bool isCompressed(const String& payload);
    void parseCompressedPosition(const String& payload);

    String rawFrame;
    FrameType type;

    String source;
    String destination;
    String path;

    String addressee;
    String message;

    double latitude = 0.0;
    double longitude = 0.0;
    bool hasPosition = false;

    char symbolTable = ' ';
    char symbolCode = ' ';

    int altitudeFeet = -1;
    double altitudeMetre = 0.0;
    bool hasAltitude = false;
};

#endif

