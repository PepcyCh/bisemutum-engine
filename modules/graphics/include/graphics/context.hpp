#pragma once

#include "core/ptr.hpp"
#include "queue.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFX_NAMESPACE_BEGIN

class FrameContext {
public:
    virtual ~FrameContext() = default;

    virtual void Reset() = 0;

    virtual Ptr<class CommandEncoder> GetCommandEncoder(QueueType queue = QueueType::eGraphics) = 0;

protected:
    FrameContext() = default;
};

BISMUTH_GFX_NAMESPACE_END

BISMUTH_NAMESPACE_END
