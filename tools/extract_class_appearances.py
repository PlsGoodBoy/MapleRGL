"""Compose selected original NX character layers. Input archives remain read-only."""
from pathlib import Path
import argparse,json
from PIL import Image
from extract_local_mobs import NX

def resolve(n,path):
 i=0
 for k in path.split('/'):
  i=n.children(i).get(k)
  if i is None:return None
 return i

def text(n,i):return n.name(int.from_bytes(n.node(i)[4][:4],'little'))
def layers(n,i):
 if i is None:return []
 result=[]
 for name,j in n.children(i).items():
  if n.node(j)[3]==5:
   p=n.children(j);maps={k:list(n.value(v)) for k,v in n.children(p['map']).items()} if 'map' in p else {}
   result.append({'node':j,'name':name,'maps':maps,'origin':list(n.value(p['origin'])) if 'origin' in p else [0,0],'z':text(n,p['z']) if 'z' in p and n.node(p['z'])[3]==3 else 'body'})
  elif name=='hairShade':result.extend(layers(n,j))
 return result

def main():
 a=argparse.ArgumentParser();a.add_argument('nx_directory',type=Path);a.add_argument('--output',type=Path,default=Path(__file__).resolve().parents[1]/'assets');args=a.parse_args();out=args.output
 n=NX(args.nx_directory/'Character.nx');base=NX(args.nx_directory/'Base.nx');order={k:base.value(v)[0] for k,v in base.children(resolve(base,'zmap.img')).items()}
 sets={'archer':['Hair/00030000.img','Cap/01002008.img','Coat/01040002.img','Pants/01060002.img','Shoes/01072001.img','Weapon/01452000.img'], 'mage':['Hair/00030020.img','Cap/01002016.img','Longcoat/01050001.img','Shoes/01072001.img','Weapon/01372005.img'], 'warrior':['Hair/00030030.img','Cap/01002002.img','Longcoat/01050000.img','Shoes/01072001.img','Weapon/01302000.img']}
 manifest={'sprites':{},'class_equipment':sets,'notes':['Read-only local Character.nx + Base.nx zmap. No recoloring.','Death uses original dead body and front head with class hair/cap. Original dead has no navel/hand anchors, so armor and weapon are intentionally absent in death.','Hit uses original alert pose as reaction substitute.']}
 for cls,equipment in sets.items():
  for state,action in [('idle','stand1'),('move','walk1'),('hit','alert'),('die','dead')]:
   result=[];bi=resolve(n,'00002000.img/'+action)
   for frame,idx in sorted(n.children(bi).items()):
    if not frame.isdigit():continue
    props=n.children(idx);seq=layers(n,idx);front=n.value(props['face'])[0] if 'face' in props else 1
    hi=resolve(n,'00012000.img/'+action+'/'+frame)
    if hi is None:hi=resolve(n,'00012000.img/'+('front' if front else 'back'))
    seq+=layers(n,hi)
    if front:seq+=layers(n,resolve(n,'Face/00020000.img/default'))
    for eq in equipment:
     if state=='die' and not eq.startswith(('Hair/','Cap/')):continue
     pose=action+'/'+frame if state!='die' else 'stand1/0';ei=resolve(n,eq+'/'+pose)
     if ei is None:ei=resolve(n,eq+'/stand1/0')
     assert ei is not None,(eq,pose)
     ls=layers(n,ei)
     if eq.startswith('Hair/'):
      ls=[l for l in ls if l['name'] not in ['hairOverHead','backHairBelowCapWide','backHairBelowCapNarrow']]
      # Only skin tone zero hair shade, not every skin variant.
      ls=[l for l in ls if not l['name'].isdigit() or l['name']=='0']
     seq+=ls
    body=next(l for l in seq if l['name']=='body');body['pos']=[0,0];anchors=dict(body['maps']);placed=[body];pending=[l for l in seq if l is not body]
    for _ in range(10):
     remaining=[]
     for l in pending:
      key=next((k for k in l['maps'] if k in anchors),None)
      if key is None:remaining.append(l);continue
      pos=[anchors[key][a]-l['maps'][key][a] for a in [0,1]];l['pos']=pos;placed.append(l)
      for k,v in l['maps'].items():anchors.setdefault(k,[pos[0]+v[0],pos[1]+v[1]])
     if len(remaining)==len(pending):
      if 'handMove' not in anchors and 'hand' in anchors:
       anchors['handMove']=anchors['hand'];pending=remaining;continue
      break
     pending=remaining
    assert not pending,[(cls,state,frame,l['name'],l['maps']) for l in pending]
    for l in placed:
     l['image']=n.bitmap(l['node']);l['xy']=[l['pos'][a]-l['origin'][a] for a in [0,1]]
    x0=min(l['xy'][0] for l in placed);y0=min(l['xy'][1] for l in placed);x1=max(l['xy'][0]+l['image'].width for l in placed);y1=max(l['xy'][1]+l['image'].height for l in placed)
    im=Image.new('RGBA',(x1-x0,y1-y0))
    for l in sorted(placed,key=lambda l:order.get(l['z'],80),reverse=True):im.alpha_composite(l['image'],(l['xy'][0]-x0,l['xy'][1]-y0))
    rel=f'sprites/player_{cls}/{state}/{frame}.png';p=out/rel;p.parent.mkdir(parents=True,exist_ok=True);im.save(p)
    result.append({'file':rel,'duration_ms':n.value(props['delay'])[0] if 'delay' in props else 150,'origin':[-x0,-y0],'source_pose':action+'/'+frame,'unresolved_layers':0,'layer_count':len(placed)})
   manifest['sprites']['player_'+cls+'.'+state]={'frames':result}
 s=NX(args.nx_directory/'Skill.nx');ball=resolve(s,'300.img/skill/3001004/ball');frames=[]
 for name,i in sorted(s.children(ball).items()):
  if not name.isdigit() or s.node(i)[3]!=5:continue
  p=s.children(i);rel='sprites/skill/arrow/'+name+'.png';target=out/rel;target.parent.mkdir(parents=True,exist_ok=True);s.bitmap(i).save(target)
  frames.append({'file':rel,'duration_ms':s.value(p['delay'])[0] if 'delay' in p else 100,'origin':list(s.value(p['origin'])) if 'origin' in p else [0,0],'source':'Skill.nx/300.img/skill/3001004/ball/'+name})
 manifest['sprites']['skill.arrow']={'frames':frames,'forward_angle_degrees':180}
 (out/'next_classes.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf8')
 print({k:len(v['frames']) for k,v in manifest['sprites'].items()})
if __name__=='__main__':main()
