// Copyright 2019, The Jackson Laboratory, Bar Harbor, Maine - all rights reserved

#ifndef LTM_EXCEPTIONS_H
#define LTM_EXCEPTIONS_H

#include <stdexcept>

class NotImplementedException : public std::logic_error
{
public:
    NotImplementedException() : std::logic_error("Function not yet implemented") { };
};

#endif
