#ifndef COMMANDINTERPRETER_H
#define COMMANDINTERPRETER_H

#include <string>
#include "connection.h"

enum class Command {Echo, File, Unknown};

class CommandInterpreter
{
    Connection& socket;
    unsigned maxPackageSize = 1024;
    u_int8_t clientId = 1;
    size_t getFileSize(std::fstream& file);
    FileInitPackage fillInitPackage(size_t fileSize, const std::string& fileName);
    std::string transferFile(const std::string& cmd);
    std::string echo(const std::string& cmd);
    Command decode(const std::string& cmd);
    Header fillHeader(ServerCommand command, size_t size, bool confirm);
    void sendMarker();
    void sendFilePart(Buffer& buff, std::fstream& file, unsigned chunkId, unsigned fullChunkSize, unsigned chunkSize);
    bool resendMissingParts(Buffer& buff, std::fstream& file, unsigned chunkId, unsigned fullChunkSize, unsigned chunkSize);
public:
    CommandInterpreter(Connection& socket, int id);
    std::string Interpret(const std::string& command);
};

#endif // COMMANDINTERPRETER_H
