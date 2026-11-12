"""Selective, read-only NX export for battle actors, projectiles and fields.

Uses the existing manifest/animation format. Source pixels are never repainted.
Run with the directory containing Skill.nx, Mob.nx, Effect.nx and Map.nx.
"""
import argparse
import json
from pathlib import Path
from extract_local_mobs import NX
from extract_regions import resolve, properties
from extract_expansion_assets import export_sequence


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archives', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    output = root / 'assets'
    manifest = json.loads((output / 'manifest.json').read_text(encoding='utf8'))
    report_path = root / 'docs/assets/battle_art_sources.json'
    previous_report = json.loads(report_path.read_text(encoding='utf8')) if report_path.exists() else {}
    previous_reviews = {(r['key'], r['source']): r['runtime_review'] for r in previous_report.get('sequences', [])}
    sprites = manifest['sprites']
    nx = {name: NX(args.archives / (name + '.nx')) for name in ['Skill', 'Mob', 'Map', 'Effect', 'UI']}
    records = []

    def exists(archive, path):
        try:
            resolve(nx[archive], path)
            return True
        except KeyError:
            return False

    def sequence(key, archive, path, usage, loop=False, correspondence='exact', **metadata):
        if nx[archive].node(resolve(nx[archive], path))[3] == 5:
            node=resolve(nx[archive],path);props=properties(nx[archive],node);bitmap=nx[archive].bitmap(node)
            relative='expansion/sequences/'+key.replace('.', '/')+'/0.png';target=output/relative
            target.parent.mkdir(parents=True,exist_ok=True);bitmap.save(target)
            frame={'file':relative,'duration_ms':100,'origin':props.get('origin',[0,0]),'size':list(bitmap.size),'source':archive+'.nx/'+path,'source_properties':props,'alpha_nonzero_bounds':list(bitmap.getchannel('A').getbbox())}
            value = {'source':archive+'.nx/'+path,'frames':[frame],'loop':loop,'total_duration_ms':100}
        else:
            value = export_sequence(nx[archive], output, key, archive, path, loop, skip_zero_delay=True)
        value.update(default_facing='left', forward_angle_degrees=180,
                     usage=usage, correspondence=correspondence, **metadata)
        # Keep ground anchors for actors; projectiles/effect patches use the
        # visible body center, independent of the transparent canvas margins.
        for frame in value['frames']:
            bounds = frame['alpha_nonzero_bounds']
            frame['visual_center'] = [(bounds[0]+bounds[2])/2, (bounds[1]+bounds[3])/2] if bounds else frame['origin']
        sprites[key] = value
        records.append({'key': key, 'source': value['source'], 'usage': usage,
                        'correspondence': correspondence, 'frames': len(value['frames']),
                        'duration_ms': value['total_duration_ms'], 'default_facing': 'left',
                        'anchors_and_timing': 'assets/manifest.json sprites/' + key,
                        'runtime_review': previous_reviews.get((key, value['source']), 'pending'), **metadata})
        return key

    octopus = '521.img/skill/5211001'
    for action, node in [('idle','stand'), ('attack','attack1'), ('spawn','summoned'), ('end','die')]:
        sequence('battle.octopus.'+action, 'Skill', octopus+'/summon/'+node,
                 'OctopusTurret / DeepFleet original gunslinger summon body', action=='idle',
                 source_skill_id=5211001, ground_anchor=[0,0], render_scale=.9,
                 muzzle_native=[-23,-18], attack_release_ms=480)
    sequence('battle.octopus.ball','Skill',octopus+'/summon/attack1/info/ball','OctopusTurret / DeepFleet flying ink projectile',True)
    sequence('battle.octopus.hit','Skill',octopus+'/hit/0','OctopusTurret / DeepFleet impact')
    for action,node in [('idle','stand'),('attack','attack1'),('move','fly'),('spawn','summoned'),('end','die')]:
        sequence('battle.phoenix.'+action,'Skill','312.img/skill/3121006/summon/'+node,'Phoenix actor '+action,action in ('idle','move'))
    for action,node in [('idle','stand'),('move','move'),('attack','attack1'),('die','die'),('spawn','summoned')]:
        sequence('skill.fire_beast.'+action,'Skill','212.img/skill/2121005/summon/'+node,'FireBeast original Ifrit body',action in ('idle','move'),source_skill_id=2121005)
    sequence('skill.fire_beast.hit','Skill','212.img/skill/2121005/hit/0','FireBeast Ifrit impact')
    for action,node in [('idle','stand'),('move','move'),('attack','attack1'),('end','die1')]:
        if exists('Mob','5110302.img/'+node):
            sequence('battle.engineer.'+action,'Mob','5110302.img/'+node,'Engineer / mechanical fusion ally body',action in ('idle','move'), 'thematic substitute: Neo Huroid; Mechanic skill absent from this Skill.nx')
    for action,node in [('idle','stand'),('move','move'),('attack','attack1'),('end','die1')]:
        if exists('Mob','9300004.img/'+node):
            sequence('battle.launcher.'+action,'Mob','9300004.img/'+node,'Missile launcher / mechanical fusion ally body',action in ('idle','move'),'thematic substitute: original mechanical monster, not Mechanic summon')
    for action in ['fly','move','stand']:
        if exists('Mob','2300100.img/'+action):
            sequence('battle.bat.fly','Mob','2300100.img/'+action,'ShadowBats / GhostWings flying body',True,'thematic substitute: original Stirge, not a skill icon')
            break
    for key,path,usage,loop in [
        ('rocket','5110302.img/attack1/info/ball','Missiles / Engineer / mechanical fusion projectile',True),
        ('snow','8220001.img/attack1/info/ball','Snowman snowball',True),
        ('curse','5220002.img/attack1/info/ball','Faust original banana projectile',True),
        ('fire','8140500.img/attack1/info/ball','Hellhound fire projectile',True),
        ('seed','3220000.img/attack2/info/ball','Forest / spore thematic projectile',True),
        ('bubble','3230103.img/attack1/info/ball','Water bubble projectile',True),
        ('arcane','4230106.img/attack1/info/ball','Moon Pixie magic projectile',True),
    ]:
        sequence('battle.shot.'+key,'Mob',path,usage,loop)
        hit=path.rsplit('/',1)[0]+'/hit'
        if exists('Mob',hit): sequence('battle.shot.'+key+'.hit','Mob',hit,usage+' impact')
    sequence('battle.shot.cannon','Skill','522.img/skill/5221007/ball','Gun / cannon ammunition',True)
    sequence('battle.shot.gunhit','Skill','522.img/skill/5221007/hit/0','Gun / cannon impact')
    for number in range(3):
        sequence('battle.field.smoke'+str(number),'Skill',f'422.img/skill/4221006/tile/{number}', 'Smoke field: original Smokescreen cloud, gray-blue runtime tint', True)
    sequence('battle.field.clock','Skill','512.img/skill/5121010/effect','Distortion clock texture: Time Leap original art',False,'thematic texture: custom field uses a held source clock frame plus transparent hands')
    sequence('battle.field.seal','Skill','221.img/skill/2211004/effect','Regression inward seal glyph',False,'thematic substitute: Seal cast glyph')
    sequence('battle.field.nature','Skill','231.img/skill/2311003/effect','Natural Order equipment suppression ground sigil',False,'thematic substitute: Holy Symbol sigil with foliage, no poison damage')
    sequence('battle.field.fire','Skill','312.img/skill/3121006/tile/0','Ground flames',True)
    sequence('battle.shot.card','UI','UIWindow.img/Minigame/MemoryGame/card/back0','Phantom card projectile',True,'thematic substitute: original Memory Game card back; not Phantom skill artwork or a skill icon')

    # Inspect every currently shipped monster's actual attack metadata before
    # selecting a same-theme donor. No Chinese display-name guessing is needed.
    enemies={}
    for key,value in list(sprites.items()):
        if not key.endswith('.idle') or not (key.startswith('enemy.') or key.startswith('boss')): continue
        src=value.get('source','')
        if 'Mob.nx/' not in src: continue
        img=src.split('Mob.nx/')[1].split('/')[0]
        if not exists('Mob',img): continue
        runtime=key[:-5]; actions=nx['Mob'].children(resolve(nx['Mob'],img)); own_ball=None; own_hit=None; attack='attack1'; inspected=[]
        for name in actions:
            if not name.startswith('attack'): continue
            base=img+'/'+name+'/info'; inspected.append(base)
            if exists('Mob',base+'/ball') and own_ball is None:own_ball=base+'/ball';attack=name
            if exists('Mob',base+'/hit') and own_hit is None:own_hit=base+'/hit'
        theme='arcane'
        if any(t in runtime for t in ['snow','yeti','pepe','lilynouch']):theme='snow'
        elif any(t in runtime for t in ['pirate','robot','zeno','alien','clock','cannon']):theme='rocket'
        elif any(t in runtime for t in ['fire','hell','balrog','lion','manon','chimera']):theme='fire'
        elif any(t in runtime for t in ['water','fish','crab','shark','bubble','slime','dodo']):theme='bubble'
        elif any(t in runtime for t in ['lupin','faust']):theme='curse'
        elif any(t in runtime for t in ['mushroom','stump','cactus','snail','plant','eliza']):theme='seed'
        ball='battle.shot.'+theme; hit='battle.shot.'+theme+'.hit'
        if own_ball:
            ball=sequence('battle.hostile.'+runtime+'.ball','Mob',own_ball,runtime+' flying projectile',True)
            candidate=own_ball.rsplit('/',1)[0]+'/hit'
            if exists('Mob',candidate):own_hit=candidate
        if own_hit:hit=sequence('battle.hostile.'+runtime+'.hit','Mob',own_hit,runtime+' impact')
        if hit not in sprites:hit='vfx.skills.gun.hit'
        if attack in actions and runtime+'.'+attack not in sprites:
            sequence(runtime+'.'+attack,'Mob',img+'/'+attack,runtime+' attack body')
        enemies[runtime]={'body_action':attack,'projectile':ball,'impact':hit,
                          'correspondence':'exact' if own_ball else 'thematic substitute',
                          'searched':inspected,'source_mob':img,
                          'note':'' if own_ball else 'No ball node in source attacks; keeps existing game attack behavior with a named original same-theme projectile.'}
    manifest['battle_art']={'enemies':enemies,'notes':[
        'No original NX/WZ archives are modified or copied into this repository.',
        'Modern Mechanic/Phantom skill IDs are absent in the local Skill.nx; local server XML has no corresponding canvas. Installed split WZ client has no reusable stand-alone selective decoder in this workspace.',
        'Original animation source origins, alpha bounds and frame durations remain in each sprite. Visual centers are for projectile collision-body alignment only.',
        'Transient visuals follow combat time, not encounter progress. Low effects keeps bodies and fields.'
    ]}
    (output/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    report_path.write_text(json.dumps({'sequences':records,'enemy_attacks':enemies,'notes':manifest['battle_art']['notes'],
                                      'runtime_scenes':previous_report.get('runtime_scenes',[])},ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(f'Exported {len(records)} sequences / {sum(r["frames"] for r in records)} frames; mapped {len(enemies)} enemy looks.')


if __name__=='__main__':main()

