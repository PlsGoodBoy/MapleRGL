#!/usr/bin/env python3
"""Read-only export of original, inventory-sized Fusion HUD icons.

The skill icons are genuine Skill.nx icon canvases, never resized effect frames.
Source names are attribution, not a claim to reproduce original MapleStory skills.
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
from extract_combat_assets import resolve
from extract_equipment_icons import export_icon, fingerprint


SKILLS = [
    ('homing', 2001004, '追踪魔弹'),
    ('arrow', 3001004, '精准射击'),
    ('nova', 1311006, '龙咆哮'),
    ('pierce', 3221001, '穿透箭'),
    ('lightning', 2221006, '连锁闪电'),
    ('zone', 2121003, '火焰领域'),
    ('summon', 3121006, '凤凰'),
    ('cleave', 1001005, '劈空斩'),
    ('orbit', 1120003, '旋风剑刃'),
    ('spread', 3111006, '多重箭'),
    ('rain', 3111004, '箭雨'),
    ('ricochet', 3101005, '弹射箭'),
    ('frost', 2211002, '冰咆哮'),
    ('meteor', 2121007, '陨石'),
    ('rune', 2321001, '魔法阵'),
    ('charge', 1121006, '冲锋斩'),
    ('blade_wave', 11101004, '剑气'),
    ('retaliate', 1101007, '防御反击'),
]

# Fusion and core icons are separate packs.
FUSION_SKILLS = [
    ('star_return', 3221006, '回旋贯星'),
    ('frost_storm', 2221007, '霜雷领域'),
    ('dragon_armor', 1320006, '龙魂战甲'),
]

CONTENT_SKILLS = [
    ('hunter_mark', 3221007, '猎手标记'),
    ('arrow_trap', 3201005, '箭矢陷阱'),
    ('orbital_orb', 2311005, '环绕法球'),
    ('gravity_well', 1321001, '牵引魔法'),
    ('heavy_strike', 1111003, '蓄势重斩'),
    ('bulwark', 1120005, '蓄盾战意'),
    ('phoenix_rain', 3111003, '焚天箭雨'),
    ('hunter_volley', 3121004, '追猎箭阵'),
    ('molten_star', 12111006, '熔星灾变'),
    ('arcane_ring', 12111005, '奥术星环'),
    ('sword_cyclone', 1121008, '万剑回旋'),
    ('dash_breaker', 11111006, '破阵连斩'),
]


def skill_path(skill_id):
    return f'{skill_id // 10000}.img/skill/{skill_id}/icon'


SPECS = [('skill.icon.' + key, 'Skill', skill_path(skill_id), name)
         for key, skill_id, name in SKILLS]
SPECS += [
    ('passive.icon.1', 'Skill', skill_path(1001004), '伤害'),
    ('passive.icon.2', 'Skill', skill_path(3101002), '攻速'),
    ('passive.icon.3', 'Skill', skill_path(2301004), '范围'),
    ('passive.icon.4', 'Skill', skill_path(3001005), '投射物'),
    ('passive.icon.5', 'Skill', skill_path(4101004), '移动速度'),
    ('passive.icon.6', 'Skill', skill_path(1301007), '生命'),
    ('passive.icon.7', 'Character', 'PetEquip/01812000.img/info/icon', '拾取范围'),
    ('passive.icon.8', 'Item', 'Cash/0550.img/05500001/info/icon', '冷却'),
    ('passive.icon.9', 'Skill', skill_path(2121004), '持续时间'),
    ('passive.icon.10', 'Skill', skill_path(3121002), '暴击概率'),
    ('passive.icon.11', 'Skill', skill_path(4100001), '暴击伤害'),
    ('passive.icon.12', 'Item', 'Consume/0200.img/02000000/info/icon', '恢复生命'),
    ('buff.icon.fury', 'Skill', skill_path(1111002), '狂热'),
    ('buff.icon.magnet', 'Character', 'PetEquip/01812000.img/info/icon', '磁铁'),
    ('buff.icon.shield', 'Item', 'Consume/0202.img/02022269/info/icon', '护盾'),
    ('buff.icon.double_xp', 'Item', 'Cash/0521.img/05211000/info/icon', '双倍经验'),
    ('buff.icon.power', 'Item', 'Consume/0202.img/02022259/info/icon', '强化'),
    ('buff.icon.rift', 'Skill', skill_path(2201002), '裂隙'),
    ('buff.icon.curse', 'Skill', skill_path(1201006), '诅咒'),
    ('buff.icon.mud', 'Skill', skill_path(2101003), '泥地减速'),
]


def original_skill_name(strings, path):
    skill_id = int(path.split('/')[2])
    name_path = f'Skill.img/{skill_id:07d}/name'
    node = strings.node(resolve(strings, name_path))
    if node[3] != 3:
        raise ValueError('Expected original skill name: ' + name_path)
    return strings.name(struct.unpack('<I', node[4][:4])[0]), name_path


def contact_sheet(output, sprites, path):
    width, height, columns = 192, 98, min(6, len(sprites))
    sheet = Image.new('RGB', (width * columns, height * math.ceil(len(sprites) / columns)), (25, 40, 34))
    draw = ImageDraw.Draw(sheet)
    for i, (key, sprite) in enumerate(sprites.items()):
        x, y = (i % columns) * width, (i // columns) * height
        draw.text((x + 8, y + 7), key, fill=(239, 224, 188))
        with Image.open(output / sprite['frames'][0]['file']) as image:
            sheet.paste(image, (x + 10, y + 42), image)
            enlarged = image.resize((image.width * 2, image.height * 2), Image.Resampling.NEAREST)
            sheet.paste(enlarged, (x + 88, y + 28), enlarged)
    path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path, help='Directory of original NX archives')
    parser.add_argument('--output', type=Path, default=Path(__file__).resolve().parents[1] / 'assets')
    parser.add_argument('--pack', choices=['core', 'fusions', 'content'], default='core', help='Select the icon pack to export')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    added_skills = FUSION_SKILLS if args.pack == 'fusions' else CONTENT_SKILLS
    specs = SPECS if args.pack == 'core' else [('skill.icon.' + key, 'Skill', skill_path(skill_id), name)
                                            for key, skill_id, name in added_skills]
    source_names = sorted(set(entry[1] for entry in specs) | {'String'})
    paths = {name: args.source / (name + '.nx') for name in source_names}
    before = {name: fingerprint(path) for name, path in paths.items()}
    archives, sprites, exported = {}, {}, {}
    try:
        archives = {name: NX(path) for name, path in paths.items()}
        for key, archive, source_path, game_name in specs:
            source = archive + '.nx/' + source_path
            if source in exported:
                alias_key, frame = exported[source]
            else:
                alias_key = key
                relative = 'sprites/skill_icons/' + key.replace('.', '/') + '/0.png'
                frame = export_icon(archives[archive], args.output, relative, source_path, archive, False)
                exported[source] = (key, frame)
            sprite = {'frames': [frame], 'source': source, 'name': game_name,
                      'loop': False, 'total_duration_ms': frame['duration_ms'],
                      'icon_kind': 'original_skill_icon' if archive == 'Skill' else 'original_inventory_icon'}
            if archive == 'Skill':
                name, name_path = original_skill_name(archives['String'], source_path)
                sprite.update(source_skill_id=int(source_path.split('/')[2]), source_name=name,
                              source_name_source='String.nx/' + name_path)
            if key != alias_key:
                sprite['alias_of'] = alias_key
            sprites[key] = sprite
    finally:
        for archive in archives.values():
            archive.m.close()
            archive.f.close()
        after = {name: fingerprint(path) for name, path in paths.items()}
        if before != after:
            raise RuntimeError('Original archive size/mtime changed during read-only extraction')
    metadata = {'version': 1, 'sprites': sprites,
                'source_archives': {name + '.nx': details for name, details in before.items()},
                'validation': {'original_archives_unchanged': True, 'all_pngs_pixel_equal': True,
                               'unique_pngs': len(exported), 'sprite_keys': len(sprites)},
                'notes': ['Genuine skill/inventory icon canvases; no effect-frame thumbnails or cropped character artwork.',
                          'RGBA and original dimensions retained. HUD display origin is centered; original origin is in source_origin.',
                          'Source names identify original art only; MapleRGL names and mechanics are custom.',
                          'Contact sheet is an inspection artifact only and is not loaded by the game.']}
    if args.pack == 'core':
        metadata['notes'].insert(-1, 'The magnet buff and magnet passive intentionally share the same magnetic pickup icon.')
    elif args.pack == 'fusions':
        metadata['notes'].insert(-1, 'Fusion-result icons use their own original skill icon; no material icon is overwritten or reused.')
    else:
        metadata['notes'].insert(-1, 'The content pack contains six base-skill and six fusion-result icons. All twelve source icons are distinct from each other and from the previous twenty-one skill icons.')
    file_stem = {'core': 'skill_icons', 'fusions': 'fusion_icons', 'content': 'content_icons'}[args.pack]
    sources_name = {'core': 'skill_icon_sources', 'fusions': 'fusion_icon_sources', 'content': 'content_icon_sources'}[args.pack]
    (args.output / (file_stem + '.json')).write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    manifest_path = args.output / 'manifest.json'
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    manifest['sprites'].update(sprites)
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    report = {key: value for key, value in metadata.items() if key != 'sprites'}
    report['icons'] = [{'key': key, 'game_name': value['name'], 'source': value['source'],
                        'file': value['frames'][0]['file'], 'size': value['frames'][0]['size'],
                        'rgba_sha256': value['frames'][0]['rgba_sha256']}
                       for key, value in sprites.items()]
    (root / 'docs/assets' / (sources_name + '.json')).write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    contact_sheet(args.output, sprites, root / 'artifacts/assets' / (file_stem + '_contact.png'))
    print(json.dumps(metadata['validation'], ensure_ascii=True))


if __name__ == '__main__':
    main()
