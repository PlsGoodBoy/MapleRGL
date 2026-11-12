#!/usr/bin/env python3
"""Export the selected Fusion enemy appearance pack from read-only NX archives.

Actual encounter AI, collision circles and chapter pools belong to MapleRGL.
The source art, frame origins, typed delays and alpha endpoints are preserved.
Existing enemy PNGs are reused after comparison with their original NX pixels.
"""
import argparse
import hashlib
import json
import math
import struct
import sys
from pathlib import Path

sys.dont_write_bytecode = True
from PIL import Image, ImageDraw
from extract_local_mobs import NX
from extract_regions import resolve, properties
from extract_equipment_icons import fingerprint
from extract_scene_props import check_png


# key, source mob ID, gameplay label, scale (before camera zoom), footprint
# radius in world units, existing PNG prefix, suggested region, behavior cue.
BOSSES = [
    ('boss', 6130101, '蘑菇王', 1.50, 50, 'boss', 'village', 'landing_pressure'),
    ('boss_stumpy', 3220000, '森林守卫', 1.00, 46, 'boss_stumpy', 'forest', 'roots_and_lines'),
    ('boss_king_slime', 9300003, '超级绿水灵', 1.05, 48, None, 'village', 'leap_and_split'),
    ('boss_snail_king', 2220000, '蜗牛王', 1.50, 40, None, 'village', 'slow_bullet_patterns'),
    ('boss_golem', 8210005, '石巨人统领', 1.15, 48, None, 'wasteland', 'heavy_slam_and_opening'),
    ('boss_balrog', 8150000, '蝙蝠魔', .60, 32, None, 'wasteland', 'dive_and_crossfire'),
]
ENEMIES = [
    ('enemy.slime', 210100, '绿水灵', .85, 16, 'slime', 'village', 'pursuit'),
    ('enemy.mushroom', 120100, '蘑菇仔', 1.20, 18, 'mushroom', 'village', 'pursuit'),
    ('enemy.blue_mushroom', 2220100, '蓝蘑菇', .85, 22, 'blue_mushroom', 'village', 'armored'),
    ('enemy.pig', 1210100, '猪猪', .90, 16, 'pig', 'village', 'rush'),
    ('enemy.wild_boar', 2230102, '野猪', 1.05, 25, 'elite', 'village/wasteland', 'rush_elite'),
    ('enemy.snail', 100100, '蜗牛', 1.25, 14, None, 'village', 'pursuit'),
    ('enemy.stump', 130100, '木妖', .95, 20, None, 'forest', 'pursuit'),
    ('enemy.octopus', 1120100, '三眼章鱼', .90, 21, None, 'forest', 'splitter'),
    ('enemy.snake', 2130103, '青蛇', 1.35, 13, None, 'forest', 'rush'),
    ('enemy.lupin', 3210800, '猴子', 1.00, 18, None, 'forest', 'ranged'),
    ('enemy.wood_mask', 2230110, '木面怪人', .80, 22, None, 'forest', 'support'),
    ('enemy.stone_mask', 2230111, '石面怪人', .90, 23, None, 'wasteland', 'armored'),
    ('enemy.skeleton', 5150001, '骷髅士兵', .85, 21, None, 'wasteland', 'armored_ranged'),
    ('enemy.scorpion', 2110301, '沙漠毒蝎', .95, 21, None, 'wasteland', 'rush'),
    ('enemy.ghost', 3230101, '小幽灵', 1.10, 15, None, 'forest', 'splitter'),
    ('enemy.fairy', 3000001, '蝴蝶精', 1.00, 15, None, 'forest', 'support'),
]


def mob_name(strings, mob_id):
    path = f'Mob.img/{mob_id}/name'
    node = strings.node(resolve(strings, path))
    if node[3] != 3:
        raise ValueError('Expected original mob name: ' + path)
    return strings.name(struct.unpack('<I', node[4][:4])[0]), 'String.nx/' + path


def export_sequence(nx, output, key, source, reused):
    node = resolve(nx, source)
    children = sorted([(int(name), index) for name, index in nx.children(node).items()
                       if name.isdigit() and nx.node(index)[3] == 5])
    if not children:
        raise ValueError('Selected animation has no original bitmap frames: ' + source)
    old_frames = reused.get('frames', []) if reused else []
    frames = []
    for ordinal, (number, frame_node) in enumerate(children):
        props = properties(nx, frame_node)
        original = nx.bitmap(frame_node)
        # Some NX delays are string-typed decimal milliseconds. Treating their
        # 8 raw bytes as an integer yields a string-table index, not a duration.
        delay = int(props.get('delay', 100))
        if delay <= 0:
            raise ValueError('Nonpositive original animation delay: ' + source)
        if old_frames:
            if len(old_frames) != len(children):
                raise ValueError('Existing animation frame count differs: ' + key)
            relative = old_frames[ordinal]['file']
            check_png(output / relative, original)
        else:
            relative = 'sprites/enemy_appearances/' + key.replace('.', '/') + f'/{number}.png'
            target = output / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            original.save(target)
            check_png(target, original)
        alpha = original.getchannel('A')
        frame = {'file': relative, 'duration_ms': delay,
                 'origin': props.get('origin', [0, 0]), 'size': list(original.size),
                 'source': 'Mob.nx/' + source + '/' + str(number),
                 'source_properties': props,
                 'alpha_start': int(props.get('a0', 255)), 'alpha_end': int(props.get('a1', 255)),
                 'alpha_nonzero_bounds': list(alpha.getbbox()) if alpha.getbbox() else None,
                 'rgba_sha256': hashlib.sha256(original.tobytes()).hexdigest()}
        if 'delay' not in props:
            frame['duration_note'] = 'Static source frame has no explicit delay; existing 100 ms renderer fallback.'
        if isinstance(props.get('delay'), str):
            frame['duration_note'] = 'Source string-typed decimal delay parsed to milliseconds; source string retained in source_properties.'
        frames.append(frame)
    sprite = {'source': 'Mob.nx/' + source, 'frames': frames,
              'loop': key.endswith(('.idle', '.move')) and len(frames) > 1,
              'total_duration_ms': sum(frame['duration_ms'] for frame in frames),
              'default_facing': 'left', 'source_sequence_properties': properties(nx, node)}
    if reused:
        sprite['reuses_existing_pngs'] = True
    return sprite


def make_contact_sheet(output, sprites, visuals, specs, target, columns, cell):
    width, height = cell
    sheet = Image.new('RGB', (width * columns, height * math.ceil(len(specs) / columns)), (29, 43, 36))
    draw = ImageDraw.Draw(sheet)
    for i, (key, mob_id, *_rest) in enumerate(specs):
        visual = visuals[key]
        frame = sprites[key + '.idle']['frames'][0]
        x, y = (i % columns) * width, (i // columns) * height
        ground = (x + width // 2, y + height - 25)
        scale = visual['suggested_render_scale']
        radius = visual['suggested_footprint_radius']
        draw.text((x + 8, y + 5), key + '  ' + str(mob_id), fill=(246, 225, 178))
        draw.text((x + 8, y + 21), 'scale ' + str(scale) + ' / footprint ' + str(radius), fill=(175, 196, 181))
        draw.ellipse((ground[0] - radius, ground[1] - radius * .35,
                      ground[0] + radius, ground[1] + radius * .35), fill=(17, 24, 20), outline=(114, 150, 123))
        with Image.open(output / frame['file']) as original:
            sample = original.resize((round(original.width * scale), round(original.height * scale)), Image.Resampling.NEAREST)
            sheet.paste(sample, (round(ground[0] - frame['origin'][0] * scale),
                                 round(ground[1] - frame['origin'][1] * scale)), sample)
        draw.line((ground[0] - 4, ground[1], ground[0] + 4, ground[1]), fill=(236, 179, 95))
        draw.line((ground[0], ground[1] - 4, ground[0], ground[1] + 4), fill=(236, 179, 95))
    target.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(target)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path, help='Directory containing Mob.nx and String.nx')
    parser.add_argument('--output', type=Path, default=Path(__file__).resolve().parents[1] / 'assets')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    manifest_path = args.output / 'manifest.json'
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    old_sprites = manifest['sprites']
    paths = {name: args.source / (name + '.nx') for name in ['Mob', 'String']}
    before = {name: fingerprint(path) for name, path in paths.items()}
    archives, sprites, visuals = {}, {}, {}
    try:
        archives = {name: NX(path) for name, path in paths.items()}
        mobs = archives['Mob']
        for key, mob_id, label, scale, radius, old_prefix, theme, behavior in BOSSES + ENEMIES:
            source = f'{mob_id:07d}.img'
            actions = mobs.children(resolve(mobs, source))
            original_name, name_source = mob_name(archives['String'], mob_id)
            action_map = {'idle': 'stand' if 'stand' in actions else 'fly',
                          'move': 'move' if 'move' in actions else 'fly' if 'fly' in actions else 'stand',
                          'hit': 'hit1', 'die': 'die1'}
            if 'attack1' in actions:
                action_map['attack1'] = 'attack1'
            elif 'skill1' in actions:
                action_map['attack1'] = 'skill1'
            if 'attack2' in actions:
                action_map['attack2'] = 'attack2'
            elif 'skill1' in actions and action_map.get('attack1') != 'skill1':
                action_map['attack2'] = 'skill1'
            for runtime_action, source_action in action_map.items():
                runtime_key = key + '.' + runtime_action
                existing = old_sprites.get(old_prefix + '.' + runtime_action) if old_prefix else None
                sprite = export_sequence(mobs, args.output, runtime_key, source + '/' + source_action, existing)
                sprite.update(name=label, source_name=original_name, source_name_source=name_source,
                              source_mob_id=mob_id, source_action=source_action)
                if existing:
                    sprite['reuses_pngs_from'] = old_prefix + '.' + runtime_action
                sprites[runtime_key] = sprite
            info = properties(mobs, actions['info'])
            idle_frame = sprites[key + '.idle']['frames'][0]
            visual = {'name': label, 'source_mob_id': mob_id, 'source_name': original_name,
                      'source': 'Mob.nx/' + source, 'source_name_source': name_source,
                      'source_tagged_boss': bool(info.get('boss', 0)),
                      'suggested_render_scale': scale, 'source_origin_to_ground_px': [0, 0],
                      'suggested_footprint_radius': radius, 'suggested_region': theme,
                      'prototype_behavior_cue': behavior, 'actions': action_map,
                      'idle_source_origin': idle_frame['origin'], 'idle_source_size': idle_frame['size'],
                      'ground_note': 'Original frame origin is a stable ground anchor across animation. Preserve it and mirror for facing; do not center individual frames or derive collision from image bounds.',
                      'mechanics_note': 'Prototype behavior, circular footprint, phase rules, rewards and encounter pool are custom; original source collision and AI are not imported.'}
            if mob_id in (3230101, 3000001):
                visual['ground_note'] += ' Source art floats above its native ground anchor; retain this gap (11 px ghost / 21 px fairy in first stand frame).'
            if key == 'boss_balrog':
                visual['ground_note'] += ' Large wings extend above the body; the suggested footprint belongs to the body at the ground anchor, not the full wing canvas.'
            if key == 'boss_golem':
                visual['mechanics_note'] += ' The source Castle Golem is an ordinary mob promoted to an explicitly custom commander encounter.'
            visuals[key] = visual
    finally:
        for archive in archives.values():
            archive.m.close()
            archive.f.close()
        after = {name: fingerprint(path) for name, path in paths.items()}
        if before != after:
            raise RuntimeError('Original archive size/mtime changed during extraction')
    files = {frame['file']: frame for sprite in sprites.values() for frame in sprite['frames']}
    new_files = {path: frame for path, frame in files.items() if path.startswith('sprites/enemy_appearances/')}
    metadata = {'schema_version': 1, 'sprites': sprites, 'visuals': visuals,
                'source_archives': {name + '.nx': details for name, details in before.items()},
                'validation': {'boss_appearances': len(BOSSES), 'enemy_appearances': len(ENEMIES),
                               'sprite_keys': len(sprites), 'new_pngs': len(new_files),
                               'reused_pngs': len(files) - len(new_files),
                               'new_png_bytes': sum((args.output / path).stat().st_size for path in new_files),
                               'new_rgba_bytes': sum(frame['size'][0] * frame['size'][1] * 4 for frame in new_files.values()),
                               'original_archives_unchanged': True, 'all_pngs_pixel_equal': True},
                'notes': ['Original MapleStory art belongs to its rights holders; this unofficial prototype does not establish redistribution rights.',
                          'No original archive was edited. Runtime PNG pixels, source dimensions and frame origins are unchanged.',
                          'Source delays are parsed according to their NX scalar type, including decimal strings. Alpha interpolation endpoints are retained.',
                          'The existing seven appearance packs reuse their existing PNGs; source metadata is read again to retain exact timing and alpha.',
                          'Footprint sizes are authored circular ground footprints, not source canvas dimensions or copied source collision rectangles.',
                          'Boss and enemy contact sheets are inspection records, never runtime UI art.']}
    (args.output / 'enemy_appearances.json').write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    manifest['sprites'].update(sprites)
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    report = {key: value for key, value in metadata.items() if key != 'sprites'}
    report['sequences'] = [{'key': key, 'source': sprite['source'], 'frames': len(sprite['frames']),
                            'duration_ms': sprite['total_duration_ms'], 'reused_pngs': sprite.get('reuses_existing_pngs', False)}
                           for key, sprite in sprites.items()]
    (root / 'docs/assets/enemy_sources.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    make_contact_sheet(args.output, sprites, visuals, BOSSES, root / 'artifacts/assets/enemy_bosses_contact.png', 3, (300, 270))
    make_contact_sheet(args.output, sprites, visuals, ENEMIES, root / 'docs/enemy_appearances_contact.png', 4, (220, 160))
    print(json.dumps(metadata['validation'], ensure_ascii=True))


if __name__ == '__main__':
    main()
