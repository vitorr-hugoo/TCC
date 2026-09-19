"""One-shot schematic migration, emits an apply_patch patch, not direct writes.

Run from the baseline only; after implementation KiCad files are authoritative.
Uses datasheet pin numbers and flattened standard symbols, not drawn wire images.
"""
from pathlib import Path
import json, math, sys, uuid
from kicad_sexpr import Node, parse, replace_spans, patch_file

sys.stdout.reconfigure(encoding='utf-8')
BASE=Path(__file__).resolve().parents[1]
STD=Path('C:/Program Files/KiCad/9.0/share/kicad')
ROOT='0c305a6e-f9cd-49a1-b146-c2956e5469b6'
SHEET='19a3c20f-53ab-4562-aabc-58e6e99571a0'
PATH=f'/{ROOT}/{SHEET}'
def q(v): return json.dumps(str(v),ensure_ascii=False)
def uid(v): return str(uuid.uuid5(uuid.NAMESPACE_URL,'tcc-power-1/'+v))
def fmt(v): return f'{float(v):.5f}'.rstrip('0').rstrip('.')
def xy(p): return ' '.join(fmt(v) for v in p)
def effects(size=1.27,more=''): return f'(effects (font (size {size} {size})) {more})'
def prop(k,v,x=0,y=0,hide=True,just=''):
    return f'(property {q(k)} {q(v)} (at {fmt(x)} {fmt(y)} 0) {effects(1.27, ("(hide yes)" if hide else just))})'

libcache={}
def libsymbol(lib,name):
    key=lib+':'+name
    if key in libcache:return libcache[key]
    path=BASE/'libraries/Hardware_Power/Hardware_Power.kicad_sym' if lib=='Hardware_Power' else STD/'symbols'/f'{lib}.kicad_sym'
    text=path.read_text(encoding='utf-8-sig'); root=parse(text)
    node=next(n for n in root.all('symbol') if n.values[1]==name)
    if node.one('extends'):
        parent=libsymbol(lib,node.get('extends')); pr=parse(parent); reps=[]
        for pp in pr.all('property'):
            new=next((p for p in node.all('property') if p.values[1]==pp.values[1]),None)
            if new:reps.append((pp.start,pp.end,text[new.start:new.end]))
        result=replace_spans(parent,reps).replace(q(pr.values[1]),q(name),1)
        result=result.replace('"'+pr.values[1]+'_','"'+name+'_')
    else:result=text[node.start:node.end]
    libcache[key]=result
    return result

inv={p['componentCode']:p for p in json.loads((BASE/'docs/alimentacao/catalogo_jlc_final.json').read_text(encoding='utf-8'))['partes']}
R='Resistor_SMD:R_0603_1608Metric'
C='Capacitor_SMD:C_0603_1608Metric'
PARTS={
 'P4':('Hardware_Power:DC-005-2.0_C22459640','Hardware_Power:DC-IN-TH_DC-005-2.0_C22459640','C22459640','P4 / DC-005-2.0','20V; 3A; centro positivo; confirmar plugue 5.5/2.1'),
 'JST':('Connector_Generic:Conn_01x02','Connector_JST:JST_XH_B2B-XH-A_1x02_P2.50mm_Vertical','C158012','J_PWR / JST-XH','3A; 250V; passo 2.50mm; p1 positivo, p2 GND'),
 'JP':('Jumper:SolderJumper_3_Open','Jumper:SolderJumper-3_P2.0mm_Open_TrianglePad1.0x1.5mm_NumberLabels','','SEL_FONTE','Pad de cobre; fechar apenas 1-2 OU 2-3'),
 'F':('Device:Fuse','Fuse:Fuse_1206_3216Metric','C553929','1.5A / 32V lento','I2t tip 0.4137 A2s; interrupcao 50A; DCR tip 140mOhm'),
 'TVS':('Device:D_TVS','Diode_SMD:D_SMB','C3001977','SMBJ16CA','Bidirecional; VRWM16V; VC26V a23.1A; 600W 10/1000us'),
 'Q':('Hardware_Power:DMP6023LE-13','Hardware_Power:SOT-223-4_L6.5-W3.5-P2.30-LS7.0-BR','C154901','DMP6023LE-13','PMOS60V; VGS +/-20V; 28mOhm max @-10V; 1G 2D 3S 4D'),
 'Z':('Device:D_Zener','Diode_SMD:D_SOD-123','C19077408','BZT52C10 / 10V','350mW; VZ9.5-10.5V @5mA; K=source, A=gate'),
 '100k':('Device:R',R,'C25803','100k / 1%','0.1W; 75V'),
 '300k':('Device:R',R,'C23024','300k / 1%','0.1W; 75V'),
 '150k':('Device:R',R,'C22807','150k / 1%','0.1W; 75V'),
 '47u':('Device:C_Polarized','Capacitor_SMD:CP_Elec_6.3x5.8','C87068','47u / 35V','20%; -55..105C; 1000h@105C; bulk de cabo, nao substitui MLCC'),
 '10u':('Device:C','Capacitor_SMD:C_1206_3216Metric','C13585','10u / 50V','X5R; 10%; considerar perda por bias DC'),
 '10uLV':('Device:C','Capacitor_SMD:C_0805_2012Metric','C15850','10u / 25V','X5R; 10%; considerar perda por bias DC'),
 '22u':('Device:C','Capacitor_SMD:C_1206_3216Metric','C12891','22u / 25V','X5R; 10%; considerar perda por bias DC'),
 '100n':('Device:C',C,'C14663','100n / 50V','X7R; 10%'),
 '6p8':('Device:C',C,'C1679','6p8 / 50V','C0G; +/-0.25pF; feed-forward'),
 'L5':('Device:L','Inductor_SMD:L_Sunlord_MWSA0603S','C408447','4u7 / MWSA0603S','Isat min7.2A/tip9A; Irms min5A; DCRmax33mOhm'),
 'L3':('Device:L','Inductor_SMD:L_Sunlord_MWSA0402S','C408335','2u2 / MWSA0402S','Isat min4A/tip5A; Irms min3.8A; DCRmax58mOhm'),
 'BUCK5':('Regulator_Switching:AP63205WU','Package_TO_SOT_SMD:TSOT-23-6','C2071056','AP63205WU-7','5V fixos; 2A CI; VIN3.8-32V; 1.1MHz; IQtip22uA'),
 'BUCK3':('Regulator_Switching:TLV62569DBV','Package_TO_SOT_SMD:SOT-23-5','C141836','TLV62569DBVR','2A CI; VIN2.5-5.5V; 1.5MHz; IQtip35uA; sem PG'),
 'TP':('Connector:TestPoint','TestPoint:TestPoint_Pad_D1.5mm','','','Cobre exposto; nao comprar'),
 'FLAG':('power:PWR_FLAG','','','PWR_FLAG','Declaracao ERC, nao componente'),
}

class Sheet:
    def __init__(self):self.items=[];self.libs=set();self.count=0;self.components=[];self.pins={};self.wires=[]
    def u(self):self.count+=1;return uid('item/'+str(self.count))
    def wire(self,*pts):
        for a,b in zip(pts,pts[1:]):
            a=tuple(round(v,5) for v in a);b=tuple(round(v,5) for v in b)
            if a==b:continue
            assert a[0]==b[0] or a[1]==b[1],(a,b)
            self.wires.append((a,b))
            self.items.append(f'(wire (pts (xy {xy(a)}) (xy {xy(b)})) (stroke (width 0) (type default)) (uuid {q(self.u())}))')
    def dot(self,p):self.items.append(f'(junction (at {xy(p)}) (diameter 0) (color 0 0 0 0) (uuid {q(self.u())}))')
    def label(self,name,p,kind='label',angle=0,shape='passive'):
        self.items.append(f'({kind} {q(name)} '+(f'(shape {shape}) ' if kind!='label' else '')+f'(at {xy(p)} {angle}) {effects(1.27,"(justify left)" if angle in [0,180] else "")} (uuid {q(self.u())}))')
    def gnd(self,p):self.label('GND',p,'global_label')
    def text(self,t,p,size=1.27):self.items.append(f'(text {q(t)} (at {xy(p)} 0) {effects(size,"(justify left top)")} (uuid {q(self.u())}))')
    def nc(self,p):self.items.append(f'(no_connect (at {xy(p)}) (uuid {q(self.u())}))')
    def sym(self,part,ref,p,angle=0,role='',value=None,field=None,dnp=False):
        lib,fp,code,val,rating=PARTS[part];self.libs.add(lib)
        libtext=libsymbol(*lib.split(':'));lr=parse(libtext)
        x,y=p; a=math.radians(angle);pins={}
        for un in lr.all('symbol'):
            for pin in un.all('pin'):
                px,py=map(float,pin.one('at').values[1:3])
                pins[pin.get('number')]=(round(x+px*math.cos(a)-py*math.sin(a),5),round(y-px*math.sin(a)-py*math.cos(a),5))
        self.pins[ref]=pins
        if field is None:
            field=(x+3.81,y-1.27) if part not in ['P4','JST','Q','BUCK5','BUCK3','JP','TP','FLAG'] and angle==0 else (x,y-10.16)
        fx,fy=field
        fs=[prop('Reference',ref,fx,fy,part=='FLAG','(justify left)'),prop('Value',val if value is None else value,fx,fy+2.54,part=='FLAG','(justify left)')]
        meta=inv.get(code,{})
        ds=meta.get('dataManualUrl','')
        if part=='BUCK5':ds='https://www.diodes.com/datasheet/download/AP63200-AP63201-AP63203-AP63205.pdf'
        if part=='BUCK3':ds='https://www.ti.com/lit/ds/symlink/tlv62569.pdf'
        if part=='Q':ds='https://www.diodes.com/datasheet/download/DMP6023LE.pdf'
        fs += [prop(k,v,x,y) for k,v in [('Footprint',fp),('Datasheet',ds),('Manufacturer',meta.get('componentBrandEn','')),('MPN',meta.get('componentModelEn','')),('LCSC',code),('LCSC Part',code),('Rating',rating),('Function',role)]]
        self.items.append(f'(symbol (lib_id {q(lib)}) (at {xy(p)} {angle}) (unit 1) (exclude_from_sim no) (in_bom {"no" if part in ["TP","JP","FLAG"] else "yes"}) (on_board {"no" if part=="FLAG" else "yes"}) (dnp {"yes" if dnp else "no"}) (uuid {q(uid(ref))})\n'+ '\n'.join(fs)+f'\n(instances (project "esquematico_V4" (path {q(PATH)} (reference {q(ref)}) (unit 1)))))')
        self.components.append(dict(reference=ref,part=part,value=val if value is None else value,lib=lib,footprint=fp,lcsc=code,role=role,dnp=dnp))
        return pins
    def shunt(self,part,ref,x,y,top,bottom,role,dnp=False):
        pins=self.sym(part,ref,(x,y),role=role,dnp=dnp)
        self.wire((x,top),pins['1']);self.wire(pins['2'],(x,bottom));self.gnd((x,bottom));self.dot((x,top))
    def render(self):
        libs=[libsymbol(*lib.split(':')).replace(q(lib.split(':')[1]),q(lib),1) for lib in sorted(self.libs)]
        return f'(kicad_sch (version 20250114) (generator "eeschema") (generator_version "9.0") (uuid "204d78b9-2f67-4bc5-b61f-ac323b0f96af") (paper "A3")\n(title_block (title "PCB PRINCIPAL - alimentacao local protegida") (date "2026-09-18") (rev "PWR-1") (company "TCC - Estacao meteorologica"))\n(lib_symbols\n'+ '\n'.join(libs)+')\n'+'\n'.join(self.items)+'\n(embedded_fonts no))\n'

changes={}
# Preserve the converter output, but fix unspecified ERC pin types in the local library.
lp=BASE/'libraries/Hardware_Power/Hardware_Power.kicad_sym'
lt=lp.read_text(encoding='utf-8-sig').replace('(pin unspecified line','(pin passive line')
changes[lp]=lt
# load normalized local symbols into cache
lr=parse(lt)
for n in lr.all('symbol'):libcache['Hardware_Power:'+n.values[1]]=lt[n.start:n.end]

s=Sheet()
s.text('ALIMENTACAO DA PCB PRINCIPAL | entrada nominal 12 V, nao regulada | faixa de projeto 10 a 16 Vcc',(20.32,15.24),2.0)
s.text('Placa de energia externa: solar + VRLA + carregador + LVD. Nesta folha: selecao MANUAL, protecao e bucks locais.',(20.32,23.0),1.5)
s.text('1  ENTRADAS / SELECAO / PROTECAO',(20.32,35.56),1.8)
s.text('2  BUCK 5 V  |  CI 2 A / VALIDAR TERMICA',(20.32,137.16),1.8)
s.text('3  BUCK 3,3 V  |  SEM MCU PARA PARTIDA',(220.98,137.16),1.8)

j=s.sym('P4','J60',(30.48,55.88),field=(20.32,43.18),role='Fonte externa SELV regulada 12V, centro positivo; nao ligar painel diretamente')
s.wire(j['1'],(85.09,54.61),(85.09,64.77))
s.nc(j['3']);s.wire(j['2'],(43.18,59.69),(43.18,66.04));s.gnd((43.18,66.04))
j=s.sym('JST','J61',(30.48,93.98),180,field=(20.32,81.28),role='Placa de energia externa: p1=BUS nominal12V apos LVD, p2=GND')
s.wire(j['1'],(63.5,93.98),(63.5,80.01),(85.09,80.01),(85.09,74.93))
s.wire(j['2'],(43.18,91.44),(43.18,99.06));s.gnd((43.18,99.06))
jp=s.sym('JP','JP60',(85.09,69.85),90,field=(92.71,48.26),role='1-2 = J61; 2-3 = J60; aberto de fabrica, fechar na configuracao')
s.label('P4_POS',(55.88,54.61));s.label('ENERGIA_POS',(64.77,80.01))
f=s.sym('F','F60',(111.76,69.85),90,field=(102.87,57.15),role='Protecao local; fonte externa limitada a <=2A; VRLA requer fusivel junto a bateria')
s.wire(jp['2'],f['1'])
d=s.sym('TVS','D60',(137.16,92.71),90,field=(143.51,90.17),role='TVS bidirecional depois do fusivel e antes do PMOS; surtos limitados')
s.wire(f['2'],(137.16,69.85),(172.72,69.85))
s.wire((137.16,69.85),d['2']);s.wire(d['1'],(137.16,113.03));s.gnd((137.16,113.03));s.dot((137.16,69.85))
# Imported PMOS rotated90: drains left, source right, gate down.
qp=s.sym('Q','Q60',(182.88,72.39),90,field=(171.45,48.26),role='Reverse polarity: 2/4 drains toward source connector; 3 source toward buck; NOT reverse-current blocker')
s.wire((172.72,69.85),qp['2']);s.wire(qp['4'],(167.64,67.31),(167.64,69.85));s.dot((167.64,69.85))
s.wire(qp['3'],(213.36,69.85),(254,69.85),(279.4,69.85),(304.8,69.85),(330.2,69.85),(370.84,69.85))
z=s.sym('Z','D61',(213.36,90.17),270,field=(219.71,87.63),role='Limita |VGS|; catodo1 na source3; anodo2 no gate1')
s.wire((213.36,69.85),z['1']);s.dot((213.36,69.85))
s.wire(qp['1'],(182.88,99.06),(213.36,99.06),z['2'])
r=s.sym('100k','R60',(182.88,109.22),role='Pull-down do gate; I tip ~20uA @12V com zener10V')
s.wire((182.88,99.06),r['1']);s.wire(r['2'],(182.88,118.11));s.gnd((182.88,118.11));s.dot((182.88,99.06))
s.shunt('47u','C60',254,92.71,69.85,113.03,'Bulk/amortecimento do cabo; positivo p1; ESR ajuda hot-plug')
s.shunt('10u','C61',279.4,92.71,69.85,113.03,'MLCC de entrada do AP63205; junto ao VIN/GND')
s.shunt('10u','C62',304.8,92.71,69.85,113.03,'Reserva de capacitancia efetiva sob bias 16V')
s.shunt('100n','C63',330.2,92.71,69.85,113.03,'Desacoplamento HF junto ao CI')
s.label('VIN_MAIN',(355.6,69.85));s.sym('TP','TP60',(355.6,59.69),value='VIN_MAIN',field=(350.52,50.8));s.wire((355.6,59.69),(355.6,69.85));s.dot((355.6,69.85))
s.sym('FLAG','#FLG060',(370.84,69.85));s.sym('FLAG','#FLG061',(370.84,113.03));s.gnd((370.84,113.03))
s.text('JP60 aberto: placa desligada. Fechar SOMENTE 1-2 (J_PWR) ou 2-3 (P4).\nNunca fechar os tres pads. Desenergizar antes de alterar. GNDs sao comuns.',(20.32,118.11),1.27)
s.text('Nao conectar bateria sem LVD/fusivel externo. Nao ha OR automatico nesta PCB.\nTVS nao protege contra sobretensao sustentada nem raio direto.',(254,120.65),1.27)

# 5 V buck and explicit feedback loop. Upper SW route, bootstrap underneath.
u=s.sym('BUCK5','U60',(76.2,175.26),field=(63.5,155.0),role='Converte VIN_MAIN para5V; EN ligado aVIN; FB direto na saida filtrada')
s.label('VIN_MAIN',(27.94,172.72));s.wire((27.94,172.72),(55.88,172.72),u['3']);s.wire((55.88,172.72),(55.88,177.8),u['2']);s.dot((55.88,172.72))
l=s.sym('L5','L60',(124.46,172.72),90,field=(111.76,160.02),role='Armazena energia e filtra SW; blindado; nao e bead')
s.wire(u['5'],(93.98,172.72),l['1']);s.wire(l['2'],(154.94,172.72),(180.34,172.72),(200.66,172.72))
c=s.sym('100n','C64',(101.6,191.77),90,field=(92.71,198.12),role='Bootstrap entre BST6 e SW5; nunca aGND')
s.wire(u['6'],(88.9,175.26),(88.9,191.77),c['1']);s.wire(c['2'],(111.76,191.77),(111.76,181.61),(93.98,181.61),(93.98,172.72));s.dot((93.98,172.72))
s.wire(u['1'],(86.36,213.36),(195.58,213.36),(195.58,172.72));s.dot((195.58,172.72));s.label('SENSE_5V',(142.24,213.36))
s.shunt('22u','C65',154.94,190.5,172.72,205.74,'Saida5V; 22uF25V em paralelo comC66')
s.shunt('22u','C66',180.34,190.5,172.72,205.74,'Saida5V; Ctotal44uFnominal')
s.wire(u['4'],(76.2,205.74));s.gnd((76.2,205.74))
s.sym('TP','TP61',(195.58,160.02),value='+5V',field=(193.04,149.86));s.wire((195.58,160.02),(195.58,172.72))
s.sym('FLAG','#FLG062',(200.66,172.72));s.label('+5V',(200.66,172.72),'hierarchical_label',180,'output')
s.text('L60=4,7uH; C65+C66=44uF nominais; FB ligado APOS L60.\nC61/C62/C63 fisicamente junto a U60, apesar de desenhados acima.\nAlvo inicial: 5V externo <=0,5A + carga do buck3V3; validar termica.',(20.32,226.06))

# 3.3 V buck, divider450k/100k and6.8pF across both upper resistors.
u=s.sym('BUCK3','U61',(256.54,175.26),field=(242.57,155.0),role='Buck5V->3V3; R61+R62=450k, R63=100k; EN sempre ligado')
s.label('+5V',(220.98,172.72),'global_label');s.wire((220.98,172.72),(238.76,172.72),u['4']);s.wire((238.76,172.72),(238.76,175.26),u['1']);s.dot((238.76,172.72))
s.shunt('10uLV','C67',226.06,191.77,172.72,207.01,'Entrada local U61; alem dos capacitores da saida5V')
l=s.sym('L3','L61',(284.48,172.72),90,field=(274.32,160.02),role='2.2uH blindado; LC aprovado pela matriz do datasheetTI')
s.wire(u['3'],l['1']);s.wire(l['2'],(302.26,172.72),(325.12,172.72),(347.98,172.72),(373.38,172.72),(391.16,172.72))
s.shunt('10uLV','C68',347.98,191.77,172.72,207.01,'Saida3V3 local; mais30.5uF das folhas existentes =40.5uF nominais')
s.shunt('22u','C69',373.38,191.77,172.72,207.01,'DNP: reserva, nao montar sem rever estabilidade e capacitancia total',dnp=True)
r=s.sym('300k','R61',(302.26,184.15),role='Metade superior do divisor, junto aR62 forma450k')
s.wire((302.26,172.72),r['1']);s.dot((302.26,172.72))
r2=s.sym('150k','R62',(302.26,196.85),role='Serie com300k; resistoresBasic para reduzir taxa de montagem')
s.wire(r['2'],r2['1']);s.wire(r2['2'],(302.26,213.36))
r=s.sym('100k','R63',(302.26,228.6),role='Perna inferior do divisor; Vout0.6*(1+450k/100k)=3.3V')
s.wire((302.26,213.36),r['1']);s.wire(r['2'],(302.26,238.76));s.gnd((302.26,238.76))
c=s.sym('6p8','C70',(325.12,196.85),role='Feed-forward em paralelo comR61+R62; resposta de transientes')
s.wire((325.12,172.72),c['1']);s.wire(c['2'],(325.12,213.36),(302.26,213.36));s.dot((325.12,172.72));s.dot((302.26,213.36))
s.wire(u['5'],(269.24,175.26),(269.24,213.36),(302.26,213.36));s.label('FB_3V3',(276.86,213.36))
s.wire(u['2'],(256.54,207.01));s.gnd((256.54,207.01))
s.sym('TP','TP62',(388.62,160.02),value='+3V3',field=(383.54,149.86));s.wire((388.62,160.02),(388.62,172.72));s.dot((388.62,172.72))
s.sym('FLAG','#FLG063',(391.16,172.72));s.label('+3V3',(391.16,172.72),'hierarchical_label',180,'output')
s.sym('TP','TP63',(373.38,228.6),value='GND',field=(369.57,218.44));s.gnd((373.38,228.6));s.wire((373.38,228.6),(391.16,228.6));s.label('GND',(391.16,228.6),'hierarchical_label',180,'passive')
s.text('C68=10uF +30,5uF existentes =40,5uF; C69 DNP.\nAlvo3V3 <=1A; validar carga/termica. Sem LED permanente.\nNAO unir3V3 ao regulador3V3 da Nucleo; evitar retorno porUSB.',(220.98,246.38))
s.text('REFERENCIAS: Diodes AP63205 DS41326 Fig.21/Tab.3; TI TLV62569 SLVSDG1C sec.8.2/10.\nLAYOUT: malhas VIN-CIN-GND curtas; SW minimo; FB longe deSW; planoGND continuo.\nDetalhes, BOM/LCSC e limites: docs/alimentacao/README.md',(20.32,254),1.27)
changes[BASE/'alimentacao.kicad_sch']=s.render()

# Only modify the root power-sheet declaration and its existing rail annotations.
p=BASE/'esquematico_V4.kicad_sch';old=p.read_text(encoding='utf-8-sig');r=parse(old);reps=[]
for n in r.values:
    if not isinstance(n,Node):continue
    remove=False
    if n.tag=='sheet' and n.get('uuid')==SHEET:remove=True
    if n.tag in ['symbol','global_label','junction','wire']:
        pts=n.one('pts')
        coords=[tuple(map(float,a.values[1:3])) for a in pts.all('xy')] if pts else ([tuple(map(float,n.one('at').values[1:3]))] if n.one('at') else [])
        if coords and all(55<=x<=64 and 133<=y<=150 for x,y in coords):remove=True
    if remove:reps.append((n.start,n.end,''))
pins='\n'.join(f'(pin {q(net)} {typ} (at 55.88 {y} 0) {effects(1.27,"(justify right)")} (uuid {q(uid("root-pin-"+net))}))' for net,typ,y in [('+5V','output',138.43),('+3V3','output',143.51),('GND','passive',148.59)])
decl=f'''(sheet (at 30.48 134.62) (size 25.4 19.05) (exclude_from_sim no) (in_bom yes) (on_board yes) (dnp no)
(stroke (width 0.1524) (type solid)) (fill (color 0 0 0 0)) (uuid {q(SHEET)})
{prop('Sheetname','alimentacao',30.48,132.08,False,'(justify left)')}
{prop('Sheetfile','alimentacao.kicad_sch',30.48,156.21,False,'(justify left)')}
{pins}
(instances (project "esquematico_V4" (path {q('/'+ROOT)} (page "3")))))'''
for net,y in [('+5V',138.43),('+3V3',143.51),('GND',148.59)]:
    decl+=f'\n(wire (pts (xy 55.88 {y}) (xy 66.04 {y})) (stroke (width 0) (type default)) (uuid {q(uid("root-wire-"+net))}))\n(global_label {q(net)} (shape passive) (at 66.04 {y} 0) {effects(1.27,"(justify left)")} (uuid {q(uid("root-label-"+net))}))'
new=replace_spans(old,reps).rstrip();changes[p]=new[:-1]+decl+'\n)\n'
for table,typ,uri in [('sym-lib-table','KiCad','${KIPRJMOD}/libraries/Hardware_Power/Hardware_Power.kicad_sym'),('fp-lib-table','KiCad','${KIPRJMOD}/libraries/Hardware_Power/Hardware_Power.pretty')]:
    p=BASE/table;t=p.read_text(encoding='utf-8-sig').rstrip()
    if '(name "Hardware_Power")' not in t:changes[p]=t[:-1]+f'  (lib (name "Hardware_Power")(type "{typ}")(uri "{uri}")(options "")(descr "Alimentacao - importados EasyEDA, pinagem revisada"))\n)\n'

manifest=BASE/'docs/alimentacao/componentes_implementados.json'
changes[manifest]=json.dumps(s.components,ensure_ascii=False,indent=2)+'\n'
print('*** Begin Patch')
for p,new in changes.items():
    if p.exists():print(patch_file(p,p.read_text(encoding='utf-8-sig'),new),end='')
    else:print('*** Add File: '+p.as_posix()+'\n'+'\n'.join('+'+line for line in new.splitlines()))
print('*** End Patch')
