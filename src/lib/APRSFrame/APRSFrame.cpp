#include "APRSFrame.h"

APRSFrame::APRSFrame() :
    type(FrameType::Unknown)
{
}

APRSFrame::APRSFrame(const String& frame) :
    rawFrame(frame),
    type(FrameType::Unknown)
{
    parse();
}

APRSFrame::~APRSFrame()
{
}

void APRSFrame::setRaw(const String& frame)
{
    rawFrame = frame;
    type = FrameType::Unknown;

    source = "";
    destination = "";
    path = "";
    message = "";
    addressee = "";

    latitude = 0.0;
    longitude = 0.0;
    altitudeFeet = -1;
    altitudeMetre = 0.0;

    hasPosition = false;
    hasAltitude = false;

    symbolTable = ' ';
    symbolCode = ' ';

    parse();
}

void APRSFrame::parse()
{
    int gtPos = rawFrame.indexOf('>');
    int colonPos = rawFrame.indexOf(':');

    if (gtPos < 0 || colonPos < 0 || colonPos <= gtPos)
        return;

    source = rawFrame.substring(0, gtPos);

    String destAndPath = rawFrame.substring(gtPos + 1, colonPos);
    int commaPos = destAndPath.indexOf(',');

    if (commaPos >= 0) {
        destination = destAndPath.substring(0, commaPos);
        path = destAndPath.substring(commaPos + 1);
    } else {
        destination = destAndPath;
    }

    String payload = rawFrame.substring(colonPos + 1);

    if (payload.length() == 0)
        return;

    char c = payload[0];

    if (c == '!' || c == '=' || c == '/' || c == '@') {

        type = FrameType::Position;

        if (isCompressed(payload))
            parseCompressedPosition(payload);
        else
            parseUncompressedPosition(payload);

    } else if (c == '>') {

        type = FrameType::Status;

    } else if (c == '_') {

        type = FrameType::Weather;

    } else if (payload.startsWith("T#")) {

        type = FrameType::Telemetry;

    } else if (c == ':' && payload.length() >= 11 && payload[10] == ':') {

        type = FrameType::Message;

        addressee = payload.substring(1, 10);
        rtrim(addressee);

        message = payload.substring(11);
        rtrim(message);

    } else {

        type = FrameType::Unknown;
    }
}

void APRSFrame::print() const
{
    Serial.println();
    Serial.print("Raw : ");
    Serial.println(rawFrame);

    Serial.print("Source        : ");
    Serial.println(source);

    Serial.print("Destination   : ");
    Serial.println(destination);

    Serial.print("Path          : ");
    Serial.println(path);

    Serial.print("Type de trame : ");
    Serial.println(typeToString(type));

    if (type == FrameType::Message) {
        Serial.print("Addressee     : ");
        Serial.println(addressee);

        Serial.print("Message       : ");
        Serial.println(message);
    }

    if (hasPosition) {
        Serial.print("Symbole APRS  : ");
        Serial.print(symbolTable);
        Serial.print(symbolCode);
        Serial.print(" -> ");
        Serial.println(getSymbolDescription());

        Serial.print("Latitude      : ");
        Serial.println(latitude, 5);

        Serial.print("Longitude     : ");
        Serial.println(longitude, 5);

        if (hasAltitude) {
            Serial.print("Altitude      : ");
            Serial.print(altitudeFeet);
            Serial.print(" ft (");
            Serial.print(altitudeMetre, 1);
            Serial.println(" m)");
        }
    }

    Serial.println();
}

String APRSFrame::typeToString(FrameType type)
{
    switch (type) {
        case FrameType::Message:
            return "Message";

        case FrameType::Position:
            return "Position";

        case FrameType::Status:
            return "Status";

        case FrameType::Telemetry:
            return "Telemetry";

        case FrameType::Weather:
            return "Weather";

        default:
            return "Unknown";
    }
}

String APRSFrame::getSource() const
{
    return source;
}

String APRSFrame::getDestination() const
{
    return destination;
}

String APRSFrame::getPath() const
{
    return path;
}

String APRSFrame::getAddressee() const
{
    return addressee;
}

String APRSFrame::getMessage() const
{
    return message;
}

APRSFrame::FrameType APRSFrame::getFrameType() const
{
    return type;
}

double APRSFrame::getLatitude() const
{
    return latitude;
}

double APRSFrame::getLongitude() const
{
    return longitude;
}

double APRSFrame::getAltitude() const
{
    return altitudeMetre;
}

void APRSFrame::parseUncompressedPosition(const String& payload)
{
    hasPosition = false;
    hasAltitude = false;

    if (payload.length() < 20)
        return;

    String latStr = payload.substring(1, 9);
    String lonStr = payload.substring(10, 19);

    latitude = parseCoordinate(latStr, latStr.charAt(7));
    longitude = parseCoordinate(lonStr, lonStr.charAt(8));

    if (isnan(latitude) || isnan(longitude))
        return;

    symbolTable = payload.charAt(9);
    symbolCode = payload.charAt(19);

    hasPosition = true;

    int altPos = payload.indexOf("/A=");

    if (altPos >= 0 && altPos + 9 <= payload.length()) {

        String altStr = payload.substring(altPos + 3, altPos + 9);

        altitudeFeet = altStr.toInt();
        altitudeMetre = altitudeFeet * 0.3048;

        hasAltitude = true;
    }
}

bool APRSFrame::isCompressed(const String& payload)
{
    if (payload.length() < 13)
        return false;

    return !isdigit((unsigned char)payload.charAt(1));
}

void APRSFrame::parseCompressedPosition(const String& payload)
{
    hasPosition = false;
    hasAltitude = false;

    if (payload.length() < 11)
        return;

    symbolTable = payload.charAt(1);

    String latStr = payload.substring(2, 6);
    String lonStr = payload.substring(6, 10);

    symbolCode = payload.charAt(10);

    long latVal = base91ToDecimal(latStr);
    long lonVal = base91ToDecimal(lonStr);

    latitude = 90.0 - (latVal / 380926.0);
    longitude = -180.0 + (lonVal / 190463.0);

    hasPosition = true;
}

double APRSFrame::parseCoordinate(const String& coord, char direction)
{
    int dot = coord.indexOf('.');

    if (dot < 2)
        return NAN;

    double deg = coord.substring(0, dot - 2).toDouble();
    double min = coord.substring(dot - 2).toDouble();

    double decimal = deg + (min / 60.0);

    if (direction == 'S' || direction == 'W')
        decimal = -decimal;

    return decimal;
}

String APRSFrame::getSymbolDescription() const
{
    String key;
    key += symbolTable;
    key += symbolCode;

    if (key == "/>")
        return "Car";

    if (key == "/<")
        return "Motorcycle";

    if (key == "/b")
        return "Bicycle";

    if (key == "/O")
        return "Balloon";

    if (key == "/_")
        return "Weather station";

    if (key == "/-")
        return "House";

    if (key == "/*")
        return "Snowmobile";

    if (key == "/[")
        return "Human";

    if (key == "/k")
        return "Truck";

    if (key == "/r")
        return "Repeater tower";

    if (key == "/s")
        return "Ship, power boat";

    if (key == "/Y")
        return "Sailboat";

    if (key == "/v")
        return "Van";

    if (key == "\\O")
        return "Rocket";

    if (key == "\\-")
        return "House, HF antenna";

    if (key == "\\^")
        return "Aircraft";

    if (key == "\\s")
        return "Ship, boat";

    if (key == "\\S")
        return "Satellite";

    if (key == "L#")
        return "Digipeater, green star + L";

    if (key == "La")
        return "Red diamond + L";

    if (key == "L_")
        return "Weather site + L";

    if (key == "L&")
        return "Gateway station + L";

    if (key == "R&")
        return "Gateway station + R";

    if (key == "D&")
        return "Gateway station + D";

    return "Symbole inconnu (" + key + ")";
}

void APRSFrame::rtrim(String& s)
{
    while (s.length() > 0 &&
           isspace((unsigned char)s.charAt(s.length() - 1))) {
        s.remove(s.length() - 1);
    }
}

long APRSFrame::base91ToDecimal(const String& str)
{
    long value = 0;

    for (size_t i = 0; i < str.length(); i++)
        value = value * 91 + (str.charAt(i) - 33);

    return value;
}

