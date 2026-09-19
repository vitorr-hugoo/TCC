"""Emit focused fixes for the meteorology migration as an apply_patch patch."""
from pathlib import Path
import sys
from kicad_sexpr import parse, patch_file, replace_spans
base=Path(__file__).resolve().parents[1]
changes={}
for stem in ['pluviometro','anemometro','biruta']:
    p=base/(stem+'.kicad_sch');s=p.read_text(encoding='utf-8');r=parse(s);reps=[]
    for n in r.all('symbol'):
        if n.prop('Reference') in ['D30','D40','D50']:
            at=n.one('at');reps.append((at.start,at.end,f'(at {at.values[1]} {at.values[2]} 270)'))
    for n in r.all('hierarchical_label'):
        a=n.one('at');reps.append((a.start,a.end,f'(at {a.values[1]} {a.values[2]} {0 if n.values[1]=="VREF_SENSOR" else 180})'))
    changes[p]=replace_spans(s,reps)
p=base/'libraries/Sensores_Meteo/Sensores_Meteo.kicad_sym'
changes[p]=p.read_text(encoding='utf-8').replace('(version 20250114)','(version 20241209)')
p=base/'mcu.kicad_sch';s=p.read_text(encoding='utf-8');r=parse(s)
node=r.one('lib_symbols').all('symbol')[0]
lib=s[node.start:node.end];lr=parse(lib);reps=[]
for n in lr.all('symbol'):
    if int(n.values[1].split('_')[-2])>2:reps.append((n.start,n.end,''))
lib=replace_spans(lib,reps).replace('Nucleo_F446:NUCLEO-F446RE','Hardware_MCU:NUCLEO-F446RE_Morpho').replace('"NUCLEO-F446RE_','"NUCLEO-F446RE_Morpho_')
# Expose stacked return pins; prevent implicit CN7_GND power nets.
lib=lib.replace('(hide yes)\n\t\t\t\t\t(name "CN7_GND"','(name "CN7_GND"').replace('(hide yes)\n\t\t\t\t\t(name "CN10_GND"','(name "CN10_GND"')
s=replace_spans(s,[(node.start,node.end,lib)]).replace('(lib_id "Nucleo_F446:NUCLEO-F446RE")','(lib_id "Hardware_MCU:NUCLEO-F446RE_Morpho")')
changes[p]=s
loc=lib.replace('"Hardware_MCU:NUCLEO-F446RE_Morpho"','"NUCLEO-F446RE_Morpho"',1)
changes[base/'libraries/Hardware_MCU/Hardware_MCU.kicad_sym']='(kicad_symbol_lib (version 20241209) (generator "kicad_symbol_editor")\n'+loc+'\n)\n'
p=base/'sym-lib-table';s=p.read_text(encoding='utf-8');s=s.rstrip()[:-1]+'  (lib (name "Hardware_MCU")(type "KiCad")(uri "${KIPRJMOD}/libraries/Hardware_MCU/Hardware_MCU.kicad_sym")(options "")(descr "Interface Morpho CN7/CN10 da Nucleo-F446RE; AVDD exportado"))\n)\n';changes[p]=s
print('*** Begin Patch')
for p,s in changes.items():
    if p.exists():sys.stdout.write(patch_file(p,p.read_text(encoding='utf-8'),s))
    else:print(f'*** Add File: {p.as_posix()}\n'+'\n'.join('+'+l for l in s.splitlines()))
print('*** End Patch')
