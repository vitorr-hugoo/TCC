"""Patches pontuais de integração autorizados pelo usuário; imprime apply_patch."""
from pathlib import Path
import sys
from kicad_sexpr import parse, replace_spans, patch_file
sys.stdout.reconfigure(encoding='utf-8')
base = Path(__file__).resolve().parents[1]
changes = {}
p = base / 'alimentacao.kicad_sch'
old = p.read_text(encoding='utf-8'); root = parse(old)
reps = []
for n in root.all('label'):
    if n.get('uuid') == 'fb9fd816-0d14-5880-932c-94a89c14abe3':
        reps.append((n.start, n.end, '(text "FB: medir apos L60" (at 137.16 215.90 0) (effects (font (size 1.0 1.0)) (justify right)) (uuid "fb9fd816-0d14-5880-932c-94a89c14abe3"))'))
new = replace_spans(old, reps).replace('Isat tip5A; verificar min na revisao; Irms4A; DCRmax47mOhm', 'Isat min4A/tip5A; Irms min3.8A; DCRmax58mOhm')
changes[p] = new
p = base / 'tools/implementar_alimentacao.py'
changes[p] = p.read_text(encoding='utf-8').replace('Isat tip5A; verificar min na revisao; Irms4A; DCRmax47mOhm', 'Isat min4A/tip5A; Irms min3.8A; DCRmax58mOhm')
p = base / 'libraries/Hardware_Power/Hardware_Power.kicad_sym'
old = p.read_text(encoding='utf-8'); root = parse(old); reps=[]
for n in root.all('symbol'):
    if n.values[1] == 'DC-005-2.0_C22459640':
        for pp in n.all('property'):
            if pp.values[1] == 'Reference':
                reps.append((pp.start, pp.end, old[pp.start:pp.end].replace('"U"', '"J"')))
changes[p] = replace_spans(old,reps)
p = base / 'bh1750.kicad_sch'
old=p.read_text(encoding='utf-8'); root=parse(old); reps=[]
# Remove somente regulador e três ramais terminais; conserva capacitores,
# barramentos +5V/+3V3, GND e toda a circuitaria do sensor.
remove = {'b0a70338-da05-4431-8e83-24a293717f30',
          '31906b8f-4b11-4ea9-9feb-8e6028837e65',
          'b3e018ff-9063-44a0-964d-2a1fae9bac36',
          '025e2467-1d66-4f05-97bf-77ab17df0ff4',
          'ccdabbd3-f25f-4376-b8c2-616046531fa3'}
for n in root.all('symbol')+root.all('wire')+root.all('junction'):
    if n.get('uuid') in remove: reps.append((n.start,n.end,''))
changes[p]=replace_spans(old,reps)
print('*** Begin Patch')
for p,new in changes.items():print(patch_file(p,p.read_text(encoding='utf-8'),new),end='')
print('*** End Patch')
