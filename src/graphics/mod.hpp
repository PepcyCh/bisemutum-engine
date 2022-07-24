#pragma once

#include "bismuth.hpp"
#include "core/log.hpp"

BISMUTH_NAMESPACE_BEGIN

#define BISMUTH_GFX_NAMESPACE_BEGIN namespace gfx {
#define BISMUTH_GFX_NAMESPACE_END }

BISMUTH_GFX_NAMESPACE_BEGIN

inline Logger gGraphicsLogger;

void Initialize();

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
