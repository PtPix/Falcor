#pragma once

#include "Falcor.h"
#include "RenderGraph/RenderPass.h"

using namespace Falcor;

class RasterGBuffer : public RenderPass
{
public:
    FALCOR_PLUGIN_CLASS(RasterGBuffer, "RasterGBuffer", "Raster base pass writing a compact engine-style G-buffer.");

    static ref<RasterGBuffer> create(ref<Device> pDevice, const Properties& props)
    {
        return make_ref<RasterGBuffer>(pDevice, props);
    }

    RasterGBuffer(ref<Device> pDevice, const Properties& props);

    RenderPassReflection reflect(const CompileData& compileData) override;
    void execute(RenderContext* pRenderContext, const RenderData& renderData) override;
    void setScene(RenderContext* pRenderContext, const ref<Scene>& pScene) override;
    void onHotReload(HotReloadFlags reloaded) override;

private:
    struct RasterPipeline
    {
        ref<Program> pProgram;
        ref<ProgramVars> pVars;
        ref<GraphicsState> pState;
    };

    void resetPrograms();
    void createProgram(RasterPipeline& pipeline, const std::string& shaderFile);

    ref<Scene> mpScene;
    ref<Fbo> mpGBufferFbo;
    RasterPipeline mDepthPass;
    RasterPipeline mGBufferPass;
};
