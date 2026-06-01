"""Embed a hidden helper.py-compatible components layer into a production panel.
The components layer uses Inkscape label='components' and display:none so it
does not render in Rack (nanosvg respects display:none) but helper.py can read it.
Positions are in the panel's native coordinate space (px = mm * S)."""
import re, math, sys

S = 3.7795

def emit(panel_path, params, inputs, outputs, lights, mm_units=False):
    with open(panel_path) as f: svg = f.read()
    mul = 1.0 if mm_units else S
    def circles(items, color, r):
        out=[]
        for name,x,y in items:
            out.append(f'    <circle id="{name}" cx="{x*mul:.3f}" cy="{y*mul:.3f}" r="{r*mul:.3f}" style="fill:{color}"/>')
        return "\n".join(out)
    layer = f'''<g inkscape:label="components" inkscape:groupmode="layer" id="components" style="display:none">
{circles(params,  "#ff0000", 1.0)}
{circles(inputs,  "#00ff00", 1.0)}
{circles(outputs, "#0000ff", 1.0)}
{circles(lights,  "#00ffff", 0.6)}
</g>'''
    # Add inkscape namespace if missing
    if 'xmlns:inkscape' not in svg:
        svg = svg.replace('<svg ', '<svg xmlns:inkscape="http://www.inkscape.org/namespaces/inkscape" ', 1)
    # Remove any prior components layer
    svg = re.sub(r'<g inkscape:label="components".*?</g>\s*(?=</svg>)', '', svg, flags=re.DOTALL)
    svg = svg.replace('</svg>', layer + '\n</svg>')
    with open(panel_path,'w') as f: f.write(svg)
    return len(params)+len(inputs)+len(outputs)+len(lights)
