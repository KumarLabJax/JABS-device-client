#include "server_command.h"

#include <cpprest/json.h>

using namespace web;
using namespace CommandTypes;

#define START_CMD "START"
#define STOP_CMD  "STOP"
#define COMPLETE_CMD "COMPLETE"

CommandTypes::CommandTypes getCommand(json::value payload)
{
    if (payload["command_name"].as_string() == START_CMD) {
        return CommandTypes::START_RECORDING;
    } else if (payload["command_name"].as_string() == STOP_CMD) {
        return CommandTypes::STOP_RECORDING;
    } else if (payload["command_name"].as_string() == COMPLETE_CMD) {
        return CommandTypes::COMPLETE;
    }
    return CommandTypes::UNKNOWN;
}

RecordingParameters RecordCommand::parseRecordingParameters(web::json::value payload)
{
    assert(getCommand(payload) == START_RECORDING);

    utility::stringstream_t ss;
    ss << payload["parameters"].as_string();
    json::value parameters = json::value::parse(ss);

    RecordingParameters params;
    if (!parameters["file_prefix"].is_null()) {
        params.file_prefix = parameters["file_prefix"].as_string();
    } else {
        params.file_prefix = "";
    }
    params.fragment_hourly = parameters["fragment_hourly"].as_bool();
    params.apply_filter = parameters["apply_filter"].as_bool();
    params.duration = parameters["duration"].as_number().to_uint64();
    params.session_id = parameters["session_id"].as_number().to_int32();
    params.target_fps = parameters["target_fps"].as_number().to_int32();

    return params;

}

ServerCommand::ServerCommand() : command_(NOOP) {}

ServerCommand::ServerCommand(CommandTypes::CommandTypes cmd): command_(cmd) {}

ServerCommand::ServerCommand(web::json::value command_payload)
{
    command_ = getCommand(command_payload);
}

RecordCommand::RecordCommand(web::json::value command_payload) : ServerCommand(command_payload)
{
    parameters_ = parseRecordingParameters(command_payload);
}