// Copyright 2019, The Jackson Laboratory, Bar Harbor, Maine - all rights reserved

#include <iostream>
#include <string>
#include <systemd/sd-daemon.h>
#include <vector>

#include <cpprest/asyncrt_utils.h>
#include <cpprest/http_client.h>
#include <cpprest/json.h> 

#include "status_update.h"
#include "system_info.h"

using namespace utility;
using namespace web;
using namespace web::http;
using namespace web::http::client;
using namespace concurrency::streams;

// API endpoint for sending status updates
const std::string kStatusUpdateEndpoint = "/device/heartbeat";


BaseCommand* send_status_update(SysInfo system_info, CameraController& camera_controller, const std::string api_uri)
{
    static web::http::client::http_client client(api_uri);
    
    json::value payload;
    BaseCommand *command = nullptr;

    std::string timestamp = datetime::utc_now().to_string(datetime::date_format::ISO_8601);    
    std::clog << SD_INFO << "Sending status update @ " << timestamp << std::endl;
    
    payload["name"] = web::json::value(system_info.hostname());
    payload["timestamp"] = web::json::value::string(timestamp);

    if (camera_controller.recording()) {
        payload["state"] = web::json::value("BUSY");
        payload["sensor_status"]["camera"]["recording"] = web::json::value::boolean(true);
        payload["sensor_status"]["camera"]["duration"] = web::json::value::number(camera_controller.elapsed_time().count());
        payload["sensor_status"]["camera"]["fps"] = web::json::value::number(camera_controller.avg_fps());
        payload["session_id"] = web::json::value::number(camera_controller.session_id());
    } else {
        payload["state"] = web::json::value("IDLE");
        payload["sensor_status"]["camera"]["recording"] = web::json::value::boolean(false);
    }
    
    payload["system_info"]["release"] = web::json::value::string(system_info.release());
    payload["system_info"]["uptime"] = web::json::value::number(system_info.uptime());
    payload["system_info"]["load"] = web::json::value::number(system_info.load());
    payload["system_info"]["free_ram"] = web::json::value::number(system_info.mem_available());
    payload["system_info"]["total_ram"] = web::json::value::number(system_info.mem_total());
    
    //TODO we might change the API to take a list of mount points that are being monitored
    //right now we only support monitoring a single drive, so there will only be one
    //registered mount in the vector returned by system_info.get_registered_mounts()
    std::vector<std::string> mounts = system_info.registered_mounts();
    DiskInfo di = system_info.disk_info(mounts[0]);
    payload["system_info"]["free_disk"] = web::json::value::number(di.available);
    payload["system_info"]["total_disk"] = web::json::value::number(di.capacity);
    
    // send update to the server
    pplx::task<void> requestTask = client.request(web::http::methods::POST, kStatusUpdateEndpoint, payload)
    .then([&command](const web::http::http_response& response) {
        status_code status = response.status_code();
        
        if (status >= http::status_codes::BadRequest) {
            json::value response_body = response.extract_json().get();
            
            // for systemd logging purposes, each line is handled as a new logging event
            // therefore we combine all the information related to this error into a 
            // single line
            std::string err_msg;
            
            if (response_body.has_field("message")) {
                err_msg = response_body["message"].as_string();
            } else {
                err_msg = "Status update request failed with http status code " + status;
            }
            
            // The webservice includes a field 'errors' for payload verification failures.
            // This is a JSON object where the keys are the names of invalid parameters
            // and the value is an error message.
            if (response_body.has_field("errors")) {
                if (!err_msg.empty()) {
                    err_msg += ":  ";
                }
                
                json::object err_obj = response_body["errors"].as_object();
                for (auto iter = err_obj.cbegin(); iter != err_obj.cend(); ++iter)
                {
                    const utility::string_t &key = iter->first;
                    const json::value &val = iter->second;
                    if (iter != err_obj.cbegin()) {
                        err_msg += ", ";
                    }
                    err_msg += key + ":" + val.as_string();
                }
            }
            
            std::clog << SD_ERR << err_msg << std::endl;
            
        } else if (status == http::status_codes::NoContent) {
            std::clog << SD_INFO << "Server responded with no content" << std::endl;
        } else if (status == http::status_codes::OK) {
            json::value response_body = response.extract_json().get();
            switch (getCommand(response_body)) {
                case CommandTypes::START_RECORDING:
                    command = new RecordCommand(response_body);
                    break;
                case CommandTypes::STOP_RECORDING:
                    command = new ServerCommand(response_body);
                    break;
                case CommandTypes::NOOP:
                    command = new ServerCommand();
                    break;
                case CommandTypes::UNKNOWN:
                    command = new ServerCommand(CommandTypes::UNKNOWN);
                    break;
            }
        }      
    });
    
    try {
        requestTask.wait();
    }
    catch (http_exception &e)
    {
        std::clog << SD_ERR << "HTTP Exception: " << e.what() << std::endl;
    }

    if (!command) {
        command = new ServerCommand();
    }

    return command;
}