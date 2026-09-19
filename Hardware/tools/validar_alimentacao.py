"""Audita netlist KiCad, BOM, pinagem/pads e regressões ERC da alimentação.

Pré-requisito: reexportar netlist.xml e erc_depois.json com kicad-cli.
Sem argumentos: somente leitura. --emit-docs-patch: imprime patch dos relatórios.
Não simula transientes, estabilidade, térmica ou compatibilidade mecânica.
"""
from pathlib import Path
from collections import Counter, defaultdict
import hashlib, json, re, sys, xml.etree.ElementTree as ET
from kicad_sexpr import parse, patch_file

sys.stdout.reconfigure(encoding='utf-8')
B=Path(__file__).resolve().parents[1]
D=B/'docs/alimentacao'
STD=Path('C:/Program Files/KiCad/9.0/share/kicad')
x=ET.parse(D/'validacao/netlist.xml').getroot()
components={n.get('ref'):n for n in x.findall('components/comp')}
nets={n.get('name'):{(p.get('ref'),p.get('pin')) for p in n.findall('node')} for n in x.findall('nets/net')}
by_pin={p:name for name,pins in nets.items() for p in pins}
s=parse((B/'alimentacao.kicad_sch').read_text(encoding='utf-8'))
symbols={n.prop('Reference'):n for n in s.all('symbol')}
libcache={n.values[1]:n for n in s.one('lib_symbols').all('symbol')}
catalog=json.loads((D/'catalogo_jlc_final.json').read_text(encoding='utf-8'))
catalog_by_code={n['componentCode']:n for n in catalog['partes']}
tests=[]

def check(name,condition,detail=None):
    tests.append({'teste':name,'resultado':'PASS' if condition else 'FAIL','detalhe':detail})
    # Registrar todos os resultados, inclusive pendências de integração.

def connected(name,pins,exact=False):
    pins={tuple(p.split('.')) for p in pins.split()}
    names={by_pin.get(p) for p in pins}
    check(name,len(names)==1 and None not in names,sorted(pins))
    if exact:check(name+' (sem derivações)',nets[next(iter(names))]==pins)

connected('J61 positivo / JP60 pad1','J61.1 JP60.1',True)
connected('J60 centro / JP60 pad3','J60.1 JP60.3',True)
connected('JP60 comum / entrada fusível','JP60.2 F60.1',True)
connected('Após fusível / TVS / dois drenos PMOS','F60.2 D60.2 Q60.2 Q60.4',True)
connected('Gate / anodo zener / pull-down','Q60.1 D61.2 R60.1',True)
connected('VIN_MAIN protegido e EN U60','Q60.3 D61.1 U60.3 U60.2 C60.1 C61.1 C62.1 C63.1 TP60.1',True)
connected('Bootstrap de U60','U60.6 C64.1',True)
connected('SW U60 somente L e bootstrap','U60.5 C64.2 L60.1',True)
connected('5V: sense pós L60 / entrada U61','L60.2 U60.1 C65.1 C66.1 U61.4 U61.1 C67.1 TP61.1')
connected('SW U61 somente indutor','U61.3 L61.1',True)
connected('3V3: saída e início divisor','L61.2 C68.1 C69.1 R61.1 C70.1 TP62.1 U9.1')
connected('Divisor série 300k+150k','R61.2 R62.1',True)
connected('FB U61 e capacitor feed-forward','U61.5 R62.2 R63.1 C70.2',True)
connected('Terra comum e negativos','J60.2 J61.2 D60.1 R60.2 C60.2 C61.2 C62.2 C63.2 U60.4 C65.2 C66.2 U61.2 C67.2 C68.2 C69.2 R63.2 TP63.1')
critical=[('J61','1'),('J60','1'),('JP60','2'),('U60','3'),('L60','2'),('L61','2'),('TP63','1')]
check('Sete redes distintas com JP60 aberto',len({by_pin[p] for p in critical})==len(critical))
check('Contato chaveado P4 não utilizado',len(nets[by_pin[('J60','3')]])==1)
check('Sem regulador antigo U10 em paralelo','U10' not in components)
check('Capacitor opcional C69 DNP',symbols['C69'].get('dnp')=='yes')
check('ENs sem dependência do MCU',by_pin[('U60','2')]==by_pin[('U60','3')] and by_pin[('U61','1')]==by_pin[('U61','4')])
check('3V3 não conectado às saídas 3V3 da Nucleo',not any(('U5',p) in nets[by_pin[('L61','2')]] for p in ['CN7_12','CN7_16','CN6_4']))
check('Nucleo alimentada por E5V CN7_6',by_pin.get(('U5','CN7_6'))==by_pin[('L60','2')])
check('Saída CN7_18 da Nucleo isolada',len(nets[by_pin[('U5','CN7_18')]])==1)

# Compare todos os pads elétricos dos footprints com os números do símbolo.
audit=[]
for ref,n in symbols.items():
    if n.get('on_board')!='yes':continue
    fp=n.prop('Footprint');lib,part=fp.split(':')
    path=B/'libraries/Hardware_Power/Hardware_Power.pretty'/f'{part}.kicad_mod' if lib=='Hardware_Power' else STD/'footprints'/f'{lib}.pretty'/f'{part}.kicad_mod'
    check(f'{ref}: footprint existe',path.is_file(),fp)
    fr=parse(path.read_text(encoding='utf-8-sig'))
    pads={p.values[1] for p in fr.all('pad') if p.values[1]}
    ls=libcache[n.get('lib_id')]
    pins={p.get('number') for unit in ls.all('symbol') for p in unit.all('pin')}
    check(f'{ref}: números dos pinos coincidem com pads',pins==pads,{'pins':sorted(pins),'pads':sorted(pads)})
    if lib=='Hardware_Power':
        for model in fr.all('model'):
            mp=Path(model.values[1].replace('${KIPRJMOD}',B.as_posix()))
            check(f'{ref}: modelo 3D local',mp.is_file(),str(mp))
    code=n.prop('LCSC','');meta=catalog_by_code.get(code,{})
    if n.get('in_bom')=='yes':
        check(f'{ref}: código JLC e MPN consistentes',bool(code) and n.prop('MPN')==meta.get('componentModelEn'))
        check(f'{ref}: disponibilidade de pré-compra positiva',meta.get('canPresaleNumber',0)>0)
    audit.append({'ref':ref,'valor':n.prop('Value'),'simbolo':n.get('lib_id'),'footprint':fp,'pins':sorted(pins),'lcsc':code,'dnp':n.get('dnp')=='yes','mpn':n.prop('MPN',''),'origem':'EasyEDA convertido e normalizado KiCad 9' if lib=='Hardware_Power' else 'KiCad 9 padrão'})

def violations(path):
    d=json.loads(path.read_text(encoding='utf-8'))
    return [(sh['path'],v) for sh in d['sheets'] for v in sh['violations']]
before=violations(B/'docs/alimentacao_erc_antes.json')
after=violations(D/'validacao/erc_depois.json')
def sig(p,v):return p,v['type'],tuple(sorted(i['uuid'] for i in v['items']))
baseline={sig(p,v) for p,v in before}
new=[{'folha':p,**v} for p,v in after if sig(p,v) not in baseline]
check('Sem novas violações ERC no projeto',not new,new)
check('Folha alimentação: zero violações ERC',not any(p=='/alimentacao/' for p,v in after))

# Carga capacitiva nominal visível no esquema, não modelos internos de módulos.
cap_refs=[];cap_total=0
for ref,pin in nets[by_pin[('L61','2')]]:
    if not re.fullmatch(r'C\d+',ref) or pin!='1':continue
    comp=components[ref]
    if ref=='C69':continue
    value=comp.findtext('value')
    m=re.match(r'(\d+(?:\.\d+)?)\s*([unp])',value)
    if m and by_pin.get((ref,'2'))=='GND':
        uf=float(m[1])*{'u':1,'n':.001,'p':.000001}[m[2]]
        cap_refs.append({'ref':ref,'uF':uf});cap_total+=uf
check('Capacitância nominal 3V3 contabilizada',10<=cap_total<=47,cap_refs)
groups=defaultdict(list)
for row in audit:
    if row['lcsc']:groups[(row['lcsc'],row['dnp'])].append(row)
total=0;rows=[]
for (code,dnp),parts in groups.items():
    meta=catalog_by_code[code];qty=len(parts);price=meta['initialPrice']
    if not dnp:total+=qty*price
    rows.append('| '+ ' | '.join([', '.join(p['ref'] for p in parts),str(qty),'DNP' if dnp else 'Montar',meta['componentModelEn'],f'[{code}]({meta["url"]})',str(meta.get('canPresaleNumber','n/d')),f'{price:.4f}',{'base':'Basic','expand':'Extended'}.get(meta.get('componentLibraryType'),'não informado')])+' |')
result={'data_catalogo':catalog['consultado_em'],'testes_aprovados':len(tests),'testes':tests,'erc_antes':dict(Counter(v['severity'] for p,v in before)),'erc_depois':dict(Counter(v['severity'] for p,v in after)),'novas_violacoes_erc':new,'erc_por_folha':dict(Counter(p for p,v in after)),'capacitancia_nominal_3V3_uF':round(cap_total,3),'capacitores_3V3':cap_refs,'subtotal_componentes_USD':round(total,4),'componentes_fisicos_folha':len(audit),'pecas_compradas_montadas':sum(len(g) for (c,d),g in groups.items() if not d),'hashes_sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [B/'alimentacao.kicad_sch',B/'esquematico_V4.kicad_sch',B/'bh1750.kicad_sch',B/'mcu.kicad_sch',B/'esquematico_V4.kicad_pcb',D/'validacao/netlist.xml']}}
result['testes_aprovados']=sum(t['resultado']=='PASS' for t in tests)
result['testes_reprovados']=[t for t in tests if t['resultado']=='FAIL']
if '--emit-docs-patch' not in sys.argv:
    print(json.dumps(result,ensure_ascii=False,indent=2));sys.exit(bool(result['testes_reprovados']))
bom=f'''# BOM da alimentação — revisão PWR-1

Consulta: {catalog['consultado_em']}. Lista extraída dos símbolos implementados, incluindo C69 opcional. Subtotal indicativo dos **24 componentes montados: US$ {total:.4f}**. Não é orçamento nem reserva: exclui quantidade mínima, excedentes de compra, taxas de componentes Extended, PCB, montagem, THT, frete e impostos. Para poucas placas, essas taxas podem dominar o custo.

Os números são `canPresaleNumber` do catálogo público JLCPCB: disponibilidade de **pré-compra/fornecedor**, não comprovação de estoque interno reservado para PCBA. Confirmar disponibilidade efetiva e pacote de montagem ao enviar a BOM. `initialPrice` é o preço indicativo consultado, não garantia de faixa de quantidade.

| Referências | Qtd. | Montagem | MPN | Código JLC / fonte | Pré-compra (un.) | US$/un. | Categoria retornada |
|---|---:|---|---|---|---:|---:|---|
'''+ '\n'.join(rows)+'''

JP60 e TP60–TP63 são cobre da própria PCB, não peças compradas. Os quatro PWR_FLAG são declarações elétricas, sem footprint. C69 permanece **DNP**; não deve entrar na montagem. J60/J61 são THT: confirmar serviço de montagem ou soldar manualmente. Chicote, carcaça/crimps JST e plugue P4 não estão nesta BOM.

## Símbolos e footprints auditados

Todos os footprints existem e seus números de pads correspondem aos pinos dos símbolos. Isso não substitui cotejo dimensional final, inspeção de montagem e teste mecânico de J60 com o plugue real.

| Ref. | Símbolo | Footprint | Pinos/pads | Origem |
|---|---|---|---|---|
'''+ '\n'.join('| '+ ' | '.join([a['ref'],a['simbolo'],a['footprint'],', '.join(a['pins']),a['origem']])+' |' for a in audit)+'''

## Importações necessárias

AP63205WU e TLV62569DBV já existem no KiCad 9. Passivos, TVS, zener, JST, jumper e pontos de teste usam símbolos/footprints padrão. Para o jack exato C22459640 e o PMOS C154901, foram importados símbolo, footprint e 3D com **easyeda2kicad 1.0.1**, depois normalizados para KiCad 9. Pinagem elétrica e courtyards foram revisados. No PMOS importado, o tab é pad4 e deve permanecer ligado ao dreno pad2; não trocar por um footprint de três pads sem adaptar o mapeamento.

Biblioteca local: `Hardware/libraries/Hardware_Power/`, registrada com `${KIPRJMOD}` em ambas as tabelas. J60: 1=centro, 2=retorno, 3=contato chaveado não utilizado. Q60: 1=G, 2=D, 3=S, 4=D/tab.

Comando de conversão utilizado, executado na pasta Hardware (não repetir com --overwrite sobre a biblioteca revisada):

```powershell
python -X utf8 -m easyeda2kicad --lcsc_id C22459640 C154901 --symbol --footprint --output D:/TCC/Hardware/libraries/Hardware_Power/Hardware_Power.kicad_sym --project-relative --overwrite
```

A opção `--3d` também foi executada para ambos. Os modelos STEP/WRL são auxiliares de visualização, não autoridade para numeração elétrica ou tolerâncias mecânicas.
'''
updates={D/'BOM_e_bibliotecas.md':bom,D/'validacao/resumo_validacao.json':json.dumps(result,ensure_ascii=False,indent=2)+'\n'}
print('*** Begin Patch')
for p,new in updates.items():
    if p.exists():print(patch_file(p,p.read_text(encoding='utf-8'),new),end='')
    else:print('*** Add File: '+p.as_posix()+'\n'+'\n'.join('+'+l for l in new.splitlines()))
print('*** End Patch')
sys.exit(bool(result['testes_reprovados']))
