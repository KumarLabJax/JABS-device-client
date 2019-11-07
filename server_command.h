#ifndef SERVER_COMMAND_H
#define SERVER_COMMAND_H

#include <string>

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

/**
 * @brief parameters for a "START_RECORDING" command
 */
struct RecordingParameters {
    int session_id;          ///< session ID of recording session device is joining
    size_t duration;         ///< length of recording session in seconds
    bool fragment_hourly;    ///< fragment video files at the top of the hour
    std::string file_prefix; ///< user specified filename prefix
};

/**
 * @brief basic command from the server
 *
 * this is used for basic commands that don't have any parameters
 */
class ServerCommand {
public:

    /**
     * @brief get the type of command this is
     * @return command type of CommandTypes::CommandTypes enum
     */
    CommandTypes::CommandTypes command(){return command_;}

    /**
     * @brief create a new ServerCommand from the json payload in the server
     * response
     * @param command_payload JSON payload sent in response to a status update
     */
    ServerCommand(web::json::value command_payload);

    /**
     * @brief explicitly create a ServerCommand of a specific command type
     * @param cmd command type
     */
    ServerCommand(CommandTypes::CommandTypes cmd);

    /**
     * @brief default constructor -- creates a "NO OP" command
     */
    ServerCommand();

private:
    /// command type (what action the server wants us to take)
    CommandTypes::CommandTypes command_;
};

/**
 * @brief special case command for a START_RECORDING command since it also
 * includes parameters
 */
class RecordCommand: public ServerCommand {
public:
    /**
     * @brief get parameters for this RecordCommand
     * @return RecordingParameters struct containing recording session parameters
     */
    RecordingParameters parameters(){return parameters_;}

    /**
     * @brief createa  RecordCommand from JSON payload
     */
    RecordCommand(web::json::value command_payload);
private:

    /// recording session parameters from server
    RecordingParameters parameters_;

    /**
     * @brief extract parameters from JSON payload
     * @param payload JSON body from server response
     * @return RecordingParameters struct with information extracted from JSON
     */
    RecordingParameters parseRecordingParameters(web::json::value payload);
};

/**
 * @brief determine command type by examining JSON
 * @param payload JSON body from server response
 * @return which command type is specified by the response
 */
CommandTypes::CommandTypes getCommand(web::json::value payload);

#endif //SERVER_COMMAND_H
