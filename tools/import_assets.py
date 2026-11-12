#!/usr/bin/env python3
"""Import a small real MapleStory asset subset from a local cosmetic-playground checkout.
Usage: python tools/import_assets.py /path/to/maplestory-cosmetic-playground
Requires Pillow. Source artwork belongs to Nexon, not covered by this project's code license.
"""
import argparse, gzip, json, shutil, subprocess
from pathlib import Path
from PIL import Image
p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('--output',type=Path,default=Path(__file__).resolve().parents[1]/'assets');a=p.parse_args();src=a.source;out=a.output;out.mkdir(parents=True,exist_ok=True)
manifest={'version':1,'sprites':{},'sources':{'repository':'https://github.com/Javipen/maplestory-cosmetic-playground','commit':subprocess.check_output(['git','-C',str(src),'rev-parse','HEAD'],text=True).strip()},'notes':['Nexon artwork: private prototype use; no redistribution license asserted.','Player frames are assembled from original body and equipment layers using source anchors.']}
def read(path):
 path=src/path
 return json.load(gzip.open(path) if path.suffix=='.gz' else open(path))
def save(key,seq):
 frames=[]
 for n,f in enumerate(seq):
  target=out/'sprites'/key.replace('.','/')/(str(n)+'.png');target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src/f['url'],target)
  frames.append({'file':target.relative_to(out).as_posix(),'duration_ms':f.get('delay',100),'origin':[f.get('origin',{}).get('x',0),f.get('origin',{}).get('y',0)],'alpha_start':f.get('a0',255),'alpha_end':f.get('a1',255),'source':f.get('source',{})})
 manifest['sprites'][key]={'frames':frames}
# Original skill sequences, not icons.
for key,id,seq in [('skill.bolt',2001002,'ball'),('skill.slash',1001002,'effect'),('skill.nova',2311003,'effect'),('skill.pierce',3201004,'ball'),('skill.lightning',2201004,'hit/0'),('skill.poison',2111002,'tile/0'),('skill.summon',2311005,'summon/fly'),('skill.hit',2001002,'hit/0'),('skill.dash',4211005,'effect')]:
 d=read(Path('data/skills')/(str(id)+'.json'));save(key,d['sequences'][seq])
practice=read(Path('maps/practice.json'));save('terrain.ground',[practice['ground']]);save('terrain.soil',[practice['soil']]);save('terrain.background',practice['background'][:1])
hen=read(Path('maps/henesys.json'));decos=[d for d in hen['drawables'] if d['kind']=='obj' and 'nature' in d['sequence'][0].get('source',{}).get('property','')]
for n,d in enumerate(decos[:8]):save('decor.'+str(n),d['sequence'])
ui=read(Path('maps/ui.json'))
for name,frame in ui.items():
 if isinstance(frame,dict) and frame.get('url'):save('ui.'+name,[frame])
base=read(Path('data/base.json.gz'));ids=['20000','30000','1047007','1067005','1077000'];catalog={x['item_id']:x for x in read(Path('data/catalog.json'))['items']};docs={id:read(Path('data/layers')/(id+'.json.gz')) for id in ids}
# Anchor solver follows the source's documented neck/navel/brow attachment convention.
for state,action in [('idle','stand1'),('move','walk1'),('hit','alert'),('die','dead')]:
 frames=[]
 for n,spec in enumerate(base['specs'][action]):
  pose=f"{spec['action']}/{spec['frame']}";body=base['poses']['0'][pose];layers=[dict(l) for l in body['body']]+[dict(l) for l in body['head'] if l['name'] in ['head','humanEar']]
  for id,d in docs.items():
   cat=catalog[id]['category']
   if cat=='Face':ls=d['expressions']['default']['frames']['0'] if body.get('face') else []
   elif cat=='Hair':ls=d.get('poses',{}).get(pose,d.get('front' if body.get('face') else 'back',[]));ls=[l for l in ls if l['z'] not in ['hair','backHairBelowCap','backHairBelowCapWide','backHairBelowCapNarrow']]
   else:ls=d.get('poses',{}).get(pose,[])
   layers.extend(dict(l) for l in ls)
  anchors={};placed=[];pending=[]
  for l in layers:
   if l.get('name')=='body' and not placed:l['pos']=[0,0];placed.append(l);anchors.update({k:[v['x'],v['y']] for k,v in l['maps'].items()})
   else:pending.append(l)
  for _ in range(10):
   remaining=[]
   for l in pending:
    shared=next((k for k in l['maps'] if k in anchors),None)
    if shared is None:remaining.append(l);continue
    x,y=anchors[shared][0]-l['maps'][shared]['x'],anchors[shared][1]-l['maps'][shared]['y'];l['pos']=[x,y];placed.append(l)
    for k,v in l['maps'].items():anchors.setdefault(k,[x+v['x'],y+v['y']])
   if len(remaining)==len(pending):break
   pending=remaining
  if 'handMove' not in anchors and 'hand' in anchors:
   anchors['handMove']=anchors['hand']
   for l in pending[:]:
    k=next((k for k in l['maps'] if k in anchors),None)
    if k:
     l['pos']=[anchors[k][0]-l['maps'][k]['x'],anchors[k][1]-l['maps'][k]['y']];placed.append(l);pending.remove(l)
  for l in placed:l['xy']=[l['pos'][0]-l['origin']['x'],l['pos'][1]-l['origin']['y']]
  x0=min(l['xy'][0] for l in placed);y0=min(l['xy'][1] for l in placed);x1=max(l['xy'][0]+l['w'] for l in placed);y1=max(l['xy'][1]+l['h'] for l in placed)
  image=Image.new('RGBA',(x1-x0,y1-y0));order=base['orders']['classic']
  for l in sorted(placed,key=lambda l:order.get(l['z'],80),reverse=True):image.alpha_composite(Image.open(src/l['url']).convert('RGBA'),(l['xy'][0]-x0,l['xy'][1]-y0))
  target=out/'sprites/player'/state/(str(n)+'.png');target.parent.mkdir(parents=True,exist_ok=True);image.save(target);frames.append({'file':target.relative_to(out).as_posix(),'duration_ms':spec['delay'],'origin':[-x0,-y0],'source_pose':pose,'unresolved_layers':len(pending)})
 manifest['sprites']['player.'+state]={'frames':frames}
for alias,key in {'skill.projectile':'skill.bolt','skill.zone':'skill.poison','map.decor':'decor.0'}.items():manifest['sprites'][alias]=dict(manifest['sprites'][key],alias_of=key)
(out/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2))
shutil.copy2(src/'ASSET-NOTICE.md',out/'UPSTREAM-ASSET-NOTICE.md')
print('Imported',len(manifest['sprites']),'sequences;',sum(len(s['frames']) for s in manifest['sprites'].values()),'frames')
