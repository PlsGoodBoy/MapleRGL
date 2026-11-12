"""Export bounded Combat animations and icons from read-only Skill/Item.nx.

Only new assets/sprites/combat paths and combat_assets.json are written. Pixel
bytes, frame origins, source delays and alpha metadata are kept unchanged.
Use the existing NX reader, as in extract_phoenix.py. No atlas, resizing, tinting
or original archive repacking is performed.
"""
from pathlib import Path
import argparse
import json

from PIL import Image
from extract_local_mobs import NX


ITEM_SPECS = [
    ('item.shield', 'Consume/0202.img/02022269/info/icon', 2022269, 'Defensive Film', 'Temporary invulnerability shield'),
    ('item.bomb', 'Etc/0403.img/04032929/info/icon', 4032929, 'Bomb', 'Instant area clear'),
    ('item.double_xp', 'Cash/0521.img/05211000/info/icon', 5211000, 'Double EXP coupon', 'Temporary double experience'),
    ('item.power', 'Consume/0202.img/02022259/info/icon', 2022259, 'Attack Crystal', 'Temporary attack boost'),
    ('item.reset', 'Cash/0550.img/05500001/info/icon', 5500001, 'Magic Hourglass', 'Reset active skill cooldowns'),
    ('item.chest', 'Etc/0428.img/04280000/info/icon', 4280000, 'Golden Chest', 'Random reward chest'),
]

SPECS = [
    ('skill.rain', 'rain', '311.img/skill/3111004/special/0', 'Arrow Rain',
     {'forward_angle_degrees': 112, 'placement': 'falling_arrow',
      'direction_note': 'Original cyan arrow points down-left, approximately 112 degrees (0 right, 90 down).'}),
    ('skill.rain_hit', 'rain_hit', '311.img/skill/3111004/hit/0', 'Arrow Rain impact', {}),
    ('skill.frost', 'frost', '221.img/skill/2211002/effect', 'Ice Strike', {}),
    ('skill.frost_hit', 'frost_hit', '221.img/skill/2211002/hit/0', 'Ice Strike shatter', {}),
    ('skill.meteor_full', 'meteor', '212.img/skill/2121007/hit/0', 'Meteor full descent',
     {'impact_start_ms': 1680, 'placement': 'fixed_impact_point',
      'render_note': 'Full original descent, impact and fade. Gameplay may use skill.meteor after its own warning delay.'}),
    ('skill.rune', 'rune', '232.img/skill/2321001/effect', 'Big Bang magic circle',
     {'placement': 'fixed_world', 'render_note': 'One 600 ms pulse, not an infinitely looping field.'}),
    ('skill.charge', 'charge', '112.img/skill/1121006/effect', 'Rush',
     {'forward_angle_degrees': 180, 'default_facing': 'left',
      'placement': 'follow_charging_player',
      'direction_note': 'Bright leading tip points left; long golden trail extends right.'}),
    ('skill.blade_wave', 'blade_wave', '1110.img/skill/11101004/ball', 'Soul Blade projectile',
     {'forward_angle_degrees': 180, 'default_facing': 'left',
      'placement': 'projectile', 'loop': True,
      'render_note': 'Original four-frame spinning crescent; loop only during finite projectile lifetime.'}),
    ('skill.blade_wave_hit', 'blade_wave_hit', '1110.img/skill/11101004/hit/0', 'Soul Blade impact', {}),
    ('skill.retaliate', 'retaliate', '110.img/skill/1101007/effect', 'Power Guard counter shield',
     {'placement': 'player_on_counter',
      'render_note': 'A bounded original shield pulse for damage-triggered retaliation, not a persistent shield.'}),
]


def resolve(nx, source):
    node = 0
    for name in source.split('/'):
        node = nx.children(node)[name]
    return node


def export_sequence(nx, output, folder, source):
    frames = []
    for name, node in sorted(nx.children(resolve(nx, source)).items(),
                             key=lambda item: int(item[0]) if item[0].isdigit() else 1000000):
        if not name.isdigit() or nx.node(node)[3] != 5:
            continue
        props = nx.children(node)
        image = nx.bitmap(node)
        relative = 'sprites/combat/' + folder + '/' + name + '.png'
        target = output / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        image.save(target)
        duration = int(nx.value(props['delay'])[0]) if 'delay' in props else 100
        if duration <= 0:
            raise ValueError('Nonpositive source duration at ' + source + '/' + name)
        frame = {'file': relative, 'duration_ms': duration,
                 'origin': list(nx.value(props['origin'])) if 'origin' in props else [0, 0],
                 'size': list(image.size), 'source': 'Skill.nx/' + source + '/' + name}
        if 'delay' not in props:
            frame['duration_note'] = 'No source delay; 100 ms fallback, matching existing extractor.'
        for original, exported in [('a0', 'alpha_start'), ('a1', 'alpha_end')]:
            if original in props:
                frame[exported] = int(nx.value(props[original])[0])
        # Reload every export, verify lossless RGBA preservation and a valid PNG.
        with Image.open(target) as check:
            if check.format != 'PNG' or check.mode != 'RGBA' or check.size != image.size:
                raise ValueError('Invalid exported bitmap: ' + relative)
            if check.tobytes() != image.tobytes():
                raise ValueError('PNG pixel mismatch: ' + relative)
        with Image.open(target) as check:
            check.verify()
        frames.append(frame)
    if not frames:
        raise ValueError('No numeric bitmap frames at ' + source)
    return frames


def export_item_icon(nx, output, key, source, source_id, source_name, gameplay_use):
    node = resolve(nx, source)
    props = nx.children(node)
    image = nx.bitmap(node)
    relative = 'sprites/combat/items/' + key.split('.')[1] + '/0.png'
    target = output / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    image.save(target)
    with Image.open(target) as check:
        if check.format != 'PNG' or check.mode != 'RGBA' or check.tobytes() != image.tobytes():
            raise ValueError('Invalid or changed item icon: ' + relative)
    with Image.open(target) as check:
        check.verify()
    return {'frames': [{'file': relative, 'duration_ms': 100,
                        'origin': [image.width / 2, image.height / 2],
                        'source_origin': list(nx.value(props['origin'])) if 'origin' in props else [0, 0],
                        'size': list(image.size), 'source': 'Item.nx/' + source}],
            'source': 'Item.nx/' + source, 'source_item_id': source_id,
            'source_name': source_name, 'name': gameplay_use, 'loop': False,
            'total_duration_ms': 100,
            'render_note': 'Unmodified original icon, centered for a free-standing pickup. Source item purpose may differ from this game.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('skill_nx', type=Path)
    parser.add_argument('--item-nx', type=Path, help='Defaults to Item.nx next to Skill.nx')
    parser.add_argument('--output', type=Path, default=Path(__file__).resolve().parents[1] / 'assets')
    args = parser.parse_args()
    before = args.skill_nx.stat()
    item_path = args.item_nx or args.skill_nx.with_name('Item.nx')
    item_before = item_path.stat()
    nx = NX(args.skill_nx)
    result = {'sprites': {}, 'notes': [
        'Original MapleStory pixels selectively exported from the user-provided Skill.nx, opened read-only.',
        'Original artwork belongs to its rights holders. This is an unofficial personal prototype; no distribution license is implied.',
        'Original origins, delays and RGBA pixels are preserved. Missing source delays use the existing 100 ms fallback.',
        'Play area/counter/cast effects once per trigger. Projectile animation may loop only until the projectile expires.',
        'Meteor impact reuses full-sequence PNGs; no duplicate files or texture atlas is generated.',
        'Six pickup icons retain original RGBA pixels and source_origin; display origins are centered, matching existing pickup conventions.',
    ]}
    try:
        for key, folder, source, name, extra in SPECS:
            frames = export_sequence(nx, args.output, folder, source)
            result['sprites'][key] = {
                'frames': frames, 'source_skill_id': int(source.split('/')[2]),
                'source': 'Skill.nx/' + source, 'name': name, 'loop': False,
                'total_duration_ms': sum(frame['duration_ms'] for frame in frames), **extra}
        full = result['sprites']['skill.meteor_full']
        frames = [frame for frame in full['frames'] if int(frame['source'].rsplit('/', 1)[1]) >= 14]
        result['sprites']['skill.meteor'] = {
            'frames': frames, 'source_skill_id': 2121007,
            'source': full['source'], 'name': 'Meteor impact only', 'loop': False,
            'source_frame_range': [14, 22], 'source_start_ms': 1680,
            'total_duration_ms': sum(frame['duration_ms'] for frame in frames),
            'placement': 'fixed_impact_point',
            'render_note': 'Play once after the gameplay warning delay. Frames retain original 120 ms timing and origins.'}
    finally:
        nx.m.close()
        nx.f.close()
    item_nx = NX(item_path)
    try:
        for spec in ITEM_SPECS:
            result['sprites'][spec[0]] = export_item_icon(item_nx, args.output, *spec)
    finally:
        item_nx.m.close()
        item_nx.f.close()
    item_after = item_path.stat()
    if (item_before.st_size, item_before.st_mtime_ns) != (item_after.st_size, item_after.st_mtime_ns):
        raise RuntimeError('Source Item.nx changed during extraction')
    after = args.skill_nx.stat()
    if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
        raise RuntimeError('Source archive changed during extraction')
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / 'combat_assets.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    files = {frame['file'] for sprite in result['sprites'].values() for frame in sprite['frames']}
    print(json.dumps({'sprites': {key: {'frames': len(sprite['frames']), 'duration_ms': sprite['total_duration_ms']}
                                  for key, sprite in result['sprites'].items()},
                      'unique_pngs': len(files),
                      'png_bytes': sum((args.output / path).stat().st_size for path in files),
                      'verified': 'All exported PNGs reopened and compared pixel-for-pixel with source RGBA.',
                      'source_unchanged': True}, indent=2))


if __name__ == '__main__':
    main()
