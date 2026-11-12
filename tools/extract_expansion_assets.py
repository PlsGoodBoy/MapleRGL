#!/usr/bin/env python3
"""Selectively export Expansion art from read-only local NX archives.

Only the named candidates are decoded. This exports original pixels and resource
metadata; it does not register completed combat behavior or enable boss pools.
"""
import argparse
import hashlib
import json
import math
import sys
from pathlib import Path

sys.dont_write_bytecode = True
from PIL import Image, ImageDraw
from extract_local_mobs import NX
from extract_regions import resolve, properties
from extract_equipment_icons import export_icon, fingerprint
from extract_scene_props import check_png
from extract_skill_icons import original_skill_name, contact_sheet
from extract_enemy_appearances import mob_name, make_contact_sheet


UTILITY_SKILLS = [
    ('flame_dance', 1211003, '烈焰剑舞'),
    ('feather_storm', 4111005, '剑羽风暴'),
    ('star_bow', 13111006, '星轨魔弓'),
]
UTILITY_EFFECTS = [
    ('skill.flame_dance.cast', 1211003, 'effect', False),
    ('skill.feather_storm.cast', 4111005, 'effect', False),
    ('skill.feather_storm.projectile', 4111005, 'ball', False),
    ('skill.star_bow.projectile', 13111006, 'ball', True),
    ('skill.star_bow.hit', 13111006, 'hit/0', False),
]

CLASS_SKILLS = [
    ('battle_focus', 1111002, '斗气聚合'),
    ('spear_combo', 1311001, '枪连击'),
    ('holy_shield', 1220006, '圣盾回旋'),
    ('judgement', 1221011, '圣剑审判'),
    ('dragon_thrust', 5121001, '龙魂突刺'),
    ('earth_shatter', 5111006, '地裂重击'),
    ('poison_mist', 2111003, '毒雾'),
    ('ice_lance', 2201004, '冰枪'),
    ('holy_light', 2311004, '圣光'),
    ('holy_dragon', 2321003, '圣龙召唤'),
    ('fire_beast', 2221005, '火魔兽'),
    ('magic_vortex', 2121001, '魔力漩涡'),
    ('storm_shot', 13101005, '暴风连射'),
    ('sniper', 3000000, '狙击'),
    ('ice_arrow', 3211003, '寒冰箭'),
    ('blast_arrow', 5201002, '爆炸箭'),
    ('silver_eagle', 3111005, '银鹰'),
    ('decoy', 3111002, '诱敌草人'),
]
EFFECTS_CLASS_SKILLS = [
    ('skill.battle_focus.state', 1111002, 'state', True),
    ('skill.spear_combo.cast', 1311001, 'effect0', False),
    ('skill.spear_combo.hit', 1311001, 'hit/0', False),
    ('skill.holy_shield.cast', 1220006, 'special', False),
    ('skill.judgement.cast', 1221011, 'effect', False),
    ('skill.judgement.hit', 1221011, 'hit/0', False),
    ('skill.dragon_thrust.cast', 5121001, 'effect', False),
    ('skill.dragon_thrust.hit', 5121001, 'hit/0', False),
    ('skill.earth_shatter.cast', 5111006, 'effect', False),
    ('skill.earth_shatter.hit', 5111006, 'hit/0', False),
    ('skill.poison_mist.zone', 2111003, 'tile/0', True),
    ('skill.ice_lance.hit', 2201004, 'hit/0', False),
    ('skill.holy_light.cast', 2311004, 'effect', False),
    ('skill.magic_vortex.cast', 2121001, 'effect', False),
    ('skill.storm_shot.cast', 13101005, 'effect', False),
    ('skill.sniper.projectile', 3221007, 'ball', True),
    ('skill.sniper.hit', 3221007, 'hit/0', False),
    ('skill.ice_arrow.cast', 3211003, 'effect', False),
    ('skill.ice_arrow.hit', 3211003, 'hit/0', False),
    ('skill.blast_arrow.hit', 5201002, 'hit/0', False),
]
for _key, _id, _motion in [('holy_dragon', 2321003, 'fly'),
                          ('fire_beast', 2221005, 'move'),
                          ('silver_eagle', 3111005, 'fly')]:
    EFFECTS_CLASS_SKILLS += [
        ('skill.' + _key + '.idle', _id, 'summon/stand', True),
        ('skill.' + _key + '.move', _id, 'summon/' + _motion, True),
        ('skill.' + _key + '.attack', _id, 'summon/attack1', False),
        ('skill.' + _key + '.die', _id, 'summon/die', False),
        ('skill.' + _key + '.spawn', _id, 'summon/summoned', False),
        ('skill.' + _key + '.hit', _id, 'hit/0', False),
    ]
EFFECTS_CLASS_SKILLS += [
    ('skill.decoy.idle', 3111002, 'summon/stand', True),
    ('skill.decoy.hit', 3111002, 'summon/hit', False),
    ('skill.decoy.die', 3111002, 'summon/die', False),
    ('skill.decoy.spawn', 3111002, 'summon/summoned', False),
]

FUSION_SKILLS = [
    ('battle_blades', 1111004, '斗气狂刃'),
    ('dragon_formation', 1311005, '龙魂贯阵'),
    ('echo_shield', 2321005, '回响圣盾'),
    ('mountain_breaker', 5101004, '破阵山河'),
    ('spirit_judgement', 1221003, '斗气圣裁'),
    ('azure_rift', 3221003, '苍龙裂空'),
    ('sanctuary', 2311003, '圣域裁决'),
    ('poison_flare', 2111006, '瘴炎连爆'),
    ('thunder_lance', 2211003, '雷霆冰枪'),
    ('heaven_dragon', 2311006, '天国龙辉'),
    ('inferno_array', 2111002, '炎魔陨阵'),
    ('venom_vortex', 2111004, '蚀骨旋涡'),
    ('frost_fire', 2121005, '霜火双界'),
    ('purity_well', 2301002, '净化星井'),
    ('thousand_arrows', 3120005, '千矢风暴'),
    ('eagle_execution', 3121002, '鹰眼绝杀'),
    ('frozen_barrage', 2221003, '冰封箭幕'),
    ('siege_bolt', 5211004, '破城轰矢'),
    ('trick_decoy', 4121004, '诡雷草人'),
    ('twin_hunt', 3211005, '双翼巡猎'),
    ('prism_shot', 5201001, '折光狙杀'),
]

CROSS_FUSIONS = [
    ('thunder_prison', 1211007, '雷霆剑狱'),
    ('frost_armor', 2001002, '霜甲反击'),
    ('miasma_dragon', 1311008, '瘴龙吐息'),
    ('divine_verdict', 2321008, '神圣裁决'),
    ('frozen_dragon_spear', 1211005, '极寒龙枪'),
    ('phoenix_assault', 5211002, '凤翼突袭'),
    ('mirror_shield', 1001003, '镜盾回射'),
    ('dragon_breaker', 1111008, '龙吼破军'),
    ('fissure_arrows', 3211004, '裂地箭阵'),
    ('spear_bolt', 1300000, '枪弩连阵'),
    ('storm_arrow_stream', 2221004, '雷暴箭流'),
    ('poison_hunting_ground', 4221006, '毒沼猎场'),
    ('dragon_phoenix', 12001004, '龙凤巡天'),
    ('glacial_sniper', 2211006, '极寒狙击'),
    ('flowing_fire', 12111003, '流火连爆'),
]

# These are art references, not an implementation of the fusion's behavior.
# Its timings, targeting, state changes and cleanup remain in the combat core.
REUSE_SUGGESTIONS = {
    'battle_blades': ['skill.slash', 'skill.battle_focus.state'],
    'dragon_formation': ['skill.dragon_thrust.cast', 'skill.nova'],
    'echo_shield': ['skill.holy_shield.cast', 'skill.retaliate'],
    'mountain_breaker': ['skill.charge', 'skill.earth_shatter.impact'],
    'spirit_judgement': ['skill.battle_focus.state', 'skill.judgement.cast'],
    'azure_rift': ['skill.blade_wave', 'skill.dragon_thrust.cast'],
    'sanctuary': ['skill.holy_shield.cast', 'skill.judgement.cast'],
    'poison_flare': ['skill.poison_mist.zone', 'skill.blast_arrow.hit'],
    'thunder_lance': ['skill.ice_lance.hit', 'skill.lightning'],
    'heaven_dragon': ['skill.holy_dragon.attack', 'skill.holy_light.cast'],
    'inferno_array': ['skill.fire_beast.attack', 'skill.meteor'],
    'venom_vortex': ['skill.poison_mist.zone', 'skill.magic_vortex.cast'],
    'frost_fire': ['skill.frost', 'skill.fire_beast.attack'],
    'purity_well': ['skill.magic_vortex.cast', 'skill.holy_light.cast'],
    'thousand_arrows': ['skill.arrow', 'skill.storm_shot.cast'],
    'eagle_execution': ['skill.silver_eagle.attack', 'skill.sniper.projectile'],
    'frozen_barrage': ['skill.rain', 'skill.ice_arrow.hit'],
    'siege_bolt': ['skill.pierce', 'skill.blast_arrow.hit'],
    'trick_decoy': ['skill.decoy.idle', 'skill.blast_arrow.hit'],
    'twin_hunt': ['skill.summon', 'skill.silver_eagle.move'],
    'prism_shot': ['skill.sniper.projectile', 'skill.star_bow.hit'],
    'thunder_prison': ['skill.blade_wave', 'skill.lightning'],
    'frost_armor': ['skill.holy_shield.cast', 'skill.frost_hit'],
    'miasma_dragon': ['skill.dragon_thrust.cast', 'skill.poison_mist.zone'],
    'divine_verdict': ['skill.judgement.cast', 'skill.holy_light.cast'],
    'frozen_dragon_spear': ['skill.spear_combo.thrust', 'skill.ice_lance.hit'],
    'phoenix_assault': ['skill.charge', 'skill.summon'],
    'mirror_shield': ['skill.holy_shield.cast', 'skill.arrow'],
    'dragon_breaker': ['skill.nova', 'skill.sniper.projectile'],
    'fissure_arrows': ['skill.earth_shatter.impact', 'skill.rain'],
    'spear_bolt': ['skill.spear_combo.thrust', 'skill.pierce'],
    'storm_arrow_stream': ['skill.storm_shot.cast', 'skill.lightning'],
    'poison_hunting_ground': ['skill.decoy.idle', 'skill.poison_mist.zone'],
    'dragon_phoenix': ['skill.holy_dragon.move', 'skill.summon'],
    'glacial_sniper': ['skill.sniper.projectile', 'skill.ice_arrow.hit'],
    'flowing_fire': ['skill.flame_dance.cast', 'skill.blast_arrow.hit'],
}

# Runtime prefix, Mob.nx ID, authored label, scale before camera zoom,
# circular ground footprint radius, optional old prefix, theme, behavior cue.
# These remain resource proposals until the corresponding core behavior ships.
BOSSES = [
    ('boss_snowman', 8220001, '驮狼雪人王', .95, 44, None, 'snow', 'snowball_and_slam'),
    ('boss_pirate', 9300119, '海盗船长', 1.00, 42, None, 'pirate', 'cannon_and_fan_fire'),
    ('boss_clockwork', 8500001, '玩具机械统领', .65, 42, None, 'clockwork', 'directional_beam_and_zones'),
    ('boss_leviathan', 8220003, '苍海龙王', .75, 40, None, 'dragon', 'breath_and_sweep'),
    ('boss_cactus', 3220001, '沙漠仙人掌王', 1.15, 39, None, 'desert', 'spines_and_sand_zones'),
    ('boss_faust', 5220002, '浮士德', .70, 32, None, 'forest', 'hex_and_summon'),
]
ENEMIES = [
    ('enemy.crocodile', 3110100, '鳄鱼', 1.00, 22, None, 'forest', 'rush'),
    ('enemy.toy_mouse', 3210205, '玩具黑鼠', 1.10, 18, None, 'clockwork', 'rush'),
    ('enemy.robot', 5110301, '洛伊德', 1.10, 17, None, 'clockwork', 'armored'),
    ('enemy.yeti', 5100000, '小白雪人', 1.20, 18, None, 'snow', 'pursuit'),
    ('enemy.moon_pixie', 4230106, '月光精灵', 1.00, 16, None, 'forest/snow', 'support_ranged'),
    ('enemy.drake', 5130100, '青龙', .85, 27, None, 'wasteland', 'charge'),
    ('enemy.zombie', 5130107, '僵尸', .90, 20, None, 'wasteland', 'ground_control'),
    ('enemy.panda', 6130203, '柔道猫熊', 1.00, 24, None, 'forest', 'telegraphed_charge'),
    ('enemy.sand_giant', 4230600, '沙漠巨人', .85, 27, None, 'desert', 'armored'),
    ('enemy.stone_bug', 5250001, '石头虫', .80, 26, None, 'forest/wasteland', 'splitter'),
    ('enemy.skeleton_dog', 4230125, '骷髅犬', 1.00, 18, None, 'wasteland', 'rush'),
    ('enemy.hedgehog', 4230501, '豪猪', 1.10, 20, None, 'forest', 'ground_control'),
    ('enemy.bubble_fish', 3230302, '吹泡泡双鱼', 1.15, 18, None, 'clockwork', 'ranged'),
    ('enemy.pirate_crew', 9300121, '海盗克鲁', .80, 25, None, 'pirate', 'ranged'),
]


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def export_sequence(nx, output, key, archive, source, loop=False, skip_zero_delay=False):
    node = resolve(nx, source)
    children = sorted((int(name), index) for name, index in nx.children(node).items()
                      if name.isdigit() and nx.node(index)[3] == 5)
    if not children:
        raise ValueError('No original bitmap sequence: ' + source)
    frames = []
    for number, frame_node in children:
        props = properties(nx, frame_node)
        bitmap = nx.bitmap(frame_node)
        duration = int(props.get('delay', 100))
        if duration == 0 and skip_zero_delay:
            continue  # Terminal zero-time source frames do not occupy a display interval.
        if duration <= 0:
            raise ValueError('Nonpositive frame delay: ' + source)
        relative = 'expansion/sequences/' + key.replace('.', '/') + f'/{number}.png'
        target = output / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        bitmap.save(target)
        check_png(target, bitmap)
        alpha = bitmap.getchannel('A')
        frame = {'file': relative, 'duration_ms': duration,
                 'origin': props.get('origin', [0, 0]), 'size': list(bitmap.size),
                 'source': archive + '.nx/' + source + '/' + str(number),
                 'source_properties': props, 'alpha_start': int(props.get('a0', 255)),
                 'alpha_end': int(props.get('a1', 255)),
                 'alpha_nonzero_bounds': list(alpha.getbbox()) if alpha.getbbox() else None,
                 'rgba_sha256': hashlib.sha256(bitmap.tobytes()).hexdigest()}
        if 'delay' not in props:
            frame['duration_note'] = 'Static source frame has no delay; 100 ms is the existing renderer fallback.'
        if isinstance(props.get('delay'), str):
            frame['duration_note'] = 'Source decimal-string delay parsed as milliseconds; original string retained.'
        frames.append(frame)
    return {'source': archive + '.nx/' + source, 'frames': frames, 'loop': loop,
            'total_duration_ms': sum(frame['duration_ms'] for frame in frames),
            'source_sequence_properties': properties(nx, node)}


def effects_contact(output, sprites, target):
    width, height, columns = 210, 180, 4
    sheet = Image.new('RGB', (width * columns, height * len(sprites)), (28, 41, 35))
    draw = ImageDraw.Draw(sheet)
    for row, (key, sprite) in enumerate(sprites.items()):
        frames = sprite['frames']
        selected = sorted(set([0, len(frames) // 3, len(frames) * 2 // 3, len(frames) - 1]))
        for column, index in enumerate(selected):
            x, y = column * width, row * height
            frame = frames[index]
            draw.text((x + 6, y + 6), key + '/' + str(index), fill=(240, 225, 185))
            with Image.open(output / frame['file']) as bitmap:
                # Contact-sheet scaling only. Runtime PNGs remain unmodified.
                scale = min(1.0, (width - 12) / bitmap.width, (height - 48) / bitmap.height)
                sample = bitmap.resize((round(bitmap.width * scale), round(bitmap.height * scale)), Image.Resampling.NEAREST)
                sheet.paste(sample, (x + 6, y + 28), sample)
            draw.text((x + 6, y + height - 16), str(frame['duration_ms']) + ' ms / origin ' + str(frame['origin']), fill=(175, 196, 181))
    target.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(target)


def skill_pack(archives, output, old_sprites, specs=UTILITY_SKILLS, effects=UTILITY_EFFECTS):
    sprites = {}
    old_hashes = {}
    # Check the utility icon export. Other packs use a bounded visual
    # review of their chosen true icons, without rescanning old art for duplicates.
    if specs is UTILITY_SKILLS:
        for key, sprite in old_sprites.items():
            if key.startswith('skill.icon.') and key not in {'skill.icon.' + spec[0] for spec in UTILITY_SKILLS}:
                with Image.open(output / sprite['frames'][0]['file']) as bitmap:
                    old_hashes[hashlib.sha256(bitmap.convert('RGBA').tobytes()).hexdigest()] = key
    new_hashes = set()
    for key, skill_id, name in specs:
        path = f'{skill_id // 10000}.img/skill/{skill_id}/icon'
        frame = export_icon(archives['Skill'], output, f'expansion/icons/{key}/0.png', path, 'Skill', False)
        if specs is UTILITY_SKILLS and frame['size'] != [32, 32]:
            raise ValueError('Selected source icon is not 32px: ' + key)
        if specs is UTILITY_SKILLS and (frame['rgba_sha256'] in old_hashes or frame['rgba_sha256'] in new_hashes):
            raise ValueError('Selected icon duplicates another skill icon: ' + key)
        new_hashes.add(frame['rgba_sha256'])
        source_name, source_name_path = original_skill_name(archives['String'], path)
        sprites['skill.icon.' + key] = {'source': 'Skill.nx/' + path, 'frames': [frame],
                                      'name': name, 'source_skill_id': skill_id, 'source_name': source_name,
                                      'source_name_source': 'String.nx/' + source_name_path,
                                      'icon_kind': 'original_skill_icon', 'loop': False,
                                      'total_duration_ms': frame['duration_ms']}
    for key, skill_id, action, loop in effects:
        source = f'{skill_id // 10000}.img/skill/{skill_id}/{action}'
        sprites[key] = export_sequence(archives['Skill'], output, key, 'Skill', source, loop)
        sprites[key]['source_skill_id'] = skill_id
    if specs is CLASS_SKILLS:
        sprites['skill.spear_combo.thrust'] = export_sequence(
            archives['Character'], output, 'skill.spear_combo.thrust', 'Character',
            'Afterimage/spear.img/0/stabT1/2')
        sprites['skill.spear_combo.thrust'].update(
            default_facing='left', forward_angle_degrees=180,
            render_note='Original two-frame spear thrust afterimage, 30 ms then 200 ms with the original alpha fade. Place at the attacking player, mirror for facing.')
        for source_key, alias_key, first in [
                ('skill.spear_combo.cast', 'skill.spear_combo.release', 1),
                ('skill.earth_shatter.cast', 'skill.earth_shatter.impact', 1)]:
            original = sprites[source_key]
            selected = original['frames'][first:]
            sprites[alias_key] = {**original, 'frames': selected, 'alias_of': source_key,
                                 'source_start_ms': sum(frame['duration_ms'] for frame in original['frames'][:first]),
                                 'total_duration_ms': sum(frame['duration_ms'] for frame in selected),
                                 'render_note': 'Visible phase reuses exact original PNGs and timing after the game applies its own wind-up; original full sequence remains available.'}
    return sprites, {}


def enemy_pack(archives, output):
    sprites, visuals = {}, {}
    mobs = archives['Mob']
    for index, (key, mob_id, label, scale, radius, _old, theme, behavior) in enumerate(BOSSES + ENEMIES):
        source = f'{mob_id:07d}.img'
        actions = mobs.children(resolve(mobs, source))
        original_name, name_source = mob_name(archives['String'], mob_id)
        mapping = {'idle': 'stand' if 'stand' in actions else 'fly',
                   'move': 'move' if 'move' in actions else 'fly', 'hit': 'hit1', 'die': 'die1'}
        if 'attack1' in actions:
            mapping['attack1'] = 'attack1'
        elif 'skill1' in actions:
            mapping['attack1'] = 'skill1'
        if 'attack2' in actions:
            mapping['attack2'] = 'attack2'
        elif 'skill1' in actions and mapping.get('attack1') != 'skill1':
            mapping['attack2'] = 'skill1'
        for runtime_action, source_action in mapping.items():
            runtime_key = key + '.' + runtime_action
            sprite = export_sequence(mobs, output, runtime_key, 'Mob', source + '/' + source_action,
                                     runtime_action in ('idle', 'move'))
            sprite.update(name=label, source_name=original_name, source_name_source=name_source,
                          source_mob_id=mob_id, source_action=source_action, default_facing='left')
            sprites[runtime_key] = sprite
        info = properties(mobs, actions['info'])
        first = sprites[key + '.idle']['frames'][0]
        source_gap = max(0, first['origin'][1] - first['size'][1])
        ground_note = ('Original frame origin is the ground anchor; mirror it for facing. '
                       'Do not center individual animation frames or derive collision from transparent canvas bounds.')
        if source_gap:
            ground_note += f' First stand frame intentionally floats {source_gap} source pixels above this anchor.'
        if mob_id in (9300119, 9300121):
            ground_note += ' Decorative pixels extend 4 source pixels below the origin; retain the original anchor.'
        visuals[key] = {'name': label, 'source_mob_id': mob_id, 'source_name': original_name,
                        'source': 'Mob.nx/' + source, 'source_name_source': name_source,
                        'source_tagged_boss': bool(info.get('boss', 0)),
                        'proposed_visual_id': 6 + index if index < len(BOSSES) else 16 + index - len(BOSSES),
                        'suggested_render_scale': scale, 'source_origin_to_ground_px': [0, 0],
                        'suggested_footprint_radius': radius, 'suggested_region': theme,
                        'prototype_behavior_cue': behavior, 'actions': mapping,
                        'idle_source_origin': first['origin'], 'idle_source_size': first['size'],
                        'resource_status': 'exported_only', 'ground_note': ground_note,
                        'mechanics_note': 'Resource availability alone does not enable a formal boss/encounter. Combat AI, warning shapes, phases, rewards and chapter pools are authored separately.'}
    return sprites, visuals


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path, help='Directory containing original NX archives')
    parser.add_argument('--pack', choices=['utility', 'enemies', 'class_skills', 'fusion_skills', 'cross_fusions', 'phoenix'], required=True)
    parser.add_argument('--no-merge-manifest', action='store_true', help='Export only the independent pack while a candidate manifest is frozen')
    parser.add_argument('--output', type=Path, default=Path(__file__).resolve().parents[1] / 'assets')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    manifest_path = args.output / 'manifest.json'
    old_sprites = json.loads(manifest_path.read_text(encoding='utf-8'))['sprites']
    names = ['Mob', 'String'] if args.pack == 'enemies' else ['Skill', 'String']
    if args.pack == 'phoenix':
        names = ['Skill']
    if args.pack == 'class_skills':
        names += ['Character']
    paths = {name: args.source / (name + '.nx') for name in names}
    before = {name: fingerprint(path) for name, path in paths.items()}
    archives = {}
    try:
        archives = {name: NX(path) for name, path in paths.items()}
        if args.pack == 'enemies':
            sprites, visuals = enemy_pack(archives, args.output)
        elif args.pack == 'class_skills':
            sprites, visuals = skill_pack(archives, args.output, old_sprites, CLASS_SKILLS, EFFECTS_CLASS_SKILLS)
        elif args.pack == 'fusion_skills':
            sprites, visuals = skill_pack(archives, args.output, old_sprites, FUSION_SKILLS, [])
        elif args.pack == 'cross_fusions':
            sprites, visuals = skill_pack(archives, args.output, old_sprites, CROSS_FUSIONS, [])
        elif args.pack == 'phoenix':
            sprites = {'skill.phoenix.attack': export_sequence(archives['Skill'], args.output,
                        'skill.phoenix.attack', 'Skill', '312.img/skill/3121006/summon/attack1')}
            visuals = [{'key': 'skill.phoenix.attack', 'name': '凤凰原生攻击',
                        'resource_status': 'exported_only',
                        'render_note': 'Play only after the combat core launches the phoenix projectile; return to the existing skill.summon fly sequence afterward.'}]
        else:
            sprites, visuals = skill_pack(archives, args.output, old_sprites)
    finally:
        for archive in archives.values():
            archive.m.close()
            archive.f.close()
        after = {name: fingerprint(path) for name, path in paths.items()}
        if before != after:
            raise RuntimeError('Original archive size/mtime changed during export')
    files = {frame['file']: frame for sprite in sprites.values() for frame in sprite['frames']}
    validation = {'sprite_keys': len(sprites), 'pngs': len(files),
                  'png_bytes': sum((args.output / path).stat().st_size for path in files),
                  'rgba_bytes': sum(frame['size'][0] * frame['size'][1] * 4 for frame in files.values()),
                  'all_pngs_pixel_equal': True, 'original_archives_size_mtime_unchanged': True}
    if args.pack == 'utility':
        validation.update(skill_icons=3, distinct_new_skill_icons=3, effect_sequences=5)
    elif args.pack == 'class_skills':
        validation.update(skill_icons=len(CLASS_SKILLS), effect_sequences=len(EFFECTS_CLASS_SKILLS) + 3,
                          reused_phase_aliases=2)
    elif args.pack == 'fusion_skills':
        validation.update(skill_icons=len(FUSION_SKILLS), effect_sequences=0)
    elif args.pack == 'cross_fusions':
        validation.update(skill_icons=len(CROSS_FUSIONS), effect_sequences=0)
    elif args.pack == 'phoenix':
        validation.update(skill_icons=0, effect_sequences=1)
    else:
        validation.update(boss_appearances=6, enemy_appearances=14)
    notes = ['Original NX archives were opened read-only; original PNG pixels, sizes, origins, delays and alpha endpoints are retained.',
             'Only the named candidates are inspected and exported. No full archive bitmap scan.',
             'Contact sheets are inspection artifacts, not runtime UI assets.',
             'MapleStory source artwork belongs to its rights holders; this personal prototype does not establish redistribution rights.',
             'Resource export is not a claim that combat behavior, official pools or visual gameplay acceptance are complete.']
    if args.pack == 'enemies':
        notes += ['Targeted spider candidate Mob.nx/9400545.img is an ordinary mechanical spider without a source attack animation, so it is not presented as a completed giant-spider boss.',
                  'The accepted six extra bosses use snow, pirate, mechanical, dragon, desert-cactus and forest-demon themes instead of substituting mushroom variants.']
    metadata = {'schema_version': 1, 'resource_status': 'exported_only', 'sprites': sprites, 'visuals': visuals,
                'source_archives': {name + '.nx': details for name, details in before.items()},
                'validation': validation, 'notes': notes}
    if args.pack in ('fusion_skills', 'cross_fusions'):
        metadata['suggested_effect_reuse'] = {key.removeprefix('skill.icon.'): REUSE_SUGGESTIONS[key.removeprefix('skill.icon.')]
                                              for key in sprites}
        notes.append('Effect reuse is a resource suggestion only; original skill mechanisms, fusion timing and new behavior are not imported or implemented by these references.')
    stem = 'utility_skills' if args.pack == 'utility' else args.pack
    write_json(args.output / 'expansion' / (stem + '.json'), metadata)
    # Merge against the latest manifest so other completed resource keys survive.
    if not args.no_merge_manifest:
        manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
        manifest['sprites'].update(sprites)
        write_json(manifest_path, manifest)
    report = {key: value for key, value in metadata.items() if key != 'sprites'}
    report['sequences'] = [{'key': key, 'source': sprite['source'], 'frames': len(sprite['frames']),
                            'duration_ms': sprite['total_duration_ms']} for key, sprite in sprites.items()]
    report_name = {'utility': 'utility_skill', 'class_skills': 'class_skill', 'fusion_skills': 'fusion_skill', 'cross_fusions': 'cross_fusion', 'phoenix': 'phoenix', 'enemies': 'extended_enemy'}[args.pack]
    write_json(root / 'docs/assets' / (report_name + '_sources.json'), report)
    if args.pack != 'enemies':
        icons = {key: sprite for key, sprite in sprites.items() if key.startswith('skill.icon.')}
        if icons:
            contact_sheet(args.output, icons, root / 'artifacts/assets' / ('expansion_' + stem + '_icons_contact.png'))
        effects = {key: sprite for key, sprite in sprites.items() if not key.startswith('skill.icon.')}
        if args.pack == 'class_skills':
            actors = ('skill.holy_dragon.', 'skill.fire_beast.', 'skill.silver_eagle.', 'skill.decoy.')
            effects_contact(args.output, {key: sprite for key, sprite in effects.items() if key.startswith(actors)},
                            root / 'artifacts/assets/expansion_class_skills_summons_contact.png')
            effects = {key: sprite for key, sprite in effects.items() if not key.startswith(actors)}
        if effects:
            effects_contact(args.output, effects, root / 'artifacts/assets' / ('expansion_' + stem + '_effects_contact.png'))
    else:
        make_contact_sheet(args.output, sprites, visuals, BOSSES, root / 'artifacts/assets/expansion_bosses_contact.png', 3, (300, 270))
        make_contact_sheet(args.output, sprites, visuals, ENEMIES, root / 'artifacts/assets/expansion_enemies_contact.png', 4, (220, 160))
    print(json.dumps(validation, ensure_ascii=True))


if __name__ == '__main__':
    main()
