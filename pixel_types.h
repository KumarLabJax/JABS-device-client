#ifndef PIXEL_TYPES_H
#define PIXEL_TYPES_H

#include <string>
namespace pixel_types {
static std::vector<std::string> type_names({"Mono8", "Mono12", "YUV420P"});
static const std::string YUV420P = "YUV420P";
static const std::string MONO8 = "Mono8";
static const std::string MONO12 = "Mono12";

bool Validate(std::string type_name);
} //namespace pixel_types
#endif
