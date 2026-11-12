"""Package the approved RGBA artwork into a Windows multi-size icon (Pillow)."""
from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parents[1]
source = root / 'resources/maplergl.png'
output = root / 'resources/maplergl.ico'
sizes = [(n, n) for n in (16, 24, 32, 48, 64, 128, 256)]
with Image.open(source) as artwork:
    artwork.save(output, format='ICO', sizes=sizes)
print(f'Windows icon: {output}')
