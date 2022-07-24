#pragma once

#include "bismuth.hpp"

BISMUTH_NAMESPACE_BEGIN

class LoggerManager;

class Logger {
private:
    friend LoggerManager;

    size_t index_;
};

BISMUTH_NAMESPACE_END
