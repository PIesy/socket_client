#include "connection.h"
#include <iostream>
#include "../spolks1/helpers.h"

Connection::Connection(const std::string& ip, unsigned short port, SocketType type)
{
    this->ip = ip;
    this->port = port;
    this->type = type;
    if (type == SocketType::TCP)
    {
        socket.Create(SocketType::TCP);
        socket.Connect(ip, port);
        socket.SetReadTimeout(1);
        socket.SetWriteTimeout(1);
        client = new TCPClient(socket);
    }
    else
    {
        socket.Create(SocketType::UDP);
        socket.SetReadTimeout(5);
        client = new UDPClient(socket, ip, port);
    }
    sockets::setSendBuffSize(socket.getSocket(), 2000000);
    std::cerr << "Buff size: " << sockets::getSendBuffSize(socket.getSocket()) << std::endl;
}

Connection::~Connection()
{
    delete client;
}

size_t Connection::getMaxPackageSize()
{
    if (type == SocketType::TCP)
        return 2000000;
    return 1400;
}

bool Connection::Reconnect()
{
    if (!client->isReachable())
    {
        if (asBool(client->getSocket().Connect(ip, port)))
            onReconnectCallback(*this);
        else
        {
            std::cerr << "Cannot reconnect" << std::endl;
            return false;
        }
    }
    return true;
}

bool Connection::IsConnected()
{
    return client->isReachable();
}

void Connection::setOnReconnectCallback(const std::function<void(Connection&)>& callback)
{
    onReconnectCallback = callback;
}

OperationResult Connection::repeatIfFailed(std::function<OperationResult()> task)
{
    size_t time = 0;
    size_t reconnects = 0;
    OperationResult result = OperationResult::PartiallyFinished;
    bool connected = true;

    while (reconnects < reconnectTries)
    {
        time = 0;
        if (connected)
            while (time < timeoutWait)
            {
                result = task();
                while (result == OperationResult::Timeout || result == OperationResult::PartiallyFinished)
                    result = task();
                if (result == OperationResult::Success)
                    return result;
                std::cerr << "Got error retrying " << helpers::integral(result) << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(1));
                time++;
            }
        connected = Reconnect();
        std::cerr << "Reconnecting" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        reconnectTries++;
    }
    return result;
}

OperationResult Connection::Send(Buffer& data, size_t size)
{
    return repeatIfFailed([&]() { return client->Send(size, data); });
}

OperationResult Connection::getData(Buffer& buff, size_t size)
{
    return repeatIfFailed([&]() { return client->Recieve(size, buff); });
}

std::string Connection::getLine()
{
    char tmp[1001] = {0};
    std::string message = "";

    repeatIfFailed([&]() {
        if (client->Recieve(65000, false) == OperationResult::Error)
            return OperationResult::Error;
        client->endTransfer();
        client->getState().message << (char*)client->getState().buff.getData();
        client->getState().buff.Clear(true);
        memset(tmp, 0, 1000);
        client->getState().message.getline(tmp, 1000, '\n');
        if (client->getState().message.eof())
        {
            client->getState().message.clear();
            return OperationResult::PartiallyFinished;
        }
        message = tmp;
        return OperationResult::Success;
    });
    return message;
}
