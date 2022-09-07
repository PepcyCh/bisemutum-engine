#include "mod.hpp"

#include "core/logger.hpp"

BISMUTH_NAMESPACE_BEGIN

BISMUTH_GFXRG_NAMESPACE_BEGIN

void Initialize() {
    gRenderGraphLogger = LoggerManager::Get().RegisterLogger("RenderGraph");
}

BISMUTH_GFXRG_NAMESPACE_END

BISMUTH_NAMESPACE_END
