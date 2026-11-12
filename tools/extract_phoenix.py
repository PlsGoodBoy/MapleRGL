"""Export Phoenix 3121006 and Dragon Roar 1311006; NX stays read-only."""
from pathlib import Path
import argparse,json
from extract_local_mobs import NX

def main():
 p=argparse.ArgumentParser();p.add_argument('skill_nx',type=Path);p.add_argument('--output',type=Path,default=Path(__file__).resolve().parents[1]/'assets');a=p.parse_args();n=NX(a.skill_nx);result={'sprites':{}}
 for key,folder,source,name in [('skill.summon','phoenix','312.img/skill/3121006/summon/fly','火凤凰'),('skill.nova','dragon_roar','131.img/skill/1311006/effect','龙咆哮')]:
  i=0
  for k in source.split('/'):i=n.children(i)[k]
  frames=[]
  for fn,j in sorted(n.children(i).items(),key=lambda kv:int(kv[0])):
   props=n.children(j);im=n.bitmap(j);rel='sprites/skill/'+folder+'/'+fn+'.png';target=a.output/rel;target.parent.mkdir(parents=True,exist_ok=True);im.save(target)
   f={'file':rel,'duration_ms':n.value(props['delay'])[0] if 'delay' in props else 100,'origin':list(n.value(props['origin'])) if 'origin' in props else [0,0],'source':'Skill.nx/'+source+'/'+fn}
   for prop in ['a0','a1']:
    if prop in props:f['alpha_start' if prop=='a0' else 'alpha_end']=n.value(props[prop])[0]
   frames.append(f)
  result['sprites'][key]={'frames':frames,'source_skill_id':int(source.split('/')[2]),'name':name}
 result['sprites']['skill.summon']['default_facing']='left'
 (a.output/'phoenix_assets.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8');print({k:len(v['frames']) for k,v in result['sprites'].items()})
if __name__=='__main__':main()
