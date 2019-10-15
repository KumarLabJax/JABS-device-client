// Copyright 2019, The Jackson Laboratory, Bar Harbor, Maine - all rights reserved

#ifndef PIXEL_TYPES_H
#define PIXEL_TYPES_H

#include <string>
#include <vector>

namespace pixel_types {
static const std::vector<std::string> type_names({"Mono8", "Mono12", "YUV420P"});
static const std::string YUV420P = "YUV420P";
static const std::string MONO8 = "Mono8";
static const std::string MONO12 = "Mono12";

/**
 * @brief validate that a given string is a valid pixel format
 * @param type_name string to validate
 * @return true if teh string is a known pixel type, false otherwise
 */
bool Validate(std::string type_name);
} //namespace pixel_types
#endif
