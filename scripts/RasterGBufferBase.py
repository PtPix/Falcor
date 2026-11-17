from falcor import *


def render_graph_RasterGBufferBase():
    g = RenderGraph("RasterGBufferBase")
    g.addPass(createPass("RasterGBuffer"), "GBuffer")

    # Pin names must match RasterGBuffer::reflect() exactly (case-sensitive).
    g.markOutput("GBuffer.Depth")
    g.markOutput("GBuffer.GBuffer0")
    g.markOutput("GBuffer.GBuffer1")
    g.markOutput("GBuffer.GBuffer2")
    g.markOutput("GBuffer.GBuffer3")
    g.markOutput("GBuffer.Ids")

    return g


RasterGBufferBase = render_graph_RasterGBufferBase()
try:
    m.addGraph(RasterGBufferBase)
except NameError:
    None
