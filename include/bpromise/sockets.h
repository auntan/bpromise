#pragma once

#if defined(_WIN32)
#   include <winsock2.h>
#elif defined(unix) || defined(__unix__) || defined(__unix)
#   include <sys/types.h>
#   include <sys/socket.h>
namespace BPromise { using SOCKET = int; }
#else
#   error "unknown platform"
#endif
#include <string>
#include "bpromise/future.h"

namespace BPromise
{

class ServerSocket;

class ConnectedSocket
{
public:
    ConnectedSocket() = default;
    ~ConnectedSocket();

    ConnectedSocket(ConnectedSocket&& other);
    ConnectedSocket& operator=(ConnectedSocket&& other);

    int port() const { return _port; }
    BPromise::Future<int, std::string> read();
    BPromise::Future<int> send(std::string data);
    BPromise::Future<> close();

private:
    friend class ServerSocket;

    ConnectedSocket(SOCKET socket, int port) :
        _socket(socket),
        _port(port)
    {
    }

private:
    SOCKET _socket = 0;
    int _port = 0;
    std::mutex _mutex;
};

class ServerSocket
{
public:
    ServerSocket(int port);
    ~ServerSocket();

    ServerSocket(ServerSocket&& other);
    ServerSocket& operator=(ServerSocket&& other);

    BPromise::Future<ConnectedSocket> accept();

private:
    SOCKET _socket = 0;
};

}