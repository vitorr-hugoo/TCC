"""Generate a reviewable apply_patch patch; never writes the design directly.

One-shot migration of the original KiCad 9 meteorological interface placeholders.
Use only on the pre-migration revision. Existing completed schematics are not
regenerated, so subsequent edits in KiCad remain authoritative.
"""
from pathlib import Path
import json
import sys
import uuid
from kicad_sexpr import Node, parse, replace_spans, patch_file

BASE = Path(__file__).resolve().parents[1]
STD = Path('C:/Program Files/KiCad/9.0/share/kicad/symbols')
ROOT = '0c305a6e-f9cd-49a1-b146-c2956e5469b6'
METEO = '058dce74-971b-49d7-82f0-384e20be985c'
MCU = 'a23dea8e-0c82-454e-9219-5e250a611b0f'
changes = {}


def q(x): return json.dumps(str(x), ensure_ascii=False)
def uid(key): return str(uuid.uuid5(uuid.NAMESPACE_URL, 'tcc-meteo-v2/' + key))
def fmt(v): return f'{v:.5f}'.rstrip('0').rstrip('.') if isinstance(v, float) else str(v)
def xy(x, y): return f'{fmt(x)} {fmt(y)}'
def eff(size=1.27, just=''): return f'(effects (font (size {size} {size})) {just})'
def prop(k,v,x=0,y=0,hide=True): return f'(property {q(k)} {q(v)} (at {xy(x,y)} 0) {eff(1.0, "(hide yes)" if hide else "")})'


def read(p): return p.read_text(encoding='utf-8-sig')
def change(p,new): changes[p] = new


def lib_symbol(lib, name):
    text = read(STD / (lib + '.kicad_sym'))
    root = parse(text)
    node = next(n for n in root.all('symbol') if n.values[1] == name)
    if node.one('extends'):
        parent = lib_symbol(lib, node.get('extends'))
        pr = parse(parent)
        replace = []
        for pp in pr.all('property'):
            nn = next((x for x in node.all('property') if x.values[1] == pp.values[1]), None)
            if nn: replace.append((pp.start,pp.end,text[nn.start:nn.end]))
        parent = replace_spans(parent,replace)
        return parent.replace(q(pr.values[1]),q(name),1).replace(q(pr.values[1]+'_'),q(name+'_'))
    return text[node.start:node.end]


LIB = {}
for lib,name in [('Device','R'),('Device','C'),('Device','D_Zener'),('Connector','TestPoint'),('74xGxx','74LVC1G17'),('Diode','BAT54S')]:
    LIB[lib+':'+name] = lib_symbol(lib,name)
# Keep the already used connector drawing and use it as the local authoritative symbol.
oldleaf = read(BASE/'pluviometro.kicad_sch')
jnode = parse(oldleaf).one('lib_symbols').all('symbol')[0]
jtext = oldleaf[jnode.start:jnode.end].replace('"Sensores_Meteo:PCB-6P6C-90"','"PCB-6P6C-90"',1)
LIB['Sensores_Meteo:PCB-6P6C-90'] = jtext
clamp = LIB['Diode:BAT54S'].replace('BAT54S','BAS70-04')
cr = parse(clamp)
reps=[]
for p in cr.all('property'):
    if p.values[1]=='Datasheet': reps.append((p.start,p.end,prop('Datasheet','https://assets.nexperia.com/documents/data-sheet/BAS70-04.pdf')))
    if p.values[1]=='Description': reps.append((p.start,p.end,prop('Description','Dual series Schottky: 1=A1, 2=K2, 3=K1/A2; SOT-23')))
clamp=replace_spans(clamp,reps)
LIB['Sensores_Meteo:BAS70-04']=clamp
change(BASE/'libraries/Sensores_Meteo/Sensores_Meteo.kicad_sym', '(kicad_symbol_lib (version 20250114) (generator "kicad_symbol_editor")\n'+jtext+'\n'+clamp+'\n)\n')

FP_R='Resistor_SMD:R_0603_1608Metric'
FP_C='Capacitor_SMD:C_0603_1608Metric'
PARTS={
 '10k':('Device:R',FP_R,'UNI-ROYAL','0603WAF1002T5E','C25804','1%; 0.1W; 75V'),
 '4k7':('Device:R',FP_R,'UNI-ROYAL','0603WAF4701T5E','C23162','1%; 0.1W; 75V'),
 '1k':('Device:R',FP_R,'UNI-ROYAL','0603WAF1001T5E','C21190','1%; 0.1W; 75V'),
 '100n':('Device:C',FP_C,'YAGEO','CC0603KRX7R9BB104','C14663','10%; 50V; X7R'),
 '10n':('Device:C',FP_C,'FH','0603B103K500NT','C57112','10%; 50V; X7R'),
 'ESD':('Device:D_Zener','Diode_SMD:D_SOD-882','Nexperia','PESD5V0S1UL,315','C85402','VRWM=5V; IR max 1uA at 5V/25C; see datasheet'),
 'CLAMP':('Sensores_Meteo:BAS70-04','Package_TO_SOT_SMD:SOT-23','Nexperia','BAS70-04,215','C389351','IR max 100nA at 50V/25C; 1=GND, 2=3V3, 3=signal'),
 'BUF':('74xGxx:74LVC1G17','Package_TO_SOT_SMD:SOT-23-5','Texas Instruments','SN74LVC1G17DBVR','C7836','Non-inverting Schmitt; 1=NC 2=A 3=GND 4=Y 5=VCC'),
 'J':('Sensores_Meteo:PCB-6P6C-90','Sensores_Meteo:RJ11-TH_PCB-6P6C-90','Ckmtw','R-RJ11R06P-A800','C189750','6P6C; p3=return; p4=signal; THT'),
 'TP':('Connector:TestPoint','TestPoint:TestPoint_Pad_D1.5mm','','','','Copper pad; no purchased component'),
}


class Sheet:
    def __init__(self, stem, sheet_uuid, path, title):
        self.stem,self.uuid,self.path,self.title=stem,sheet_uuid,path,title
        self.items=[]; self.libs=set();self.count=0
    def u(self):
        self.count+=1
        return uid(f'{self.stem}/{self.count}')
    def wire(self,a,b):
        if a==b:return
        self.items.append(f'(wire (pts (xy {xy(*a)}) (xy {xy(*b)})) (stroke (width 0) (type default)) (uuid "{self.u()}"))')
    def junction(self,x,y): self.items.append(f'(junction (at {xy(x,y)}) (diameter 0) (color 0 0 0 0) (uuid "{self.u()}"))')
    def label(self,name,x,y,kind='label',shape='passive',angle=0):
        shape_s=f'(shape {shape})' if kind!='label' else ''
        self.items.append(f'({kind} {q(name)} {shape_s} (at {xy(x,y)} {angle}) {eff(1.0, "(justify left)" if angle==0 else "(justify right)")} (uuid "{self.u()}"))')
    def gnd(self,x,y): self.label('GND',x,y,'global_label')
    def vcc(self,x,y): self.label('+3V3',x,y,'global_label')
    def text(self,t,x,y,size=1.27): self.items.append(f'(text {q(t)} (at {xy(x,y)} 0) {eff(size,"(justify left top)")} (uuid "{self.u()}"))')
    def nc(self,x,y): self.items.append(f'(no_connect (at {xy(x,y)}) (uuid "{self.u()}"))')
    def symbol(self,part,ref,x,y,ang=0,value=None,role='',old_uuid=None):
        lib,fp,mfr,mpn,lcsc,spec=PARTS[part]
        self.libs.add(lib)
        value=value or (part if part in ['10k','4k7','1k','100n','10n'] else mpn.split(',')[0])
        ds='https://www.lcsc.com/product-detail/'+lcsc+'.html' if lcsc else ''
        if part=='BUF':ds='https://www.ti.com/lit/ds/symlink/sn74lvc1g17.pdf'
        if part=='CLAMP':ds='https://assets.nexperia.com/documents/data-sheet/BAS70-04.pdf'
        if part=='ESD':ds='https://assets.nexperia.com/documents/data-sheet/PESD5V0S1UL.pdf'
        fields=[prop('Reference',ref,x+3.81,y-2.54,False),prop('Value',value,x+3.81,y,False)]
        if part in ['J','BUF','CLAMP'] or ang in [90,270]:
            fields=[prop('Reference',ref,x,y-10.16 if part=='J' else y-6.35,False),prop('Value',value,x,y-7.62 if part=='J' else y-3.81,False)]
        fields += [prop(k,v,x,y) for k,v in [('Footprint',fp),('Datasheet',ds),('Manufacturer',mfr),('MPN',mpn),('LCSC',lcsc),('LCSC Part',lcsc),('Rating',spec),('Function',role)]]
        self.items.append(f'(symbol (lib_id {q(lib)}) (at {xy(x,y)} {ang}) (unit 1) (exclude_from_sim no) (in_bom {"no" if part=="TP" else "yes"}) (on_board yes) (dnp no) (uuid "{old_uuid or uid(ref)}")\n'+ '\n'.join(fields)+f'\n(instances (project "esquematico_V4" (path "{self.path}" (reference "{ref}") (unit 1)))))')
    def tp(self,ref,name,x,y,netpoint=None):
        self.symbol('TP',ref,x,y,value=name,role=name)
        if netpoint:self.wire((x,y),netpoint)
    def connector(self,ref):
        old=parse(read(BASE/(self.stem+'.kicad_sch')))
        oldj=next(n for n in old.all('symbol') if n.prop('Reference')==ref)
        self.symbol('J',ref,35.56,81.28,role='RJ11 individual; contatos centrais',old_uuid=oldj.get('uuid'))
        for y in [74.93,77.47,85.09,87.63]:self.nc(45.72,y)
        self.wire((45.72,82.55),(50.8,82.55));self.wire((50.8,82.55),(50.8,116.84));self.gnd(50.8,116.84)
    def render(self):
        libs=[]
        for name in sorted(self.libs):
            txt=LIB[name]
            txt=txt.replace(q(parse(txt).values[1]),q(name),1)
            libs.append(txt)
        return f'(kicad_sch (version 20250114) (generator "eeschema") (generator_version "9.0") (uuid "{self.uuid}") (paper "A4")\n(title_block (title {q(self.title)}) (date "2026-09-16") (rev "METEO-1") (company "TCC - Estacao meteorologica"))\n(lib_symbols\n'+ '\n'.join(libs)+')\n'+ '\n'.join(self.items)+'\n(embedded_fonts no))\n'


for stem,j,base,signal,cap in [('pluviometro','J30',30,'RAIN_PULSE','100n'),('anemometro','J31',40,'WIND_SPEED_PULSE','10n')]:
    old=parse(read(BASE/(stem+'.kicad_sch')))
    sheetpath=next(n for n in old.all('symbol') if n.prop('Reference')==j).one('instances').one('project').one('path').values[1]
    s=Sheet(stem,old.get('uuid'),sheetpath,stem.upper()+' - protecao e condicionamento')
    s.text('MISOL: contato reed passivo; cabo externo ~2 m; entrada individual 6P6C p3/p4.',20.32,20.32)
    s.text('1  CONECTOR / ESD',20.32,38.1);s.text('2  FILTRO / CLAMP',91.44,38.1);s.text('3  SCHMITT / MCU',177.8,38.1)
    s.connector(j)
    s.wire((45.72,80.01),(66.04,80.01));s.wire((66.04,80.01),(86.36,80.01));s.wire((86.36,80.01),(97.79,80.01))
    s.label(stem.upper()+'_RAW',53.34,80.01)
    s.symbol('ESD','D'+str(base),66.04,99.06,90,role='ESD primario: K1 ao sinal, A2 a GND')
    s.wire((66.04,80.01),(66.04,95.25));s.wire((66.04,102.87),(66.04,111.76));s.gnd(66.04,111.76);s.junction(66.04,80.01)
    s.symbol('10k','R'+str(base),86.36,60.96,role='Pull-up ANTES do resistor serie; reed fechado ~0V')
    s.vcc(86.36,50.8);s.wire((86.36,50.8),(86.36,57.15));s.wire((86.36,64.77),(86.36,80.01));s.junction(86.36,80.01)
    s.symbol('4k7','R'+str(base+1),101.6,80.01,90,role='Limitacao de corrente e filtro RC')
    s.wire((105.41,80.01),(121.92,80.01));s.wire((121.92,80.01),(142.24,80.01));s.wire((142.24,80.01),(162.56,80.01));s.wire((162.56,80.01),(180.34,80.01))
    s.symbol(cap,'C'+str(base),121.92,99.06,role='Filtro do reed')
    s.wire((121.92,80.01),(121.92,95.25));s.wire((121.92,102.87),(121.92,111.76));s.gnd(121.92,111.76);s.junction(121.92,80.01)
    s.symbol('CLAMP','D'+str(base+1),142.24,58.42,role='Clamp secundario para GND e 3V3')
    s.wire((142.24,63.5),(142.24,80.01));s.wire((134.62,58.42),(129.54,58.42));s.gnd(129.54,58.42)
    s.wire((149.86,58.42),(157.48,58.42));s.vcc(157.48,58.42);s.junction(142.24,80.01)
    s.symbol('BUF','U'+str(base),193.04,80.01,role='Schmitt nao inversor; reed fecha -> borda de descida')
    s.wire((187.96,69.85),(187.96,60.96));s.vcc(187.96,60.96)
    s.wire((187.96,90.17),(187.96,111.76));s.gnd(187.96,111.76)
    s.symbol('100n','C'+str(base+1),220.98,60.96,role='Desacoplamento local junto a U'+str(base))
    s.vcc(220.98,50.8);s.wire((220.98,50.8),(220.98,57.15));s.wire((220.98,64.77),(220.98,69.85));s.gnd(220.98,69.85)
    s.wire((203.2,80.01),(223.52,80.01));s.wire((223.52,80.01),(251.46,80.01));s.label(signal,251.46,80.01,'hierarchical_label','output')
    s.tp('TP'+str(base),stem.upper()+'_RAW',66.04,69.85,(66.04,80.01))
    s.tp('TP'+str(base+1),stem.upper()+'_FILT',162.56,69.85,(162.56,80.01));s.junction(162.56,80.01)
    s.tp('TP'+str(base+2),signal,223.52,95.25,(223.52,80.01));s.junction(223.52,80.01)
    s.tp('TP'+str(base+3),'GND',81.28,111.76);s.gnd(81.28,111.76)
    s.text('Pull-up no lado do cabo evita divisor 4k7/10k na entrada Schmitt.\n'+('Tau abertura=1,47 ms; fechamento=0,47 ms. Debounce atual: 200 ms.' if cap=='100n' else 'Tau abertura=147 us; fechamento=47 us. Debounce atual: 20 ms limita f ~50 Hz.\nCom 2,4 km/h/Hz: 50 m/s exige 75 Hz; revisar debounce no firmware.'),20.32,134.62)
    s.text('ESD junto ao RJ11; clamp e C junto ao buffer. Plano GND continuo.\nProtecao transitoria a validar em bancada; sem isolamento e sem qualificacao contra raios.',20.32,157.48)
    change(BASE/(stem+'.kicad_sch'),s.render())

old=parse(read(BASE/'biruta.kicad_sch'))
sheetpath=next(n for n in old.all('symbol') if n.prop('Reference')=='J32').one('instances').one('project').one('path').values[1]
s=Sheet('biruta',old.get('uuid'),sheetpath,'BIRUTA - divisor ratiometrico e protecao ADC')
s.text('MISOL WH-SP-WD: rede resistiva passiva; 8 chaves / ate 16 codigos; cabo ~2 m.',20.32,20.32)
s.text('1  CONECTOR / DIVISOR',20.32,38.1);s.text('2  LIMITACAO / CLAMP',111.76,38.1);s.text('3  FILTRO / ADC',203.2,38.1)
s.connector('J32')
s.wire((45.72,80.01),(66.04,80.01));s.wire((66.04,80.01),(86.36,80.01));s.wire((86.36,80.01),(118.11,80.01))
s.label('WIND_DIR_RAW',53.34,80.01)
s.symbol('ESD','D50',66.04,99.06,90,role='Protecao primaria; inclui fuga na tolerancia do ADC')
s.wire((66.04,80.01),(66.04,95.25));s.wire((66.04,102.87),(66.04,111.76));s.gnd(66.04,111.76);s.junction(66.04,80.01)
s.symbol('10k','R50',86.36,60.96,role='Referencia do divisor; mesma fonte de VREF+ do ADC')
s.label('VREF_SENSOR',60.96,50.8,'hierarchical_label','input',180);s.wire((60.96,50.8),(86.36,50.8));s.wire((86.36,50.8),(86.36,57.15));s.wire((86.36,64.77),(86.36,80.01));s.junction(86.36,80.01)
s.symbol('10k','R51',121.92,80.01,90,role='Isola a protecao secundaria do divisor')
s.wire((125.73,80.01),(157.48,80.01));s.wire((157.48,80.01),(186.69,80.01))
s.symbol('CLAMP','D51',157.48,58.42,role='Clamp para 3V3 digital; nao injeta diretamente na referencia analogica')
s.wire((157.48,63.5),(157.48,80.01));s.wire((149.86,58.42),(144.78,58.42));s.gnd(144.78,58.42);s.wire((165.1,58.42),(172.72,58.42));s.vcc(172.72,58.42);s.junction(157.48,80.01)
s.symbol('1k','R52',190.5,80.01,90,role='Limita corrente residual no PA0 depois do clamp')
s.wire((194.31,80.01),(213.36,80.01));s.wire((213.36,80.01),(233.68,80.01));s.wire((233.68,80.01),(251.46,80.01))
s.symbol('10n','C50',213.36,99.06,role='Filtro / reservatorio de carga do ADC')
s.wire((213.36,80.01),(213.36,95.25));s.wire((213.36,102.87),(213.36,111.76));s.gnd(213.36,111.76);s.junction(213.36,80.01)
s.label('WIND_DIR_ADC',251.46,80.01,'hierarchical_label','passive')
s.tp('TP50','WIND_DIR_RAW',66.04,69.85,(66.04,80.01));s.tp('TP51','WIND_DIR_ADC',233.68,95.25,(233.68,80.01));s.junction(233.68,80.01)
s.tp('TP52','VREF_SENSOR',101.6,50.8,(86.36,50.8));s.junction(86.36,50.8)
s.tp('TP53','GND',81.28,111.76);s.gnd(81.28,111.76)
s.text('R50=10k 1%; ADC ideal=4095*R_sensor/(10000+R_sensor).\nR51/R52 estao APOS o divisor: nao entram na relacao DC ideal.\nRfonte maxima ~20,23k; tau maxima ~202 us; validar tempo ADC (144 ciclos).',20.32,134.62)
s.text('VREF_SENSOR vem de U5 CN10_7 (AVDD). Nao unir ao 3V3 digital por etiqueta.\nC50 junto ao PA0; retorno GND curto. Medir offsets por fuga e temperatura.\nA LUT ativa do firmware tem 16 entradas; 8 e o numero de amostras por leitura.',20.32,157.48)
change(BASE/'biruta.kicad_sch',s.render())

# Keep the integration sheet legible and make the ADC interface passive for ERC.
p=BASE/'meteorologia.kicad_sch';text=read(p)
text=text.replace('(shape output)\n\t\t(at 101.6 101.6','(shape passive)\n\t\t(at 101.6 101.6').replace('(pin "WIND_DIR_ADC" output','(pin "WIND_DIR_ADC" passive')
root=parse(text);reps=[]
for n in root.all('hierarchical_label'):
    # Right side output labels point INTO the wire, at 180 degrees.
    a=n.one('at');reps.append((a.start,a.end,f'(at {a.values[1]} {a.values[2]} 180)'))
n=next(n for n in root.all('sheet') if n.prop('Sheetname')=='BIRUTA')
insert=f'(pin "VREF_SENSOR" input (at 30.48 101.6 180) {eff()} (uuid "{uid("parent-vref-pin")}"))'
reps.append((n.end-1,n.end-1,insert))
text=replace_spans(text,reps)
extra=f'(hierarchical_label "VREF_SENSOR" (shape input) (at 15.24 101.6 0) {eff(1.0,"(justify right)")} (uuid "{uid("parent-vref-label")}"))\n(wire (pts (xy 15.24 101.6) (xy 30.48 101.6)) (stroke (width 0) (type default)) (uuid "{uid("parent-vref-wire")}"))'
text=text.rstrip()[:-1]+extra+'\n)\n';change(p,text)

# MCU: three actual hierarchical inputs; AVDD is an exported board reference.
p=BASE/'mcu.kicad_sch';text=read(p);root=parse(text);reps=[]
names={'PA0':'WIND_DIR_ADC','PA1':'RAIN_PULSE','PC7':'WIND_SPEED_PULSE'}
for n in root.all('global_label'):
    if n.values[1] in names:
        a=n.one('at').values[1:]; angle='0' if a[2]=='180' else '180'
        reps.append((n.start,n.end,f'(hierarchical_label {q(names[n.values[1]])} (shape input) (at {a[0]} {a[1]} {angle}) {eff(1.0,"(justify right)" if angle=="0" else "(justify left)")} (uuid {q(n.get("uuid"))}))'))
for n in root.all('no_connect'):
    if tuple(n.one('at').values[1:]) in [('186.69','50.8'),('186.69','121.92')]:reps.append((n.start,n.end,''))
text=replace_spans(text,reps)
extra=f'''(wire (pts (xy 186.69 50.8) (xy 210.82 50.8)) (stroke (width 0) (type default)) (uuid "{uid('avdd-wire')}"))
(hierarchical_label "VREF_SENSOR" (shape output) (at 210.82 50.8 180) {eff(1.0,'(justify left)')} (uuid "{uid('avdd-label')}"))
(wire (pts (xy 186.69 121.92) (xy 201.93 121.92)) (stroke (width 0) (type default)) (uuid "{uid('agnd-wire')}"))
(global_label "GND" (shape passive) (at 201.93 121.92 0) {eff(1.0,'(justify left)')} (uuid "{uid('agnd-label')}"))
(text "METEO: PA1=chuva; PC7=velocidade; PA0=ADC.\nVREF_SENSOR: AVDD/VREF+ da Nucleo.\nFutura MCU discreta: ligar a mesma fonte da referencia ADC." (at 45.72 152.4 0) {eff(1.27,'(justify left top)')} (uuid "{uid('mcu-note')}"))'''
# AVDD is a supply output of the board module, not a supply input of the MCU die.
text=text.rstrip()[:-1]+extra+'\n)\n'
root=parse(text);lib=root.one('lib_symbols').all('symbol')[0]
for unit in lib.all('symbol'):
    for pin in unit.all('pin'):
        if pin.get('number')=='CN10_7':
            chunk=text[pin.start:pin.end].replace('(pin power_in','(pin power_out',1)
            text=replace_spans(text,[(pin.start,pin.end,chunk)])
            break
change(p,text)

# Root: retire the superseded sensor circuits and duplicate Nucleo drawing.
p=BASE/'esquematico_V4.kicad_sch';text=read(p);root=parse(text);reps=[]
removed={'J14','J15','J16','J23','J24','J25','R8','R10','R11','C4','C5','U7','#PWR05','#PWR06','#PWR09','#PWR010','#PWR014','#PWR015'}
def in_old_area(x,y):
    return (85<=x<=160 and 130<=y<=199) or (188<=x<=277 and 130<=y<=175) or y<0
for n in root.values:
    if not isinstance(n,Node):continue
    remove=n.tag=='symbol' and n.prop('Reference') in removed
    if n.tag in ('wire','junction','no_connect','global_label','label','text'):
        pts=n.one('pts')
        if pts: coords=[tuple(map(float,a.values[1:3])) for a in pts.all('xy')]
        elif n.one('at'):coords=[tuple(map(float,n.one('at').values[1:3]))]
        else:coords=[]
        if coords and all(in_old_area(*a) for a in coords):remove=True
    if remove:reps.append((n.start,n.end,''))
    if n.tag=='sheet' and n.prop('Sheetname')=='METEOROLOGIA':
        seg=text[n.start:n.end].replace('Sensores_meteorologicos/meteorologia.kicad_sch','meteorologia.kicad_sch').replace('(pin "WIND_DIR_ADC" output','(pin "WIND_DIR_ADC" passive')
        seg=seg[:-1]+f'(pin "VREF_SENSOR" input (at 139.7 80.01 180) {eff()} (uuid "{uid("root-meteo-vref")}")))'
        reps.append((n.start,n.end,seg))
    if n.tag=='sheet' and n.prop('Sheetname')=='mcu':
        seg=text[n.start:n.end]
        pins=''.join(f'(pin {q(name)} input (at 133.35 {y} 0) {eff()} (uuid "{uid(name+"-mcu-pin")}"))' for name,y in [('RAIN_PULSE',30.48),('WIND_SPEED_PULSE',35.56),('WIND_DIR_ADC',40.64)])
        pins+=f'(pin "VREF_SENSOR" output (at 121.92 25.4 90) {eff()} (uuid "{uid("mcu-vref-pin")}"))'
        reps.append((n.start,n.end,seg[:-1]+pins+')'))
text=replace_spans(text,reps)
extra=[]
for name,y in [('RAIN_PULSE',30.48),('WIND_SPEED_PULSE',35.56),('WIND_DIR_ADC',40.64)]:
    extra += [f'(wire (pts (xy 133.35 {y}) (xy 172.72 {y})) (stroke (width 0) (type default)) (uuid "{uid(name+"-root-wire")}"))',f'(global_label {q(name)} (shape passive) (at 172.72 {y} 0) {eff(1.0,"(justify left)")} (uuid "{uid(name+"-root-label")}"))']
extra += [f'(wire (pts (xy 121.92 25.4) (xy 121.92 15.24)) (stroke (width 0) (type default)) (uuid "{uid("root-vref-wire")}"))',f'(global_label "VREF_SENSOR" (shape passive) (at 121.92 15.24 90) {eff(1.0,"(justify left)")} (uuid "{uid("root-vref-label")}"))',f'(wire (pts (xy 139.7 80.01) (xy 124.46 80.01)) (stroke (width 0) (type default)) (uuid "{uid("root-vref-wire2")}"))',f'(global_label "VREF_SENSOR" (shape passive) (at 124.46 80.01 180) {eff(1.0,"(justify right)")} (uuid "{uid("root-vref-label2")}"))']
text=text.rstrip()[:-1]+'\n'.join(extra)+'\n)\n';change(p,text)

# Normalize ground naming only in the active hierarchy; leave archived copies intact.
for p in BASE.glob('*.kicad_sch'):
    text=changes.get(p,read(p))
    text=text.replace('"Gnd"','"GND"')
    if text!=read(p):change(p,text)

# Output patch for explicit apply_patch application.
print('*** Begin Patch')
for p,new in changes.items():
    if p.exists():sys.stdout.write(patch_file(p,read(p),new))
    else:sys.stdout.write(f'*** Add File: {p.as_posix()}\n'+'\n'.join('+'+line for line in new.splitlines())+'\n')
print('*** End Patch')
