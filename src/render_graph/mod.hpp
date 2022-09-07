#pragma once

#include "bismuth.hpp"
#include "core/log.hpp"

BISMUTH_NAMESPACE_BEGIN

#define BISMUTH_GFXRG_NAMESPACE_BEGIN namespace gfxrg {
#define BISMUTH_GFXRG_NAMESPACE_END }

BISMUTH_GFXRG_NAMESPACE_BEGIN

inline Logger gRenderGraphLogger;

void Initialize();

BISMUTH_GFXRG_NAMESPACE_END

BISMUTH_NAMESPACE_END
