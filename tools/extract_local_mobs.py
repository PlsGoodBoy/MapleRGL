"""Read-only selective NX bitmap export. No game data files are modified."""
import struct,mmap,json,pathlib,argparse
from PIL import Image
class NX:
 def __init__(self,path):
  self.f=open(path,'rb');self.m=mmap.mmap(self.f.fileno(),0,access=mmap.ACCESS_READ)
  h=struct.unpack_from('<IIQIQIQIQ',self.m);assert h[0]==0x34474b50
  self.no,self.so,self.bo=h[2],h[4],h[6]
 def unpack(self,fmt,o):return struct.unpack_from(fmt,self.m,o)
 def node(self,i):return self.unpack('<IIHH8s',self.no+i*20)
 def name(self,i):
  o=self.unpack('<Q',self.so+i*8)[0];n=self.unpack('<H',o)[0];return self.m[o+2:o+2+n].decode('utf8')
 def children(self,i):
  n=self.node(i);return {self.name(self.node(k)[0]):k for k in range(n[1],n[1]+n[2])}
 def value(self,i):
  n=self.node(i);return struct.unpack('<ii' if n[3]==4 else '<q',n[4])
 def bitmap(self,i):
  bid,w,h=struct.unpack('<IHH',self.node(i)[4]);o=self.unpack('<Q',self.bo+bid*8)[0];n=self.unpack('<I',o)[0]
  src=self.m[o+4:o+4+n];out=bytearray();p=0
  while p<len(src):
   t=src[p];p+=1;l=t>>4
   if l==15:
    while True:
     b=src[p];p+=1;l+=b
     if b!=255:break
   out.extend(src[p:p+l]);p+=l
   if p==len(src):break
   d=src[p]|src[p+1]<<8;p+=2;l=(t&15)+4
   if (t&15)==15:
    while True:
     b=src[p];p+=1;l+=b
     if b!=255:break
   if not d or d>len(out):raise ValueError('bad LZ4 distance')
   for _ in range(l):out.append(out[-d])
  assert len(out)==w*h*4,(len(out),w,h)
  return Image.frombytes('RGBA',(w,h),bytes(out),'raw','BGRA')
def main():
 a=argparse.ArgumentParser();a.add_argument('source');a.add_argument('assets');args=a.parse_args();nx=NX(args.source);roots=nx.children(0);dest=pathlib.Path(args.assets);sprites={}
 mapping={'slime':'0210100.img','mushroom':'0120100.img','blue_mushroom':'2220100.img','pig':'1210100.img','elite':'2230102.img','boss':'6130101.img'}
 print('available',[(k,v,v in roots) for k,v in mapping.items()])
 for kind,mid in mapping.items():
  if mid not in roots:continue
  actions=nx.children(roots[mid])
  for action,source in [('move','move'),('stand','stand'),('hit','hit1'),('die','die1')]:
   if source not in actions:continue
   frames=[]
   for fn,fi in sorted(nx.children(actions[source]).items(),key=lambda kv:int(kv[0]) if kv[0].isdigit() else 10000):
    if not fn.isdigit() or nx.node(fi)[3]!=5:continue
    props=nx.children(fi);origin=list(nx.value(props['origin'])) if 'origin' in props else [0,0];delay=nx.value(props['delay'])[0] if 'delay' in props else 100
    rel=f'sprites/{kind}/{action}/{fn}.png';p=dest/rel;p.parent.mkdir(parents=True,exist_ok=True);nx.bitmap(fi).save(p)
    frames.append({'file':rel,'duration_ms':max(20,delay),'origin':origin})
   if frames:sprites[kind+'.'+action]={'frames':frames,'source':'local Mob.nx/'+mid+'/'+source}
 sprites.update({k.replace('.stand','.idle'):v for k,v in list(sprites.items()) if k.endswith('.stand')})
 dest.mkdir(parents=True,exist_ok=True);(dest/'local_mobs.json').write_text(json.dumps({'sprites':sprites},indent=2),encoding='utf8');print({k:len(v['frames']) for k,v in sprites.items()})
if __name__=='__main__':main()

