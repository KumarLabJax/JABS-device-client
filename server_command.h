#ifndef SERVER_COMMAND_H
#define SERVER_COMMAND_H

#include <string>
#include <map>

#include <cpprest/json.h>

namespace CommandTypes
{
enum CommandTypes {
    NOOP,
    START_RECORDING,
    STOP_RECORDING,
    COMPLETE,
    UNKNOWN
};
}

struct RecordingParameters {
    int session_id;
    size_t duration;
    bool fragment_hourly;
    std::string file_prefix;
};

class BaseCommand {
    virtual CommandTypes::CommandTypes command() = 0;
};

class ServerCommand: public BaseCommand{
public:
    CommandTypes::CommandTypes command(){return command_;}
    ServerCommand(web::json::value command_payload);
    ServerCommand(CommandTypes::CommandTypes cmd);
    ServerCommand();
private:
    CommandTypes::CommandTypes command_;
};

class RecordCommand: public ServerCommand {
public:
    RecordingParameters parameters(){return parameters_;}
    RecordCommand(web::json::value command_payload);
private:
    RecordingParameters parameters_;
    RecordingParameters parseRecordingParameters(web::json::value payload);
};

CommandTypes::CommandTypes getCommand(web::json::value payload);


#endif //SERVER_COMMAND_H
