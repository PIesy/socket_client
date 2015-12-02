#include <iostream>
#include <thread>
#include "commandinterpreter.h"
#include "connection.h"
#include "../spolks1/helpers.h"

int main(int argc, char** argv)
{
    sockets::init();
    std::string str;
    srand(time(nullptr));
    int clientID = rand() % 100;
    long port = 2115;
    std::string ip = "127.0.0.1";
    std::cerr << "Id: " << clientID << std::endl;
    SocketType type = SocketType::UDP;
    if (argc > 2)
    {
        ip = argv[1];
        port = std::strtol(argv[2], nullptr, 10);
    }
    if (argc > 3)
    {
        if (std::string(argv[3]) == "udp")
            type = SocketType::UDP;
        else
            type = SocketType::TCP;
    }
    bool cmdMode = type == SocketType::UDP;
    Connection con(ip, port, type);
    CommandInterpreter interp(con, clientID);
    Header id;
    id.id = clientID;
    id.packageSize = sizeof(id);
    id.command = helpers::integral(ServerCommand::Identify);

    int i = 0;
    while (!con.IsConnected())
    {
        if (!con.Reconnect())
        {
            std::cerr << "Cannot connect to server: " << ip << ":" << port << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            i++;
        }
        if (i == 100)
            return -1;
    }
    if (type == SocketType::UDP)
    {
        Buffer buff(sizeof(RedirectResponce));
        con.Send(id);
        con.getData(buff, buff.getSize());
        RedirectResponce& responce = buff;
        con.setIp(responce.newIp);
        con.setPort(responce.newPort);
    }
    while (1)
    {
        std::getline(std::cin, str);
        if (str == "exit")
            break;
        if (cmdMode)
            std::cerr << interp.Interpret(str) << std::endl;
        if (!cmdMode)
        {
            str = str + "\r\n";
            Buffer buff(str.c_str(), str.length());
            con.Send(buff, str.length());
            std::cout << con.getLine() << std::endl;
        }
        if (str == "!echo")
            cmdMode = false;
        if (str == "!cmd\r\n")
        {
            cmdMode = true;
            if (type == SocketType::TCP)
            {
                con.Send(id);
                con.setOnReconnectCallback([id](Connection& con) { con.Send(id); });
            }
        }
    }
    sockets::end();
    return 0;
}

