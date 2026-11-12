"""Selective original NX content export; never modifies source archives."""
from pathlib import Path
import argparse,json
from extract_local_mobs import NX

def resolve(n,path):
 i=0
 for k in path.split('/'):i=n.children(i)[k]
 return i

def main():
 a=argparse.ArgumentParser();a.add_argument('source',type=Path);a.add_argument('--output',type=Path,default=Path(__file__).resolve().parents[1]/'assets');args=a.parse_args()
 archives={k:NX(args.source/(k+'.nx')) for k in ['Character','Item']}
 specs=[('skill.orbit','Character','Weapon/01302000.img/stand1/0/weapon'),('item.heal','Item','Consume/0200.img/02000000/info/icon'),('item.magnet','Character','PetEquip/01812000.img/info/icon'),('item.fury','Item','Consume/0200.img/02002004/info/icon'),('event.chest','Item','Etc/0428.img/04280000/info/icon')]
 result={'sprites':{},'notes':['Original MapleStory images from local NX archives, read-only extraction.','Render origin normalized to image center for free-standing pickups/rotating sword. Original origin retained as source_origin.','Magnet icon is original pet magnet equipment 1812000, stored in Character.nx.']}
 for key,archive,path in specs:
  n=archives[archive];i=resolve(n,path);props=n.children(i);im=n.bitmap(i);origin=list(n.value(props['origin'])) if 'origin' in props else [0,0]
  rel='sprites/content/'+key.replace('.','/')+'/0.png';p=args.output/rel;p.parent.mkdir(parents=True,exist_ok=True);im.save(p)
  result['sprites'][key]={'frames':[{'file':rel,'duration_ms':100,'origin':[im.width/2,im.height/2],'source_origin':origin,'source':archive+'.nx/'+path,'size':[im.width,im.height]}]}
 result['sprites']['skill.orbit']['forward_angle_degrees']=165
 result['sprites']['skill.orbit']['direction_note']='Tip points left and slightly down; approximate visual angle 165 degrees (0 right, 90 down).'
 (args.output/'content_assets.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
 print({k:v['frames'][0]['size'] for k,v in result['sprites'].items()})
if __name__=='__main__':main()
