#include "commandinterpreter.h"
#include <fstream>
#include <chrono>
#include <iostream>
#include "../spolks1/helpers.h"

constexpr size_t batchSize = 90;

CommandInterpreter::CommandInterpreter(Connection& socket, int id):socket(socket)
{
    maxPackageSize = socket.getMaxPackageSize();
    clientId = id;
    std::cerr << "Max package size: " << maxPackageSize << std::endl;
}

Command CommandInterpreter::decode(const std::string& cmd)
{
    if (cmd.length() < 5)
        return Command::Unknown;
    if (cmd.substr(0, 5) == "echo ")
        return Command::Echo;
    if (cmd.substr(0, 5) == "file ")
        return Command::File;
    return Command::Unknown;
}

std::string CommandInterpreter::Interpret(const std::string &command)
{
    Command cmd = decode(command);
    std::string result = "Invalid command";

    switch (cmd)
    {
    case Command::Echo:
        if (command.length() > 5)
            result = echo(command.substr(5));
        break;
    case Command::File:
        if (command.length() > 5)
            result = transferFile(command.substr(5));
        break;
    default:
        break;
    }
    return result;
}

std::string CommandInterpreter::echo(const std::string& cmd)
{
    Buffer buff(cmd.length() + sizeof(Header));
    Header header = fillHeader(ServerCommand::Echo, cmd.length() + sizeof(Header), false);

    buff.Write(header);
    buff.Write(cmd.c_str(), cmd.length());
    socket.Send(buff, buff.getSize());
    return socket.getLine();
}

std::string CommandInterpreter::transferFile(const std::string& fileName)
{
    std::fstream file;
    size_t size;
    FileInitPackage f;
    unsigned fullChunks, lastChunkSize;
    std::chrono::time_point<std::chrono::system_clock> start, end;
    Buffer buff(maxPackageSize + sizeof(FileTransferPackage));

    file.open(fileName, std::ios_base::in | std::ios_base::binary);
    if (!file.is_open())
        return "Error: cannot open file";
    size = getFileSize(file);
    f = fillInitPackage(size, fileName);
    f.header = fillHeader(ServerCommand::FileTransferStart, sizeof(FileInitPackage), true);
    fullChunks = size / maxPackageSize;
    lastChunkSize = size % maxPackageSize;
    socket.Send(f);
    start = std::chrono::system_clock::now();
    for (unsigned i = 0; i < fullChunks; i++)
    {
        if (i % batchSize == 0)
            while (1)
            {
                sendMarker(i, batchSize);
                if (resendMissingParts(buff, file, i, maxPackageSize, maxPackageSize))
                    break;
            }
        sendFilePart(buff, file, i, maxPackageSize, maxPackageSize);
    }
    if (lastChunkSize)
        sendFilePart(buff, file, fullChunks, maxPackageSize, lastChunkSize);
    end = std::chrono::system_clock::now();
    std::chrono::duration<double> time = end - start;
    return "File transfer complete, avg speed: " + std::to_string((double)size / time.count() / 1024.0 / 1024.0) + "mb/s";
}

bool CommandInterpreter::resendMissingParts(Buffer& buff, std::fstream& file, unsigned chunkId, unsigned fullChunkSize, unsigned chunkSize)
{
    if (socket.getData(buff, markerResponceSize, true) != OperationResult::Success)
        return false;
    buff.Clear();
    if (chunkId == 0)
        return true;
    //std::cerr << "Got marker responce" << std::endl;
    MarkerResponce state = buff;
    for (size_t i = 0; i < batchSize; i++)
        if (!state.bits[i])
        {
            std::cerr << "Resending chunk: " << chunkId - batchSize + i << std::endl;
            sendFilePart(buff, file, chunkId - batchSize + i, fullChunkSize, chunkSize);
        }
    sendMarker(chunkId, 0);
    if (socket.getData(buff, markerResponceSize, true) != OperationResult::Success)
        return false;
    //std::cerr << "Got marker responce 2" << std::endl;
    buff.Clear();
    return true;
}

void CommandInterpreter::sendFilePart(Buffer& buff, std::fstream& file, unsigned chunkId, unsigned fullChunkSize, unsigned chunkSize)
{
    static unsigned lastChunkId = 0;
    FileTransferPackage p;
    p.header = fillHeader(ServerCommand::FileTransferExecute, chunkSize + sizeof(p), false);
    p.size = chunkSize;
    p.chunkId = chunkId;
    buff.Write(p);
    if (chunkId != lastChunkId + 1)
        file.seekg(fullChunkSize * chunkId, std::ios_base::beg);
    file.read((char*)buff.getWritePointer(), chunkSize);
    socket.Send(buff, p.header.packageSize);
    buff.Clear();
    lastChunkId = chunkId;
}

void CommandInterpreter::sendMarker(unsigned chunkId, size_t chunksCount)
{
    FileTransferPackage package;

    package.header = fillHeader(ServerCommand::FileTransferExecute, sizeof(package), false);
    package.isMarker = 1;
    package.chunkId = chunkId;
    package.size = chunksCount;
    socket.Send(package);
}

FileInitPackage CommandInterpreter::fillInitPackage(size_t fileSize, const std::string& fileName)
{
    FileInitPackage f;
    size_t pos;

    f.chunksCount = fileSize / maxPackageSize + (fileSize % maxPackageSize ? 1 : 0);
    f.chunkSize = maxPackageSize;
    f.fileSize = fileSize;
    if ((pos = fileName.find_last_of('/')) != fileName.npos)
        memcpy(f.fileName, fileName.substr(pos + 1).c_str(), fileName.substr(pos + 1).length());
    if ((pos = fileName.find_last_of('\\')) != fileName.npos)
        memcpy(f.fileName, fileName.substr(pos + 1).c_str(), fileName.substr(pos + 1).length());
    else
        memcpy(f.fileName, fileName.c_str(), fileName.length());
    return f;
}

size_t CommandInterpreter::getFileSize(std::fstream& file)
{
    size_t size;

    file.seekg(0, std::ios_base::end);
    size = file.tellg();
    file.seekg(0);
    return size;
}

Header CommandInterpreter::fillHeader(ServerCommand command, size_t size, bool confirm)
{
    Header header;

    header.command = helpers::integral(command);
    header.packageSize = size;
    header.needsConfirmation = confirm ? 1 : 0;
    header.id = clientId;
    return header;
}
