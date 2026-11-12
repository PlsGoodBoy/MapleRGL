"""Selectively export a compact Progression regional decoration pack from Map.nx.

The archive is opened read-only by the project's existing NX reader. Exported
PNGs retain source RGBA pixels, sizes, origins, frame delays and alpha metadata.
This is not a complete map, top-down floor conversion, or collision importer.
Writes only assets/regions.json and assets/sprites/regions/**.
"""
from pathlib import Path
import argparse
import hashlib
import json
import math
import struct

from PIL import Image, ImageDraw
from extract_local_mobs import NX


# Selection was checked against actual source pixels, not inferred from names.
# A source without a final frame component exports its complete small sequence.
SPECS = [
    ('map.henesys.flowers', 'Obj/acc1.img/grassySoil/nature/0', 'Red flower patch', 'transparent_decor', 'Scatter sparsely behind actors; low profile.'),
    ('map.henesys.clover', 'Obj/acc1.img/grassySoil/nature/10', 'Clover and white flowers', 'transparent_decor', 'Scatter sparsely behind actors.'),
    ('map.henesys.mushrooms', 'Obj/acc1.img/grassySoil/nature/17', 'Small multicolor mushroom patch', 'transparent_decor', 'Ambient noninteractive mushrooms, not enemy or pickup markers.'),
    ('map.henesys.sunflower', 'Obj/acc1.img/grassySoil/nature/22', 'Small sunflower', 'transparent_decor', 'Sparse behind-actor accent; never a blocking obstacle.'),
    ('map.henesys.tree', 'Obj/acc1.img/grassySoil/nature/15', 'Round green tree', 'transparent_decor', 'Boundary or camp backdrop only; keep large canopy outside the combat travel corridor.'),
    ('map.henesys.sideview_ground_edge', 'Tile/grassySoil.img/enH0/0', 'Original grassy platform edge', 'source_side_view_tile', 'Side-view edge, not a top-down floor. Optional outer boundary/backdrop use only; do not imply walkable platforms.'),
    ('map.ellinia.leaf_patch', 'Obj/acc2.img/shineWood/foot/3', 'Compact bright leaf cluster', 'transparent_decor', 'Original side-view foliage/ledge art. Use as a small behind-actor foliage patch, without platform behavior.'),
    ('map.ellinia.vine', 'Obj/acc2.img/shineWood/flower/3', 'Twisted green vine', 'transparent_decor', 'Boundary foliage only; keep tall vine away from the central travel corridor.'),
    ('map.ellinia.trunk_back', 'Back/shineWood.img/back/9', 'Layered green forest trunk', 'transparent_background_decor', 'Boundary backdrop behind all actors. The source is a partial trunk segment, not a freestanding top-down tree.'),
    ('map.ellinia.magic_orb', 'Obj/acc2.img/shineWood/flower/8', 'Pulsing blue forest orb', 'transparent_decor', 'Optional boundary ambience only. Preserve original 4000/3000 ms alpha pulse; do not use as a pickup, portal, or combat telegraph.'),
    ('map.perion.pebbles', 'Obj/acc1.img/dryRock/nature/15', 'Low rocky pebble cluster', 'transparent_decor', 'Sparse low-profile ground decoration behind actors; no collision.'),
    ('map.perion.dry_grass', 'Obj/acc1.img/dryRock/nature/12', 'Dry grass and twigs', 'transparent_decor', 'Sparse low-profile ground decoration behind actors.'),
    ('map.perion.thorn_bush', 'Obj/acc1.img/dryRock/nature/13', 'Dry thorn bush', 'transparent_decor', 'Boundary accent or sparse behind-actor decor; not a damaging object.'),
    ('map.perion.dead_branch', 'Obj/acc1.img/dryRock/nature/21', 'Short dead branch', 'transparent_decor', 'Sparse boundary accent; no blocking behavior.'),
    ('map.perion.stump', 'Obj/acc1.img/dryRock/nature/23', 'Large weathered stump', 'transparent_decor', 'Outer-boundary backdrop only; large art must stay out of the combat corridor.'),
    ('map.perion.sideview_ground_edge', 'Tile/dryRock.img/enH0/0', 'Original dry rock platform edge', 'source_side_view_tile', 'Side-view edge, not a top-down floor. Optional outer boundary/backdrop only; no platform or cliff collision is supplied.'),
    ('map.camp.fire', 'Obj/trap.img/fire/campfire/0', 'Six-frame glowing campfire', 'transparent_decor', 'Camp ambience only, behind actors. Large soft source glow is preserved. Original source is in trap.img; its hazard bounds are metadata only and do not authorize damage or collision.'),
]
MAP_IDS = (100000000, 100010000, 100020000, 101000000, 101010000, 102000000, 102010000)
MAX_PNGS = 28
MAX_RGBA_BYTES = 2_500_000


def resolve(nx, source):
    node = 0
    for part in source.split('/'):
        node = nx.children(node)[part]
    return node


def scalar(nx, node):
    kind, raw = nx.node(node)[3:]
    if kind == 1:
        return struct.unpack('<q', raw)[0]
    if kind == 2:
        return struct.unpack('<d', raw)[0]
    if kind == 3:
        return nx.name(struct.unpack('<I', raw[:4])[0])
    if kind == 4:
        return list(struct.unpack('<ii', raw))
    return None


def properties(nx, node):
    return {name: scalar(nx, child) for name, child in nx.children(node).items()
            if nx.node(child)[3] in (1, 2, 3, 4)}


def map_evidence(nx):
    maps, references = [], {}
    for map_id in MAP_IDS:
        source = 'Map/Map1/' + str(map_id) + '.img'
        children = nx.children(resolve(nx, source))
        info = properties(nx, children['info'])
        backs, tiles = set(), set()
        for name, node in nx.children(children['back']).items():
            props = properties(nx, node)
            family = props.get('bS')
            if not family:
                continue
            backs.add(family)
            path = 'Back/' + family + '.img/' + ('ani' if props.get('ani') else 'back') + '/' + str(props['no'])
            references.setdefault(path, []).append('Map.nx/' + source + '/back/' + name)
        for layer in '01234567':
            layer_children = nx.children(children[layer])
            layer_info = properties(nx, layer_children['info']) if 'info' in layer_children else {}
            tile_family = layer_info.get('tS')
            if tile_family:
                tiles.add(tile_family)
            for name, node in nx.children(layer_children['obj']).items():
                p = properties(nx, node)
                path = 'Obj/' + p['oS'] + '.img/' + '/'.join(str(p[k]) for k in ('l0', 'l1', 'l2'))
                references.setdefault(path, []).append('Map.nx/' + source + '/' + layer + '/obj/' + name)
            if tile_family and 'tile' in layer_children:
                for name, node in nx.children(layer_children['tile']).items():
                    p = properties(nx, node)
                    if 'u' in p and 'no' in p:
                        path = 'Tile/' + tile_family + '.img/' + str(p['u']) + '/' + str(p['no'])
                        references.setdefault(path, []).append('Map.nx/' + source + '/' + layer + '/tile/' + name)
        maps.append({'map_id': map_id, 'source': 'Map.nx/' + source,
                     'map_marker': info.get('mapMark'), 'town': bool(info.get('town', 0)),
                     'bgm_reference_only': info.get('bgm'),
                     'background_families': sorted(backs), 'tile_families': sorted(tiles)})
    return maps, references


def check_png(target, original):
    with Image.open(target) as check:
        if check.format != 'PNG' or check.mode != 'RGBA' or check.size != original.size:
            raise ValueError('Invalid PNG format/size: ' + str(target))
        if check.tobytes() != original.tobytes():
            raise ValueError('RGBA mismatch: ' + str(target))
    with Image.open(target) as check:
        check.verify()


def export(nx, output, spec, references, validate_only=False):
    key, source, label, asset_type, note = spec
    root = resolve(nx, source)
    is_single = nx.node(root)[3] == 5
    sequence = [('0', root)] if is_single else sorted(
        [(name, node) for name, node in nx.children(root).items()
         if name.isdigit() and nx.node(node)[3] == 5], key=lambda pair: int(pair[0]))
    if not sequence or len(sequence) > 8:
        raise ValueError('Unexpected sequence size: ' + source)
    frames = []
    for name, node in sequence:
        props = properties(nx, node)
        original = nx.bitmap(node)
        rel = 'sprites/regions/' + key[4:].replace('.', '/') + '/' + name + '.png'
        target = output / rel
        if not validate_only:
            target.parent.mkdir(parents=True, exist_ok=True)
            original.save(target)
        check_png(target, original)
        if 'delay' in props and props['delay'] <= 0:
            raise ValueError('Invalid source duration: ' + source)
        alpha = original.getchannel('A')
        histogram = alpha.histogram()
        source_frame = source if is_single else source + '/' + name
        frame = {'file': rel, 'duration_ms': props.get('delay', 100),
                 'origin': props.get('origin', [0, 0]), 'size': list(original.size),
                 'source': 'Map.nx/' + source_frame,
                 'source_node_id': node,
                 'source_bitmap_id': struct.unpack('<IHH', nx.node(node)[4])[0],
                 'source_properties': props,
                 'alpha_range': list(alpha.getextrema()),
                 'alpha_nonzero_bounds': list(alpha.getbbox()),
                 'alpha_coverage': {'transparent_pixels': histogram[0],
                                    'partial_pixels': sum(histogram[1:255]),
                                    'opaque_pixels': histogram[255]},
                 'rgba_sha256': hashlib.sha256(original.tobytes()).hexdigest(),
                 'png_sha256': hashlib.sha256(target.read_bytes()).hexdigest()}
        if 'delay' not in props:
            frame['duration_note'] = 'Static source bitmap has no delay; 100 ms is a manifest compatibility value, not an animation.'
        for src, dest in [('a0', 'alpha_start'), ('a1', 'alpha_end')]:
            if src in props:
                frame[dest] = props[src]
        frames.append(frame)
    placements = list(dict.fromkeys(references.get(source, [])))
    return {'name': label, 'source': 'Map.nx/' + source,
            'asset_type': asset_type, 'frames': frames, 'loop': len(frames) > 1,
            'total_duration_ms': sum(frame['duration_ms'] for frame in frames),
            'source_sequence_properties': {} if is_single else properties(nx, root),
            'collision': False, 'interactive': False, 'draw_layer': 'behind_all_actors',
            'render_note': note,
            'verified_map_placements': placements[:4],
            'verified_placement_count_in_reference_maps': len(placements),
            'provenance_note': ('Exact source placement is verified in the listed reference maps.' if placements
                                else 'Original source-family artwork; no exact placement was found in the seven selected reference maps. Do not claim this object occurs there.')}


def contact_sheet(output, sprites):
    # A labelled inspection aid, not a gameplay texture or a replacement original.
    selected = [(key, sprite) for key, sprite in sprites.items() if 'alias_of' not in sprite]
    width, height, columns = 240, 220, 4
    sheet = Image.new('RGB', (width * columns, height * math.ceil(len(selected) / columns)), (44, 51, 46))
    draw = ImageDraw.Draw(sheet)
    for i, (key, sprite) in enumerate(selected):
        frame = sprite['frames'][0]
        with Image.open(output / frame['file']) as original:
            sample = original.copy()
        sample.thumbnail((width - 12, height - 50), Image.Resampling.NEAREST)
        x, y = (i % columns) * width + 6, (i // columns) * height + 24
        sheet.paste(sample, (x, y), sample)
        draw.text((x, y - 19), key.replace('map.', ''), fill=(245, 245, 245))
        draw.text((x, y + height - 48), str(frame['size']) + ' / ' + str(len(sprite['frames'])) + ' frame(s)', fill=(220, 222, 170))
        draw.text((x, y + height - 34), sprite['asset_type'], fill=(190, 200, 194))
    target = output / 'sprites/regions/_preview/selection.png'
    target.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(target)
    return target


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('map_nx', type=Path)
    parser.add_argument('--output', type=Path, default=Path(__file__).resolve().parents[1] / 'assets')
    parser.add_argument('--validate-only', action='store_true', help='Read and compare existing PNG/JSON output without writing')
    parser.add_argument('--contact-sheet', action='store_true', help='Create a labelled preview separately from gameplay PNGs')
    args = parser.parse_args()
    before = args.map_nx.stat()
    nx = NX(args.map_nx)
    try:
        maps, references = map_evidence(nx)
        sprites = {spec[0]: export(nx, args.output, spec, references, args.validate_only) for spec in SPECS}
        sprites['map.camp.tree'] = {**sprites['map.henesys.tree'], 'alias_of': 'map.henesys.tree',
                                   'name': 'Camp shade tree',
                                   'render_note': 'Reuses the same unmodified Henesys tree PNG. Place behind the camp area, outside movement paths; no collision.'}
    finally:
        nx.m.close()
        nx.f.close()
    after = args.map_nx.stat()
    if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
        raise RuntimeError('Source Map.nx changed during extraction')
    files = {frame['file']: frame for sprite in sprites.values() for frame in sprite['frames']}
    rgba_bytes = sum(frame['size'][0] * frame['size'][1] * 4 for frame in files.values())
    if len(files) > MAX_PNGS or rgba_bytes > MAX_RGBA_BYTES:
        raise ValueError('Bounded extraction budget exceeded')
    result = {'schema_version': 1, 'sprites': sprites, 'reference_maps': maps,
              'source_archive': {'name': 'Map.nx', 'size_bytes': before.st_size,
                                 'mtime_ns_at_export': before.st_mtime_ns, 'opened_read_only': True},
              'regions': {
                  'henesys': {'reference_map_ids': [100010000, 100020000], 'source_family': 'grassySoil', 'theme': 'Bright green field outskirts'},
                  'ellinia': {'reference_map_ids': [101000000, 101010000], 'source_family': 'shineWood', 'theme': 'Dense green magical forest'},
                  'perion': {'reference_map_ids': [102000000, 102010000], 'source_family': 'dryRock', 'theme': 'Dry rocky highlands'},
                  'camp': {'source_family': 'Reused Henesys tree plus trap/campfire art', 'theme': 'Optional quiet camp decorations'}},
              'notes': [
                  'Original MapleStory art belongs to its rights holders. This unofficial personal prototype provides no distribution license.',
                  'All gameplay PNG pixels, alpha, dimensions and origins are unchanged. No atlas, crop, resize, recolor, repaint, or source repacking was performed.',
                  'This pack is a selective set of decorations, not a whole-map export. Named themes are supported by source mapMark and asset-family references.',
                  'The two source_side_view_tile entries are original platform edges, not native top-down floor tiles. No seamless top-down floor is claimed.',
                  'All artwork is noninteractive and collision-free for this prototype. Original z/lt/rb and other properties are retained for provenance only.',
                  'Keep tall trees, stumps, vines and trunks at boundaries. Draw decorations behind all actors; leave the playable center and telegraphs visually clear.',
                  'Render at position minus frame origin, as with existing sprites. Do not assume origin is bottom-center. Use source size and origin when aligning decor.',
                  'Map references document exact observed placements when available; source-family-only selections explicitly say so.',
                  'The optional inspection contact sheet is a labelled preview only, not runtime artwork.'
              ],
              'validation': {'unique_gameplay_pngs': len(files), 'rgba_bytes': rgba_bytes,
                             'png_bytes': sum((args.output / path).stat().st_size for path in files),
                             'pixel_exact_rgba': True, 'source_stat_unchanged': True}}
    target = args.output / 'regions.json'
    if args.validate_only:
        existing = json.loads(target.read_text(encoding='utf-8'))
        if existing != result:
            raise ValueError('Existing manifest differs from current source metadata')
    else:
        args.output.mkdir(parents=True, exist_ok=True)
        target.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        if args.contact_sheet:
            contact_sheet(args.output, sprites)
    print(json.dumps({'sprite_keys': list(sprites), **result['validation'], 'mode': 'validate-only' if args.validate_only else 'export'}, indent=2))


if __name__ == '__main__':
    main()
