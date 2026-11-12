"""Export named Adventure candidates from read-only NX; keep original pixels and origins."""
import json,sys,re
from pathlib import Path
sys.dont_write_bytecode=True
from extract_local_mobs import NX
from extract_regions import resolve
from extract_equipment_icons import export_icon
from extract_enemy_appearances import mob_name
from extract_expansion_assets import export_sequence

ROOT=Path(__file__).resolve().parents[1]
SOURCE=Path(sys.argv[1])
OUTPUT=ROOT/'assets'
ENEMIES=[
 ('evil_eye',2230100,1,'basic|rush'),('zombie_mushroom',2230101,1,'basic|split'),
 ('toy_imp',3230300,5,'support|ranged'),('fire_lion',3210200,0,'rush|charger'),
 ('zombie_lupin',4230101,1,'ranged|support'),('wild_kargo',6230100,2,'rush|charger'),
 ('dark_pepe',6230200,4,'basic|rush'),('clown',6230300,5,'ranged|ground'),
 ('possessed_doll',6230400,5,'support|split'),('adult_yeti',6300000,4,'heavy|ground'),
 ('gryphon',7130000,2,'rush|ranged'),('minotaur',7130100,2,'heavy|charger'),
 ('skeleton_commander',7130103,2,'support|heavy'),('grandfather_clock',8140200,5,'ground|support'),
 ('hellhound',8140500,2,'rush|ground'),('bonefish',8140600,3,'ranged|split'),
 ('shark',8150100,3,'rush|charger'),('lizardman',8150200,2,'heavy|ranged'),
 ('gatekeeper',8160000,5,'heavy|ground'),('armored_wraith',8170000,1,'support|charger'),
 ('time_eye',8200000,5,'ranged|ground'),('time_priest',8200001,5,'support|ranged'),
 ('time_sentinel',8200003,5,'heavy|charger'),('bubble_king',3230103,3,'ranged|support'),
 ('pufferfish',3230104,3,'split|basic'),('drum_rabbit',3230400,5,'basic|support'),
 ('baby_seal',3230405,3,'basic|rush'),('crab',4230104,3,'heavy|rush'),
 ('carnivorous_plant',4230105,0,'ground|split'),('alien',4230116,5,'ranged|charger')]
BOSSES=[
 ('GiantCrab','giant_crab',5220000,3,16,25),('DollRobot','doll_robot',6220000,5,20,18),
 ('Zeno','zeno',6220001,5,21,26),('TaeRoon','tae_roon',7220000,2,17,24),
 ('NineTail','nine_tail',7220001,1,27,19),('Eliza','eliza',8220000,0,22,26),
 ('Chimera','chimera',8220002,2,23,18),('Dodo','dodo',8220004,5,24,20),
 ('Lilynouch','lilynouch',8220005,4,17,23),('Lyka','lyka',8220006,4,25,21),
 ('Manon','manon',8180000,0,22,17),('Griffey','griffey',8180001,0,19,16)]
SKILLS=[4121007,4221007,4111002,4221001,4201005,4211006,4120005,4121004,4221006,14101006,
 5201001,5211004,5221004,5201002,5111002,5101002,5111006,5121001,5221007,5211001,
 21110003,21120005,22121000,22161002,35111004,35101012,24120002,24121003]

def main():
 manifest=json.loads((OUTPUT/'manifest.json').read_text(encoding='utf-8'))
 mobs=NX(SOURCE/'Mob.nx');names=NX(SOURCE/'String.nx');skills=NX(SOURCE/'Skill.nx');report=[];enemy_rows=[];boss_rows=[]
 for index,entry in enumerate(ENEMIES+BOSSES):
  boss=index>=len(ENEMIES)
  if boss:enum,key,mob_id,theme,first,second=entry;runtime='boss_'+key
  else:key,mob_id,theme,roles=entry;runtime='enemy.'+key
  source=f'{mob_id:07d}.img';actions=mobs.children(resolve(mobs,source));name=mob_name(names,mob_id)[0]
  mapping={'idle':'stand' if 'stand' in actions else 'fly','move':'move' if 'move' in actions else 'fly' if 'fly' in actions else 'stand','hit':'hit1','die':'die1'}
  for a in ['attack1','attack2','skill1']:
   if a in actions:mapping[a]=a
  for action,node in mapping.items():
   sprite=export_sequence(mobs,OUTPUT,runtime+'.'+action,'Mob',source+'/'+node,action in ['idle','move'])
   sprite.update(source_mob_id=mob_id,source_name=name,name=name,default_facing='left');manifest['sprites'][runtime+'.'+action]=sprite
  frame=manifest['sprites'][runtime+'.idle']['frames'][0];scale=min(1.4,(100 if boss else 57)/max(frame['size']));death=manifest['sprites'][runtime+'.die']['total_duration_ms']/1000
  report.append({'key':runtime,'source':'Mob.nx/'+source,'name':name,'scale':scale,'frames':sum(len(manifest['sprites'][runtime+'.'+a]['frames']) for a in mapping)})
  if boss:boss_rows.append(f'  {{BossProfile::{enum},"{key}","{runtime}","{name}","patterns_{first}_{second}",{theme},2,15,7,7,100,12,1,6,1,true,{scale:.4f}f,{death:.3f}f,40,30,1.03f}},')
  else:enemy_rows.append(f'  {{{30+index},"{runtime}","{name}",{theme},{roles},{scale:.4f}f,22,{death:.3f}f}},')
 catalogue=(ROOT/'src/adventure_catalogue.inc').read_text(encoding='utf-8');rows=re.findall(r'X\((\d+),\w+,"([^"]+)","([^"]+)".*?,"([^"]+)"\)',catalogue)
 for i,(number,key,name,fallback) in enumerate(rows):
  if i<28:
   skill_id=SKILLS[i];source=f'{skill_id//10000}.img/skill/{skill_id}/icon'
   try:
    resolve(skills,source);frame=export_icon(skills,OUTPUT,f'adventure/icons/{key}/0.png',source,'Skill',False)
    manifest['sprites']['skill.icon.'+key]={'source':'Skill.nx/'+source,'frames':[frame],'loop':False,'total_duration_ms':100,'name':name}
    report.append({'key':'skill.icon.'+key,'source':'Skill.nx/'+source});continue
   except (KeyError,ValueError):pass
  # A named alias records the actual reused artwork; it is never called an extracted new skill.
  alias='skill.icon.'+fallback;manifest['sprites']['skill.icon.'+key]={**manifest['sprites'][alias],'alias_of':alias,'name':name}
  report.append({'key':'skill.icon.'+key,'alias_of':alias})
 (OUTPUT/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
 (ROOT/'docs/assets/adventure_sources.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
 (ROOT/'src/adventure_enemy_rows.inc').write_text('\n'.join(enemy_rows)+'\n',encoding='utf-8')
 (ROOT/'src/adventure_boss_rows.inc').write_text('\n'.join(boss_rows)+'\n',encoding='utf-8')
 print(f'Exported {len(ENEMIES)} enemy skins, {len(BOSSES)} bosses; {len(rows)} skill icon mappings with explicit provenance.')
if __name__=='__main__':main()
