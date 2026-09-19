"""Normalize wire junctions and field orientation, emitting a reviewed patch."""
from pathlib import Path
import math, sys, uuid
from kicad_sexpr import parse, replace_spans, patch_file
sys.stdout.reconfigure(encoding='utf-8')
base=Path(__file__).resolve().parents[1]
p=base/'alimentacao.kicad_sch';text=p.read_text(encoding='utf-8');root=parse(text)
def xy(p):return ' '.join(f'{v:.5f}'.rstrip('0').rstrip('.') for v in p)
points=set();segments=[];reps=[]
for n in root.all('wire'):
    a,b=[tuple(map(float,v.values[1:3])) for v in n.one('pts').all('xy')]
    segments.append((a,b));points.update((a,b));reps.append((n.start,n.end,''))
for kind in ['junction','label','global_label','hierarchical_label','no_connect']:
    for n in root.all(kind):points.add(tuple(map(float,n.one('at').values[1:3])))
libs={s.values[1]:s for s in root.one('lib_symbols').all('symbol')}
for n in root.all('symbol'):
    x,y,angle=map(float,n.one('at').values[1:]);a=math.radians(angle)
    for unit in libs[n.get('lib_id')].all('symbol'):
        for pin in unit.all('pin'):
            px,py=map(float,pin.one('at').values[1:3]);points.add((round(x+px*math.cos(a)-py*math.sin(a),5),round(y-px*math.sin(a)-py*math.cos(a),5)))
    for field in n.all('property'):
        if field.values[1] in ['Reference','Value']:
            at=field.one('at');reps.append((at.start,at.end,f'(at {at.values[1]} {at.values[2]} {int(angle)})'))
unique=set()
for a,b in segments:
    split=sorted([v for v in points if (a[0]==b[0]==v[0] and min(a[1],b[1])<=v[1]<=max(a[1],b[1])) or (a[1]==b[1]==v[1] and min(a[0],b[0])<=v[0]<=max(a[0],b[0]))])
    unique.update(zip(split,split[1:]))
new=replace_spans(text,reps).rstrip()[:-1]
for a,b in sorted(unique):
    uid=uuid.uuid5(uuid.NAMESPACE_URL,f'tcc-power-normalized/{a}/{b}')
    new+=f'\n(wire (pts (xy {xy(a)}) (xy {xy(b)})) (stroke (width 0) (type default)) (uuid "{uid}"))'
new+='\n)\n'
print('*** Begin Patch');print(patch_file(p,text,new),end='');print('*** End Patch')
