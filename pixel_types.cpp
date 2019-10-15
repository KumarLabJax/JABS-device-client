// Copyright 2019, The Jackson Laboratory, Bar Harbor, Maine - all rights reserved

#include <algorithm>
#include <string>
#include <vector>

#include "pixel_types.h"

namespace pixel_types {
bool Validate(std::string type_name)
{
    return std::find(type_names.begin(), type_names.end(), type_name) != type_names.end();
}
} //namespace pixel_types
