#pragma once
#include "Falcor.h"
#include "RenderGraph/RenderPass.h"

using namespace Falcor;

class GeometryDebugPass : public RenderPass
{
public:
    FALCOR_PLUGIN_CLASS(GeometryDebugPass, "GeometryDebugPass", "Depth reconstruction and geometry diagnostics visualization.");

    static ref<GeometryDebugPass> create(ref<Device> pDevice, const Properties& props)
    {
        return make_ref<GeometryDebugPass>(pDevice, props);
    }

    GeometryDebugPass(ref<Device> pDevice, const Properties& props);

    virtual RenderPassReflection reflect(const CompileData& compileData) override;
    Properties getProperties() const override;
    virtual void execute(RenderContext* pRenderContext, const RenderData& renderData) override;
    virtual void setScene(RenderContext* pRenderContext, const ref<Scene>& pScene) override;
    void renderUI(Gui::Widgets& widget) override;
    void onHotReload(HotReloadFlags reloaded) override;

private:
    ref<Scene> mpScene;
    ref<Program> mpProgram;
    ref<ProgramVars> mpVars;
    ref<ComputeState> mpState;

    uint32_t mDisplayMode = 6;
    float mDepthMin = 0.0f;
    float mDepthMax = 10.0f;
    float mErrorMax = 1e-4f;
    float mGridScale = 1.0f;
};