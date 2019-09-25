#include <vector>
#include <string>
#include <sstream>

#include <systemd/sd-daemon.h>

#include <cpprest/http_client.h>
#include <cpprest/json.h> 
#include <cpprest/asyncrt_utils.h>

#include "system_info.h"

using namespace utility;
using namespace web;
using namespace web::http;
using namespace web::http::client;
using namespace concurrency::streams;

void send_status_update(SysInfo system_info)
{
    static web::http::client::http_client client("http://bhltms-dev.jax.org/api");
    
    json::value payload;
    json::value json_return;
    
    std::string timestamp = datetime::utc_now().to_string(datetime::date_format::ISO_8601);
    
    // TODO make this configurable
    std::clog << SD_INFO << "Sending status update @ " << timestamp << std::endl;
    
    payload["name"] = web::json::value(system_info.get_hostname());
    payload["timestamp"] = web::json::value::string(timestamp);
    
    //TODO set this based on device state
    payload["state"] = web::json::value("IDLE");
    
    payload["sensor_status"]["camera"]["recording"] = web::json::value::boolean(false);
    //payload["sensor_status"]["camera"]["duration"] = 
    //payload["sensor_status"]["camera"]["fps"] = 
    
    
    payload["system_info"]["uptime"] = web::json::value::number(system_info.get_uptime());
    payload["system_info"]["load"] = web::json::value::number(system_info.get_load());
    payload["system_info"]["free_ram"] = web::json::value::number(system_info.get_mem_available());
    payload["system_info"]["total_ram"] = web::json::value::number(system_info.get_mem_total());
    
    //TODO we are going to change the API to take a list of mount points
    //right now it assumes one mount point is being reported
    std::vector<std::string> mounts = system_info.get_registered_mounts();
    disk_info di = system_info.get_disk_info(mounts[0]);
    payload["system_info"]["free_disk"] = web::json::value::number(di.available);
    payload["system_info"]["total_disk"] = web::json::value::number(di.capacity);
    
    pplx::task<void> requestTask = client.request(web::http::methods::POST, "/device/heartbeat", payload)
    .then([](const web::http::http_response& response) {
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
            
            // the webservice includes a field 'errors' for payload verification failures
            // this is a json object where the keys are the names of params with errors
            // and the value is an error message
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
            return;
        } else if (status == http::status_codes::OK) {
            json::value response_body = response.extract_json().get();
            // TODO parse response to see if the server wants us to do something
        }
        
        
    });
    
    try {
        requestTask.wait();
    }
    catch (http_exception &e)
    {
        std::clog << SD_ERR << "HTTP Exception: " << e.what() << std::endl;
    }
}