#!/usr/bin/env python3
"""Export the Progression relic/equipment icons from existing read-only NX archives.

Usage: python tools/extract_equipment_icons.py <nxFiles> [--verify-only]
Uses the project's existing NX reader and node-path resolver. No original game
archive is modified, no image is rescaled, and no gameplay meaning is imported.
"""
import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path

sys.dont_write_bytecode = True
from PIL import Image
from extract_local_mobs import NX
from extract_combat_assets import resolve

# Stable presentation keys; these visual cues are not original item effects.
# All item names are read verbatim from String.nx instead of guessed/transliterated.
SPECS = [
    ('relic.icon.0', 'Item', 'Consume/0206.img/02060000/info/icon', 2060000, 'arrow'),
    ('relic.icon.1', 'Item', 'Etc/0400.img/04003002/info/icon', 4003002, 'ice'),
    ('relic.icon.2', 'Item', 'Consume/0207.img/02070019/info/icon', 2070019, 'lightning'),
    ('relic.icon.3', 'Item', 'Etc/0400.img/04000232/info/icon', 4000232, 'fire'),
    ('relic.icon.4', 'Character', 'Shield/01092000.img/info/icon', 1092000, 'shield'),
    ('relic.icon.5', 'Character', 'Shoes/01072001.img/info/icon', 1072001, 'dash_boots'),
    ('relic.icon.6', 'Item', 'Consume/0210.img/02100001/info/icon', 2100001, 'summoning_bag'),
    ('relic.icon.7', 'Character', 'PetEquip/01812000.img/info/icon', 1812000, 'magnet'),
    ('relic.icon.8', 'Item', 'Etc/0400.img/04000279/info/icon', 4000279, 'flower'),
    ('relic.icon.9', 'Item', 'Etc/0400.img/04000005/info/icon', 4000005, 'leaf'),
    ('relic.icon.10', 'Item', 'Etc/0400.img/04005001/info/icon', 4005001, 'crystal'),
    ('relic.icon.11', 'Item', 'Cash/0550.img/05500001/info/icon', 5500001, 'hourglass'),
    ('equipment.icon.0', 'Character', 'Weapon/01302000.img/info/icon', 1302000, 'sword'),
    ('equipment.icon.1', 'Character', 'Weapon/01452000.img/info/icon', 1452000, 'bow'),
    ('equipment.icon.2', 'Character', 'Weapon/01382000.img/info/icon', 1382000, 'staff'),
    ('equipment.icon.3', 'Character', 'Cap/01002000.img/info/icon', 1002000, 'cap'),
    ('equipment.icon.4', 'Character', 'Shield/01092000.img/info/icon', 1092000, 'shield'),
    ('equipment.icon.5', 'Character', 'Glove/01082002.img/info/icon', 1082002, 'gloves'),
    ('equipment.icon.6', 'Character', 'Shoes/01072001.img/info/icon', 1072001, 'boots'),
    ('equipment.icon.7', 'Character', 'Ring/01112000.img/info/icon', 1112000, 'ring'),
    ('equipment.icon.8', 'Character', 'Accessory/01032000.img/info/icon', 1032000, 'earrings'),
]


def fingerprint(path):
    value = path.stat()
    return {'size_bytes': value.st_size, 'mtime_ns': value.st_mtime_ns}


def source_name(strings, archive, path, item_id):
    category = path.split('/')[0]
    if archive == 'Character':
        name_path = f'Eqp.img/Eqp/{category}/{item_id}/name'
    else:
        folder = 'Etc/' if category == 'Etc' else ''
        name_path = f'{category}.img/{folder}{item_id}/name'
    node = strings.node(resolve(strings, name_path))
    if node[3] != 3:
        raise ValueError('Expected original string node: ' + name_path)
    index = struct.unpack('<I', node[4][:4])[0]
    return strings.name(index), 'String.nx/' + name_path


def export_icon(nx, output, relative, path, archive, verify_only):
    node = resolve(nx, path)
    if nx.node(node)[3] != 5:
        raise ValueError('Expected an original bitmap: ' + path)
    props = nx.children(node)
    bitmap = nx.bitmap(node)
    if bitmap.mode != 'RGBA' or not (20 <= bitmap.width <= 40 and 20 <= bitmap.height <= 40):
        raise ValueError('Icon is not an unscaled inventory-sized RGBA image: ' + path)
    target = output / relative
    if not verify_only:
        target.parent.mkdir(parents=True, exist_ok=True)
        bitmap.save(target)
    with Image.open(target) as check:
        if check.format != 'PNG' or check.mode != 'RGBA' or check.size != bitmap.size:
            raise ValueError('Invalid PNG metadata: ' + str(target))
        if check.tobytes() != bitmap.tobytes():
            raise ValueError('PNG does not preserve source RGBA pixels: ' + str(target))
    with Image.open(target) as check:
        check.verify()
    duration = int(nx.value(props['delay'])[0]) if 'delay' in props else 100
    if duration <= 0:
        raise ValueError('Nonpositive source icon duration: ' + path)
    frame = {
        'file': relative,
        'duration_ms': duration,
        'origin': [bitmap.width / 2, bitmap.height / 2],
        'source_origin': list(nx.value(props['origin'])) if 'origin' in props else [0, 0],
        'size': list(bitmap.size),
        'source': archive + '.nx/' + path,
        'alpha_start': int(nx.value(props['a0'])[0]) if 'a0' in props else 255,
        'alpha_end': int(nx.value(props['a1'])[0]) if 'a1' in props else 255,
        'rgba_sha256': hashlib.sha256(bitmap.tobytes()).hexdigest(),
    }
    if 'delay' not in props:
        frame['duration_note'] = 'Static source icon has no delay; 100 ms is the existing renderer fallback.'
    if 'a0' not in props and 'a1' not in props:
        frame['alpha_note'] = 'No source alpha interpolation; full-opacity endpoints preserve original per-pixel RGBA alpha.'
    return frame


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path, help='Directory containing Item.nx, Character.nx and String.nx')
    parser.add_argument('--output', type=Path, default=Path(__file__).resolve().parents[1] / 'assets')
    parser.add_argument('--verify-only', action='store_true', help='Verify existing PNGs/JSON against NX without writing anything')
    args = parser.parse_args()
    paths = {name: args.source / (name + '.nx') for name in ['Item', 'Character', 'String']}
    before = {name: fingerprint(path) for name, path in paths.items()}
    archives = {}
    result = {
        'version': 1,
        'sprites': {},
        'source_archives': {name + '.nx': details for name, details in before.items()},
        'notes': [
            'Original MapleStory inventory icons selectively read from user-provided NX archives using the existing read-only NX exporter.',
            'Original artwork belongs to its rights holders. This unofficial personal prototype does not imply a redistribution license.',
            'Original RGBA pixels, source origins, frame delay and alpha interpolation are preserved; static missing delays use the established 100 ms fallback.',
            'Display origins are centered for inventory cards and free-standing pickups. Original source origins remain in source_origin.',
            'source_name is verbatim String.nx attribution only. Gameplay names, stats, rarity and effects are authored by the game, not claims about the original MapleStory items.',
            'Shield and boots are intentionally shared between relic and equipment presentation keys; the duplicate keys refer to the same PNG.',
        ],
    }
    exported = {}
    try:
        for name, path in paths.items():
            archives[name] = NX(path)
        for key, archive, path, item_id, cue in SPECS:
            source = archive + '.nx/' + path
            name, name_path = source_name(archives['String'], archive, path, item_id)
            if source in exported:
                original_key, frame = exported[source]
            else:
                original_key = key
                relative = 'sprites/equipment_icons/' + key.replace('.', '/') + '/0.png'
                frame = export_icon(archives[archive], args.output, relative, path, archive, args.verify_only)
                exported[source] = (key, frame)
            sprite = {
                'frames': [frame],
                'source': source,
                'source_item_id': item_id,
                'source_name': name,
                'source_name_source': name_path,
                'visual_cue': cue,
                'loop': False,
                'total_duration_ms': frame['duration_ms'],
            }
            if original_key != key:
                sprite['alias_of'] = original_key
            result['sprites'][key] = sprite
    finally:
        for nx in archives.values():
            nx.m.close()
            nx.f.close()
        after = {name: fingerprint(path) for name, path in paths.items()}
        if before != after:
            raise RuntimeError('Original NX source size/mtime changed during extraction or verification')
    result['validation'] = {
        'original_archives_unchanged': True,
        'all_pngs_reopened_and_pixel_equal': True,
        'unique_pngs': len(exported),
        'sprite_keys': len(result['sprites']),
    }
    metadata_path = args.output / 'equipment_icons.json'
    if args.verify_only:
        if json.loads(metadata_path.read_text(encoding='utf-8')) != result:
            raise ValueError('Existing equipment_icons.json differs from current source metadata')
    else:
        args.output.mkdir(parents=True, exist_ok=True)
        metadata_path.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({
        **result['validation'],
        'mode': 'verify-only' if args.verify_only else 'export',
        'png_bytes': sum((args.output / frame['file']).stat().st_size for _, frame in exported.values()),
        'keys': list(result['sprites']),
        'metadata': str(metadata_path),
    }, ensure_ascii=True, indent=2))


if __name__ == '__main__':
    main()