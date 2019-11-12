// Copyright 2019, The Jackson Laboratory, Bar Harbor, Maine - all rights reserved

#ifndef STATUS_UPDATE_H
#define STATUS_UPDATE_H

#include "system_info.h"
#include "server_command.h"
#include "camera_controller.h"

/**
 * @brief send a status update message to the server
 *
 * @param system_info current system information
 * @param api_uri root URI of web service
 *
 * @return void
 */
ServerCommand* send_status_update(SysInfo system_info,
    CameraController& camera_controller,
    const std::string api_url);

#endif