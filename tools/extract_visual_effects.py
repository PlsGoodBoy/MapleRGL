"""Export named original skill sequences, without modifying the NX archives."""
import json, sys
from pathlib import Path
from extract_expansion_assets import NX, export_sequence

ROOT = Path(__file__).resolve().parents[1]
SOURCE = Path(sys.argv[1])
SEQUENCES = [
 ('shadow.cast',4121007,'effect'),('shadow.hit',4121007,'hit/0'),
 ('blade.cast',4221007,'effect'),('blade.hit',4201005,'hit/0'),
 ('clone.cast',4111002,'effect0'),('assassin.cast',4221001,'effect'),
 ('bat.cast',14101006,'effect'),('gun.cast',5211004,'effect'),
 ('gun.hit',5201001,'hit/0'),('grenade.travel',5201002,'ball'),
 ('explosion.hit',5201002,'hit/0'),('energy.cast',5111002,'effect'),
 ('fist.cast',5101002,'effect'),('fist.hit',5101002,'hit/0'),
 ('turret.idle',5211001,'summon/stand'),('turret.attack',5211001,'summon/attack1'),
 ('turret.cast',5211001,'summon/summoned'),('turret.end',5211001,'summon/die'),
 ('ice.cast',21120005,'effect'),('ice.hit',21120005,'hit/0'),
]

def main():
 nx=NX(SOURCE);path=ROOT/'assets/manifest.json';manifest=json.loads(path.read_text(encoding='utf8'));report=[]
 for name,sid,node in SEQUENCES:
  key='vfx.skills.'+name;source=f'{sid//10000}.img/skill/{sid}/{node}'
  seq=export_sequence(nx,ROOT/'assets',key,'Skill',source,name.endswith('.idle'),skip_zero_delay=True)
  seq['timing_note']='Original positive-duration frames; zero-time terminal frames omitted.'
  manifest['sprites'][key]=seq
  report.append({'key':key,'source':seq['source'],'frames':len(seq['frames']),'duration_ms':seq['total_duration_ms']})
 path.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
 (ROOT/'docs/assets/visual_effect_sources.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
 print(f'Exported {len(report)} original sequences / {sum(r["frames"] for r in report)} frames')

if __name__=='__main__':main()
