#ifndef CONNECTION_H
#define CONNECTION_H

#include "../spolks1/sockportable.h"
#include "../spolks1/socket.h"
#include "../spolks1/transfer.h"
#include "../spolks1/client.h"
#include "../spolks1/tcpclient.h"
#include "../spolks1/udpclient.h"
#include <chrono>
#include <thread>
#include <functional>

class Connection
{
    size_t timeoutWait = 10;
    size_t reconnectTries = 10;
    SocketType type;
    Socket socket;
    std::string ip;
    unsigned short port;
    Transfer transfer;
    Client* client;
    Header header;
    OperationResult repeatIfFailed(std::function<OperationResult()> task);
    std::function<void(Connection&)> onReconnectCallback = [](Connection&) {};
public:
    Connection() {}
    Connection(const std::string& ip, unsigned short port, SocketType type = SocketType::TCP);
    ~Connection();
    OperationResult Send(Buffer& data, size_t size);
    std::string getLine();
    OperationResult getData(Buffer& buff, size_t size, bool noRepeat = false);
    bool IsConnected();
    void setOnReconnectCallback(const std::function<void(Connection&)>& callback);
    size_t getMaxPackageSize();
    bool Reconnect();
    void setIp(const std::string& ip);
    void setPort(unsigned short port);

    template<typename T>
    OperationResult Send(const T& data)
    {
        Buffer buff(&data, sizeof(data));
        return Send(buff, sizeof(T));
    }

};

#endif // CONNECTION_H
