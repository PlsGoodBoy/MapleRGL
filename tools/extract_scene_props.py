"""Export bounded Encounters encounter artwork from read-only NX archives.

Writes only assets/scene_props.json and assets/sprites/scene_props/**.
Reactor art is reused for explicitly custom prototype mechanics. Source RGBA,
origins, frame delays, alpha endpoints and exact paths are preserved unchanged.
No source gameplay scripts, events, collision or original encounter rules are
imported. The existing NX parser opens each archive rb / mmap.ACCESS_READ.
"""
from pathlib import Path
import argparse
import hashlib
import json
import math
import struct
import sys

sys.dont_write_bytecode = True
from PIL import Image, ImageDraw
from extract_local_mobs import NX
from extract_regions import resolve, properties

# Each source was visually inspected before selection. Numeric children are
# exported in numeric order, with no resampling, compositing or recoloring.
SPECS = [
    ('prop_crate.idle', 'Reactor.nx', '0002000.img/0', 'Wooden crate', False),
    ('prop_crate.hit', 'Reactor.nx', '0002000.img/0/hit', 'Wooden crate hit', False),
    ('prop_crate.break', 'Reactor.nx', '0002000.img/3/hit', 'Wooden crate final hit and break', False),
    ('prop_barrel.idle', 'Reactor.nx', '1072000.img/0', 'Purple witch plant', False),
    ('prop_barrel.hit', 'Reactor.nx', '1072000.img/0/hit', 'Purple plant hit', False),
    ('prop_barrel.break', 'Reactor.nx', '1072000.img/2/hit', 'Purple plant final hit and fade', False),
    ('prop_mud.idle', 'Map.nx', 'Obj/acc2.img/darkSwamp/foot/0', 'Low swamp roots', False),
    ('prop_heal.idle', 'Reactor.nx', '2221001.img/0', 'Glowing stone lantern', False),
    ('boss_stumpy.move', 'Mob.nx', '3220000.img/move', 'Stumpy move', True),
    ('boss_stumpy.idle', 'Mob.nx', '3220000.img/stand', 'Stumpy stand', True),
    ('boss_stumpy.hit', 'Mob.nx', '3220000.img/hit1', 'Stumpy hit', False),
    ('boss_stumpy.die', 'Mob.nx', '3220000.img/die1', 'Stumpy death', False),
]

PROP_VISUALS = {
    'prop_crate': {
        'prototype_label': 'Breakable crate', 'source_label': 'Maple Island common wooden-crate reactor',
        'source': 'Reactor.nx/0002000.img', 'states': ['idle', 'hit', 'break'],
        'source_origin_to_ground_px': [0, 17], 'suggested_render_scale': 1.15,
        'draw_layer': 'depth_sorted_actor',
        'mechanics_note': 'Source hit/final-hit artwork shows a wooden crate breaking. Prototype HP, blocking, drops and rewards are custom.'},
    'prop_barrel': {
        'prototype_label': 'Burst flower', 'source_label': 'Purple witch plant reactor',
        'source': 'Reactor.nx/1072000.img', 'states': ['idle', 'hit', 'break'],
        'source_origin_to_ground_px': [0, 34], 'suggested_render_scale': 0.9,
        'draw_layer': 'depth_sorted_actor',
        'mechanics_note': 'The stable barrel key is an internal compatibility name. Artwork is a purple plant, not a barrel. Warning, explosion, damage and radius are custom mechanics; no original explosive behavior is claimed. Final source sequence is a hit and fade, not a source explosion.'},
    'prop_mud': {
        'prototype_label': 'Muddy ground', 'source_label': 'Dark-swamp low roots',
        'source': 'Map.nx/Obj/acc2.img/darkSwamp/foot/0', 'states': ['idle'],
        'source_origin_to_ground_px': [-73, 105], 'suggested_render_scale': 0.185,
        'draw_layer': 'behind_all_actors',
        'mechanics_note': 'Original side-view low swamp-root art is a marker for a custom circular mud zone. It is not a top-down mud texture. Draw the gameplay circle separately; custom slowdown is not source behavior. No source map placement is claimed.'},
    'prop_heal': {
        'prototype_label': 'Healing lantern', 'source_label': 'Stone lantern reactor',
        'source': 'Reactor.nx/2221001.img', 'states': ['idle'],
        'source_origin_to_ground_px': [0, 1], 'suggested_render_scale': 0.5,
        'draw_layer': 'depth_sorted_actor',
        'mechanics_note': 'Original lantern info describes a summoning reactor, not a healing device. Prototype healing, charges and green indicator are custom. Static source art has no hit/break animation in this pack.'},
}


def check_png(path, original):
    with Image.open(path) as image:
        if image.format != 'PNG' or image.mode != 'RGBA' or image.size != original.size:
            raise ValueError('Invalid PNG: ' + str(path))
        if image.tobytes() != original.tobytes():
            raise ValueError('Source RGBA mismatch: ' + str(path))
    with Image.open(path) as image:
        image.verify()


def export(nx, output, key, archive, source, name, loop, validate_only):
    node = resolve(nx, source)
    children = sorted([(k, v) for k, v in nx.children(node).items()
                       if k.isdigit() and nx.node(v)[3] == 5], key=lambda x: int(x[0]))
    if not children or len(children) > 16:
        raise ValueError('Unexpected sequence size at ' + source)
    frames = []
    for frame_name, frame_node in children:
        source_properties = properties(nx, frame_node)
        original = nx.bitmap(frame_node)
        rel = 'sprites/scene_props/' + key.replace('.', '/') + '/' + frame_name + '.png'
        target = output / rel
        if not validate_only:
            target.parent.mkdir(parents=True, exist_ok=True)
            original.save(target)
        check_png(target, original)
        delay = source_properties.get('delay', 100)
        if delay <= 0:
            raise ValueError('Nonpositive source duration: ' + source)
        alpha = original.getchannel('A')
        histogram = alpha.histogram()
        frame = {
            'file': rel, 'duration_ms': delay,
            'origin': source_properties.get('origin', [0, 0]),
            'size': list(original.size), 'source': archive + '/' + source + '/' + frame_name,
            'source_node_id': frame_node,
            'source_bitmap_id': struct.unpack('<IHH', nx.node(frame_node)[4])[0],
            'source_properties': source_properties, 'alpha_range': list(alpha.getextrema()),
            'alpha_nonzero_bounds': list(alpha.getbbox()) if alpha.getbbox() else None,
            'alpha_coverage': {'transparent_pixels': histogram[0], 'partial_pixels': sum(histogram[1:255]),
                               'opaque_pixels': histogram[255]},
            'rgba_sha256': hashlib.sha256(original.tobytes()).hexdigest(),
            'png_sha256': hashlib.sha256(target.read_bytes()).hexdigest(),
        }
        if 'delay' not in source_properties:
            frame['duration_note'] = 'No source delay; 100 ms is a manifest compatibility value, not evidence of animation.'
        for src, dst in [('a0', 'alpha_start'), ('a1', 'alpha_end')]:
            if src in source_properties:
                frame[dst] = source_properties[src]
        frames.append(frame)
    sprite = {'name': name, 'source': archive + '/' + source, 'frames': frames,
              'loop': loop and len(frames) > 1,
              'total_duration_ms': sum(f['duration_ms'] for f in frames),
              'source_sequence_properties': properties(nx, node)}
    base = key.rsplit('.', 1)[0]
    if base in PROP_VISUALS:
        sprite['source_origin_to_ground_px'] = PROP_VISUALS[base]['source_origin_to_ground_px']
        sprite['suggested_render_scale'] = PROP_VISUALS[base]['suggested_render_scale']
        sprite['mechanics_note'] = PROP_VISUALS[base]['mechanics_note']
    else:
        sprite['default_facing'] = 'left'
        sprite['render_note'] = 'Original side-view Mob artwork. Use source origins exactly and mirror only to change facing. Prototype boss attacks and phases are custom, not imported source AI.'
    return sprite


def make_contact_sheet(output, sprites):
    selected = list(sprites.items())
    width, height, columns = 260, 235, 4
    sheet = Image.new('RGB', (width * columns, height * math.ceil(len(selected) / columns)), (43, 50, 48))
    draw = ImageDraw.Draw(sheet)
    for i, (key, sprite) in enumerate(selected):
        # Labelled inspection aid only; runtime PNGs remain completely original.
        frame = sprite['frames'][0]
        with Image.open(output / frame['file']) as original:
            sample = original.copy()
        sample.thumbnail((width - 12, height - 60), Image.Resampling.NEAREST)
        x, y = (i % columns) * width + 6, (i // columns) * height + 26
        sheet.paste(sample, (x, y), sample)
        draw.text((x, y - 20), key, fill=(245, 245, 245))
        draw.text((x, y + height - 58), str(frame['size']) + ' / ' + str(len(sprite['frames'])) + ' frames', fill=(225, 230, 170))
        draw.text((x, y + height - 43), 'origin ' + str(frame['origin']), fill=(190, 205, 198))
    target = output / 'sprites/scene_props/_preview/selection.png'
    target.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(target)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('nx_dir', type=Path)
    parser.add_argument('--output', type=Path, default=Path(__file__).resolve().parents[1] / 'assets')
    parser.add_argument('--validate-only', action='store_true')
    parser.add_argument('--contact-sheet', action='store_true')
    args = parser.parse_args()
    sprites, archives, source_info = {}, {}, {}
    for archive in ('Reactor.nx', 'Map.nx', 'Mob.nx'):
        path = args.nx_dir / archive
        before = path.stat()
        nx = NX(path)
        try:
            for key, src_archive, source, name, loop in SPECS:
                if src_archive == archive:
                    sprites[key] = export(nx, args.output, key, archive, source, name, loop, args.validate_only)
            if archive == 'Reactor.nx':
                for key, visual in PROP_VISUALS.items():
                    if visual['source'].startswith(archive + '/'):
                        root = visual['source'].split('/', 1)[1]
                        source_info[key] = properties(nx, resolve(nx, root + '/info'))
            elif archive == 'Mob.nx':
                source_info['boss_stumpy'] = properties(nx, resolve(nx, '3220000.img/info'))
                if source_info['boss_stumpy'].get('boss') != 1:
                    raise ValueError('Selected alternate mob is not source-tagged boss')
        finally:
            nx.m.close()
            nx.f.close()
        after = path.stat()
        if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
            raise RuntimeError('Source archive changed during extraction: ' + archive)
        archives[archive] = {'size_bytes': before.st_size, 'mtime_ns_at_export': before.st_mtime_ns,
                             'opened_read_only': True, 'unchanged_after_export': True}
    # A stand alias is useful to existing actor render paths; no PNG is duplicated.
    sprites['boss_stumpy.stand'] = {**sprites['boss_stumpy.idle'], 'alias_of': 'boss_stumpy.idle'}
    files = {frame['file']: frame for sprite in sprites.values() for frame in sprite['frames']}
    rgba_bytes = sum(f['size'][0] * f['size'][1] * 4 for f in files.values())
    if len(files) > 64 or rgba_bytes > 10_000_000:
        raise ValueError('Bounded extraction budget exceeded')
    result = {
        'schema_version': 1, 'sprites': sprites, 'prop_visuals': PROP_VISUALS,
        'source_archives': archives, 'source_info_reference_only': source_info,
        'notes': [
            'Original MapleStory artwork belongs to its rights holders. This unofficial personal prototype implies no distribution license.',
            'All runtime PNG pixel bytes, alpha, size and source origins are unchanged. No crop, scale, tint, repaint, atlas or archive repacking was performed.',
            'The crate and plant preserve source hit and final-hit art. The plant final animation fades; any explosion effect must be generated by prototype rendering.',
            'Barrel is a compatibility key, not a visual description. Show this prop as Burst flower in player-facing text.',
            'Stone lantern recovery, swamp slowdown, plant explosion and all prop rewards are custom prototype mechanics. Original reactor event scripts and bounds are not imported.',
            'Render source_origin_world = ground_world - source_origin_to_ground_px * render_scale, then draw frame at source_origin_world - frame.origin * render_scale. The fixed prop offset applies to every state frame; do not bottom-center each frame independently.',
            'Mud root art is an unmodified side-view marker only. Its custom circular zone and clear boundary should be rendered separately behind actors.',
            'Boss Stumpy is source-tagged boss at Mob.nx/3220000.img, with complete move/stand/hit1/die1 sequences. Its prototype phases and attacks are custom.',
            'Static frames without source delay retain a documented 100 ms loader fallback. All explicit source frame delays and a0/a1 endpoints are exact.',
            'Inspection sheets under _preview are not runtime game assets.'
        ],
        'validation': {'unique_gameplay_pngs': len(files), 'rgba_bytes': rgba_bytes,
                       'png_bytes': sum((args.output / f).stat().st_size for f in files),
                       'pixel_equality_checked': True, 'png_integrity_checked': True,
                       'source_archives_unchanged': True},
    }
    target = args.output / 'scene_props.json'
    if args.validate_only:
        old = json.loads(target.read_text(encoding='utf-8'))
        if old != result:
            raise ValueError('Existing manifest differs from source-derived export')
    else:
        args.output.mkdir(parents=True, exist_ok=True)
        target.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        if args.contact_sheet:
            make_contact_sheet(args.output, {k: v for k, v in sprites.items() if 'alias_of' not in v})
    print(json.dumps({'validation': result['validation'], 'sprites': {
        k: {'frames': len(v['frames']), 'duration_ms': v['total_duration_ms']} for k, v in sprites.items()
    }}, indent=2))


if __name__ == '__main__':
    main()
