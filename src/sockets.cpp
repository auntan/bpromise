#include "bpromise/sockets.h"
#include "bpromise/threadpool.h"

#if defined(_WIN32)
#   define IS_WIN
    using socklen_t = int;
    static int close(SOCKET s) { return closesocket(s); }
#elif defined(unix) || defined(__unix__) || defined(__unix)
#   include <netinet/in.h>
#   include <unistd.h>
#   define IS_UNIX
#endif

namespace BPromise
{

ConnectedSocket::~ConnectedSocket()
{
    if (_socket) {
        close();
    }
}

ConnectedSocket::ConnectedSocket(ConnectedSocket&& other) :
    _socket(other._socket),
    _port(other._port)
{
    other._socket = 0;
}

ConnectedSocket& ConnectedSocket::operator=(ConnectedSocket&& other)
{
    if (this != &other) {
        _socket = other._socket;
        _port = other._socket;
        other._socket = 0;
    }
    return *this;
}

BPromise::Future<> ConnectedSocket::close()
{
    auto promise = std::make_shared<BPromise::Promise<>>();
    auto future = promise->get_future();

    BPromise::ThreadPool::set_immediate([socket = _socket, promise = std::move(promise)]() mutable {
        ::close(socket);
        BPromise::MainThread::set_immediate([promise = std::move(promise)]() mutable {
            promise->set_value();
        });
    });

    _socket = 0;

    return future;
}

BPromise::Future<int> ConnectedSocket::send(std::string data)
{
    auto promise = std::make_shared<BPromise::Promise<int>>();
    auto future = promise->get_future();

    BPromise::ThreadPool::set_immediate([socket = _socket, data = std::move(data), promise = std::move(promise)]() mutable {
        int result = ::send(socket, data.data(), data.size(), 0);
        BPromise::MainThread::set_immediate([promise = std::move(promise), result]() mutable {
            promise->set_value(result);
        });
    });

    return future;
}

BPromise::Future<int, std::string> ConnectedSocket::read()
{
    auto promise = std::make_shared<BPromise::Promise<int, std::string>>();
    auto future = promise->get_future();

    BPromise::ThreadPool::set_immediate([socket = _socket, promise = std::move(promise)]() mutable {
        char buffer[1024];
        int result = recv(socket, buffer, sizeof(buffer), 0);
        std::string data(buffer, result ? result : 0);
        BPromise::MainThread::set_immediate([promise = std::move(promise), result, data = std::move(data)]() mutable {
            promise->set_value(result, std::move(data));
        });
    });

    return future;
}

ServerSocket::ServerSocket(int port)
{
#if defined(_WIN32)
    WSADATA WSAData;
    WSAStartup(MAKEWORD(2, 0), &WSAData);
#endif

    _socket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddr;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    ::bind(_socket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    ::listen(_socket, 0);
}

ServerSocket::~ServerSocket()
{
    if (_socket) {
        ::close(_socket);

#if defined(_WIN32)
        WSACleanup();
#endif
    }
}

ServerSocket::ServerSocket(ServerSocket&& other) :
    _socket(other._socket)
{
    other._socket = 0;
}

ServerSocket & ServerSocket::operator=(ServerSocket&& other)
{
    if (this != &other) {
        _socket = other._socket;
        other._socket = 0;
    }
    return *this;
}

BPromise::Future<ConnectedSocket> ServerSocket::accept()
{
    auto promise = std::make_shared<BPromise::Promise<ConnectedSocket>>();
    auto future = promise->get_future();

    BPromise::ThreadPool::set_immediate([socket = _socket, promise = std::move(promise)]() mutable {
        sockaddr_in clientAddr;
        socklen_t clientAddrSize = sizeof(sockaddr_in);
        if (SOCKET clientSocket = ::accept(socket, (sockaddr*)&clientAddr, &clientAddrSize); clientSocket != -1)
        {
            BPromise::MainThread::set_immediate([promise = std::move(promise), clientSocket, port = clientAddr.sin_port]() mutable {
                ConnectedSocket client(clientSocket, port);
                promise->set_value(std::move(client));
            });
        }
    });

    return future;
}

}