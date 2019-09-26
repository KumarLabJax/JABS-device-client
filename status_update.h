#ifndef STATUS_UPDATE_H
#define STATUS_UPDATE_H

#include "system_info.h"

/**
 * @brief send a status update message to the server
 *
 * @param system_info current system information
 * @param api_uri root URI of web service
 *
 * @return void
 */
void send_status_update(SysInfo system_info, std::string api_url);

#endif