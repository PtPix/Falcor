from pathlib import WindowsPath, PosixPath
from falcor import *

def render_graph_R():
    g = RenderGraph('R')
    g.create_pass('GeometryDebugPass', 'GeometryDebugPass', {'displayMode': 6, 'depthMin': 0.0, 'depthMax': 10.0, 'errorMax': 9.999999747378752e-05, 'gridScale': 1.0})
    g.create_pass('RasterGBuffer', 'RasterGBuffer', {})
    g.add_edge('RasterGBuffer.depth', 'GeometryDebugPass.depth')
    g.add_edge('RasterGBuffer.posW', 'GeometryDebugPass.posW')
    g.add_edge('RasterGBuffer.normW', 'GeometryDebugPass.normW')
    g.add_edge('RasterGBuffer.texC', 'GeometryDebugPass.texC')
    g.mark_output('GeometryDebugPass.debugColor')
    return g

R = render_graph_R()
try: m.addGraph(R)
except NameError: None
