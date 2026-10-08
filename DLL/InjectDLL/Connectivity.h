#pragma once

#define BUFF_SIZE 2048
#define RAPIDJSON_HAS_STDSTRING 1

#ifdef _WIN32
#define _WINSOCKAPI_
#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <tchar.h>
#else
#include "Compat.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

typedef int SOCKET;
typedef sockaddr_in SOCKADDR_IN;
typedef sockaddr SOCKADDR;
#endif

#include <string>
#include <map>
#include <any>
#include "rapidjson/writer.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "Memory.h"

namespace Connectivity
{

    ////////////////// Client.cpp //////////////////

    class Client {

    private:
#ifdef _WIN32
        WSADATA WSAData;
#endif
        SOCKET server;
        SOCKADDR_IN addr;
        char buffer[7168];

    public:
        void connectToServer(std::string IP, std::string PORT);
        void sendMessage(std::string command, std::string message);
        void sendBytes(byte Message[7168]);
        std::string receive();
        void receiveBytes(byte* Output);
        void close();

    };

    ////////////////// NamedPipes.cpp //////////////////

    // Connection to the launcher.
    class namedPipeClass
    {
    public:
        void createServer();
        bool read(char* buffer, size_t size);
        bool write(const char* buffer, size_t size);

    private:
#ifdef _WIN32
        HANDLE hPipe;
#endif
    };

    ////////////////// Interpretation.cpp //////////////////

    std::map<int, std::map<std::string, std::any>> convertData(std::string data, bool printData = false);

    ////////////////// Serializer.cpp //////////////////

    rapidjson::Value addValueToJsonDocument(rapidjson::Document::AllocatorType& allocator, std::string data, std::string dataType);
    rapidjson::Value addMapToJsonDocument(rapidjson::Document::AllocatorType& allocator, std::map<std::string, std::string> data, std::string dataTypes);
    rapidjson::Value addVectorToJsonDocument(rapidjson::Document::AllocatorType& allocator, std::vector<std::string> data, std::string dataTypes);

    ////////////////// Deserializer.cpp //////////////////

    rapidjson::Document deserializeServerData(std::string message);

}