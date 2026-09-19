"""Troca C67/C68 por 0805/25V; mantém capacitores de entrada em 50V."""
from pathlib import Path
import json,sys
from kicad_sexpr import parse,replace_spans,patch_file
from consultar_jlc_alimentacao import query
sys.stdout.reconfigure(encoding='utf-8')
base=Path(__file__).resolve().parents[1]
meta=query('C15850')
assert meta['componentModelEn']=='CL21A106KAYNNNE' and meta['canPresaleNumber']>0
changes={}
p=base/'docs/alimentacao/catalogo_jlc_final.json'
data=json.loads(p.read_text(encoding='utf-8'))
data['partes']=[x for x in data['partes'] if x.get('componentCode')!='C15850']+[meta]
changes[p]=json.dumps(data,ensure_ascii=False,indent=2)+'\n'
p=base/'alimentacao.kicad_sch';s=p.read_text(encoding='utf-8');r=parse(s);reps=[]
props={'Value':'10u / 25V','Footprint':'Capacitor_SMD:C_0805_2012Metric',
       'LCSC':'C15850','LCSC Part':'C15850','MPN':meta['componentModelEn'],
       'Datasheet':meta['dataManualUrl']}
for n in r.all('symbol'):
    if n.prop('Reference') in ['C67','C68']:
        for pp in n.all('property'):
            if pp.values[1] in props:
                text=s[pp.start:pp.end].replace(json.dumps(pp.values[2],ensure_ascii=False),json.dumps(props[pp.values[1]],ensure_ascii=False),1)
                reps.append((pp.start,pp.end,text))
changes[p]=replace_spans(s,reps)
p=base/'docs/alimentacao/componentes_implementados.json'
data=json.loads(p.read_text(encoding='utf-8'))
for n in data:
    if n['reference'] in ['C67','C68']:
        n.update(part='10uLV',value=props['Value'],footprint=props['Footprint'],lcsc='C15850')
changes[p]=json.dumps(data,ensure_ascii=False,indent=2)+'\n'
p=base/'tools/implementar_alimentacao.py';s=p.read_text(encoding='utf-8')
s=s.replace(" '22u':", " '10uLV':('Device:C','Capacitor_SMD:C_0805_2012Metric','C15850','10u / 25V','X5R; 10%; considerar perda por bias DC'),\n '22u':",1)
s=s.replace("s.shunt('10u','C67'","s.shunt('10uLV','C67'").replace("s.shunt('10u','C68'","s.shunt('10uLV','C68'")
changes[p]=s
print('*** Begin Patch')
for p,new in changes.items():print(patch_file(p,p.read_text(encoding='utf-8'),new),end='')
print('*** End Patch')
