"""Focused visual cleanup of the power sheet and imported footprints."""
from pathlib import Path
import sys
from kicad_sexpr import parse, replace_spans, patch_file
sys.stdout.reconfigure(encoding='utf-8')
base=Path(__file__).resolve().parents[1]; changes={}
p=base/'alimentacao.kicad_sch';old=p.read_text(encoding='utf-8');r=parse(old);reps=[]
places={'J61':(30.48,81.28,0),'JP60':(85.09,44.45,90),'F60':(111.76,57.15,90),'Q60':(182.88,50.8,90),'D60':(150.0,90.17,90),'D61':(231.0,87.63,270),'L60':(124.46,160.02,90),'C64':(103.0,201.93,90),'L61':(284.48,160.02,90)}
for n in r.all('symbol'):
    ref=n.prop('Reference')
    if ref not in places:continue
    x,y,a=places[ref]
    for pp in n.all('property'):
        if pp.values[1] not in ['Reference','Value']:continue
        py=y+(2.54 if pp.values[1]=='Value' else 0)
        chunk=old[pp.start:pp.end];at=pp.one('at')
        chunk=chunk[:at.start-pp.start]+f'(at {x} {py} {a})'+chunk[at.end-pp.start:]
        chunk=chunk.replace('(justify left)','').replace('(justify right)','')
        reps.append((pp.start,pp.end,chunk))
for n in r.all('label'):
    if n.values[1]=='SENSE_5V':reps.append((n.start,n.end,old[n.start:n.end].replace('"SENSE_5V"','"+5V"')))
for n in r.all('text'):
    if 'Placa de energia externa:' in n.values[1]:
        reps.append((n.start,n.end,old[n.start:n.end].replace('Placa de energia externa: solar + VRLA + carregador + LVD. Nesta folha: selecao MANUAL, protecao e bucks locais.','Placa externa: solar / VRLA / carregador / LVD. J61 requer fusivel e limitacao de corrente externos. PCB: protecao + bucks.')))
changes[p]=replace_spans(old,reps)
# Imported courtyards must include pads and body plus manufacturing clearance.
for name,box in [('DC-IN-TH_DC-005-2.0_C22459640',(-10.90,-7.15,4.20,3.55)),('SOT-223-4_L6.5-W3.5-P2.30-LS7.0-BR',(-4.32,-3.50,4.32,3.50))]:
    p=base/'libraries/Hardware_Power/Hardware_Power.pretty'/f'{name}.kicad_mod';old=p.read_text(encoding='utf-8');r=parse(old);reps=[]
    for n in r.all('fp_line'):
        if n.get('layer')=='F.CrtYd':reps.append((n.start,n.end,''))
    x1,y1,x2,y2=box
    extra='\n'.join(f'(fp_line (start {ax} {ay}) (end {bx} {by}) (layer F.CrtYd) (width 0.05))' for ax,ay,bx,by in [(x1,y1,x2,y1),(x2,y1,x2,y2),(x2,y2,x1,y2),(x1,y2,x1,y1)])
    changes[p]=replace_spans(old,reps).rstrip()[:-1]+extra+'\n)\n'
print('*** Begin Patch')
for p,new in changes.items():print(patch_file(p,p.read_text(encoding='utf-8'),new),end='')
print('*** End Patch')
