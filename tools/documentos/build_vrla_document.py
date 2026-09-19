from pathlib import Path
from math import cos, sin, atan2, pi

from PIL import Image, ImageDraw, ImageFont
from docx import Document
from docx.shared import Inches, Pt, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH, WD_BREAK
from docx.enum.section import WD_SECTION
from docx.enum.table import WD_TABLE_ALIGNMENT, WD_CELL_VERTICAL_ALIGNMENT
from docx.oxml import OxmlElement
from docx.oxml.ns import qn


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "docs" / "tcc" / "fontes" / "Projeto_alimentacao_VRLA_TCC.docx"
ASSETS = ROOT / "docs" / "tcc" / "assets" / "alimentacao-vrla"
ASSETS.mkdir(parents=True, exist_ok=True)

NAVY = "#18324B"
BLUE = "#2C5F85"
LIGHT_BLUE = "#EAF2F8"
PALE = "#F4F7F9"
GREY = "#66727E"
GRID = "#D9D9D9"
RED = "#8E3028"
GREEN = "#456E4E"
FONT = r"C:\Windows\Fonts\arial.ttf"
FONT_BOLD = r"C:\Windows\Fonts\arialbd.ttf"


def font(size, bold=False):
    return ImageFont.truetype(FONT_BOLD if bold else FONT, size)


def pil_color(value):
    if isinstance(value, str) and len(value) == 6 and all(ch in "0123456789abcdefABCDEF" for ch in value):
        return "#" + value
    return value


class SafeDraw:
    """Accept Office-style RRGGBB colors as well as Pillow color names."""
    def __init__(self, draw):
        self._draw = draw

    def __getattr__(self, name):
        member = getattr(self._draw, name)
        if not callable(member):
            return member
        def wrapped(*args, **kwargs):
            for key in ("fill", "outline"):
                if key in kwargs:
                    kwargs[key] = pil_color(kwargs[key])
            return member(*args, **kwargs)
        return wrapped


def wrap(draw, text, fnt, max_width):
    words = str(text).split()
    lines, line = [], ""
    for word in words:
        candidate = (line + " " + word).strip()
        if draw.textbbox((0, 0), candidate, font=fnt)[2] <= max_width or not line:
            line = candidate
        else:
            lines.append(line)
            line = word
    if line:
        lines.append(line)
    return lines


def multiline_center(draw, box, text, fnt, fill="black", spacing=8):
    x1, y1, x2, y2 = box
    lines = []
    for part in str(text).split("\n"):
        lines += wrap(draw, part, fnt, x2 - x1 - 32)
    heights = [draw.textbbox((0, 0), line, font=fnt)[3] for line in lines]
    total = sum(heights) + spacing * (len(lines) - 1)
    y = y1 + (y2 - y1 - total) / 2
    for line, h in zip(lines, heights):
        bb = draw.textbbox((0, 0), line, font=fnt)
        w = bb[2] - bb[0]
        draw.text((x1 + (x2 - x1 - w) / 2, y), line, font=fnt, fill=fill)
        y += h + spacing


def box(draw, xy, title, subtitle=None, fill=LIGHT_BLUE, border=BLUE, title_fill=NAVY):
    x1, y1, x2, y2 = xy
    draw.rounded_rectangle(xy, radius=28, fill=fill, outline=border, width=5)
    title_h = 58 if subtitle else 0
    if title_h:
        draw.rounded_rectangle((x1, y1, x2, y1 + title_h), radius=28, fill=title_fill)
        draw.rectangle((x1, y1 + 28, x2, y1 + title_h), fill=title_fill)
        multiline_center(draw, (x1 + 8, y1 + 5, x2 - 8, y1 + title_h - 3), title, font(29, True), "white", 3)
    else:
        multiline_center(draw, (x1 + 8, y1 + 6, x2 - 8, y2 - 6), title, font(28, True), NAVY, 3)
    if subtitle:
        multiline_center(draw, (x1 + 12, y1 + title_h + 7, x2 - 12, y2 - 8), subtitle, font(25), "black", 5)


def arrow(draw, start, end, color=NAVY, width=7, label=None, label_offset=-34):
    x1, y1 = start
    x2, y2 = end
    draw.line((x1, y1, x2, y2), fill=color, width=width)
    angle = atan2(y2 - y1, x2 - x1)
    length = 24
    a1, a2 = angle + 2.55, angle - 2.55
    draw.polygon([(x2, y2), (x2 + length * cos(a1), y2 + length * sin(a1)), (x2 + length * cos(a2), y2 + length * sin(a2))], fill=color)
    if label:
        mx, my = (x1 + x2) / 2, (y1 + y2) / 2
        bb = draw.textbbox((0, 0), label, font=font(24, True))
        draw.rectangle((mx - (bb[2]-bb[0])/2 - 8, my + label_offset - 4, mx + (bb[2]-bb[0])/2 + 8, my + label_offset + bb[3] + 4), fill="white")
        draw.text((mx - (bb[2]-bb[0])/2, my + label_offset), label, font=font(24, True), fill=color)


def line_label(draw, x, y, label, color=NAVY, anchor="la"):
    draw.text((x, y), label, font=font(23, True), fill=color, anchor=anchor)


def dot(draw, xy, r=8, fill=NAVY):
    x, y = xy
    draw.ellipse((x-r, y-r, x+r, y+r), fill=fill)


def title_image(path, title, subtitle, size=(2400, 1500)):
    img = Image.new("RGB", size, "white")
    d = SafeDraw(ImageDraw.Draw(img))
    d.text((70, 42), title, font=font(46, True), fill=NAVY)
    d.text((70, 108), subtitle, font=font(27), fill=GREY)
    return img, d


def save_architecture():
    path = ASSETS / "01_arquitetura_vrla.png"
    img, d = title_image(path, "Arquitetura de energia com bateria VRLA AGM", "Todos os conectores, fusiveis e referencias de terra pertencem a mesma placa. A rede CA fica fora dela.", (2600, 1320))
    box(d, (90, 310, 440, 550), "Painel solar", "Painel nominal 12 V\nVMP tipico 17 a 18 V\nVOC deve respeitar U1", "EAF5EB", GREEN, GREEN)
    box(d, (90, 765, 440, 1005), "Fonte externa", "15 a 18 Vcc certificada\npara alimentar e carregar\n12 Vcc: somente sistema", "FFF4E5", "A86816", "A86816")
    box(d, (590, 500, 950, 760), "Entrada protegida", "Fusiveis, anti-inversao e OR\nGera VIN_CHG\nSem ligar painel direto em VSYS", PALE, BLUE, NAVY)
    box(d, (1110, 500, 1510, 760), "U1 BQ24650", "Carregador buck solar\nMPPT, CC/CV e compensacao\npor temperatura", "EAF2F8", BLUE, NAVY)
    box(d, (1670, 450, 2070, 810), "Bateria VRLA AGM", "12 V, 6 celulas\nExemplo de projeto: 7 Ah\nFusivel junto ao terminal +", "F8E9E8", RED, RED)
    box(d, (2200, 250, 2520, 510), "Corte LVD", "U2 TPS3700\nQ3 P-MOS\nBAT_FUSED -> VSYS", PALE, BLUE, NAVY)
    box(d, (2200, 620, 2520, 875), "Buck 5 V", "U3 TPS62160\n1 A, 17 uA tip.", PALE, BLUE, NAVY)
    box(d, (2200, 980, 2520, 1225), "LDO 3,3 V", "U4 MCP1700\n1,6 uA tip.", PALE, BLUE, NAVY)
    arrow(d, (440, 430), (590, 590), GREEN, label="PV_PROT")
    arrow(d, (440, 885), (590, 670), "A86816", label="EXT_PROT")
    arrow(d, (950, 630), (1110, 630), label="VIN_CHG")
    arrow(d, (1510, 630), (1670, 630), RED, label="BAT+")
    arrow(d, (2070, 630), (2200, 380), RED, label="BAT_FUSED")
    arrow(d, (2360, 510), (2360, 620), label="VSYS")
    arrow(d, (2360, 875), (2360, 980), label="+5V")
    d.text((103, 1110), "Regra do projeto: a bateria fica sombreada e ventilada; somente o painel deve ficar exposto ao sol.", font=font(31, True), fill=RED)
    img.save(path, dpi=(300, 300))
    return path


def save_input():
    path = ASSETS / "02_entradas_or.png"
    img, d = title_image(path, "Entradas PV e fonte externa", "Esquematico funcional com protecao contra polaridade invertida e isolamento entre fontes.", (2600, 1410))
    # PV chain
    box(d, (100, 280, 370, 500), "J1 PV", "+ PV\n- GND", "F2F8FA", BLUE, NAVY)
    arrow(d, (370, 390), (500, 390), label="PV+")
    d.rectangle((500, 340, 630, 440), outline=NAVY, width=5)
    multiline_center(d, (505, 345, 625, 435), "F1\n2 A ref.", font(24, True), NAVY, 3)
    arrow(d, (630, 390), (765, 390))
    d.rounded_rectangle((765, 315, 1035, 465), radius=18, fill="EAF2F8", outline=BLUE, width=5)
    multiline_center(d, (780, 325, 1020, 455), "Q1 P-MOS\nanti-inversao\nAO3401A ou eq.", font(24, True), NAVY, 4)
    d.line((860, 465, 860, 545), fill=NAVY, width=4)
    d.line((820, 545, 900, 545), fill=NAVY, width=4)
    d.text((910, 525), "RGS + zener VGS\n(conforme VOC)", font=font(20), fill=GREY)
    arrow(d, (1035, 390), (1200, 390), label="PV_PROT")
    d.line((1125, 390, 1125, 555), fill=NAVY, width=4); d.line((1075, 555, 1175, 555), fill=NAVY, width=4); d.text((1000, 575), "CIN_PV 10 uF + 100 nF", font=font(21), fill=GREY)
    # ext chain
    box(d, (100, 815, 370, 1035), "J2 EXT", "+ 15 a 18 Vcc\n- GND", "FFF4E5", "A86816", "A86816")
    arrow(d, (370, 925), (500, 925), "A86816", label="EXT+")
    d.rectangle((500, 875, 630, 975), outline=NAVY, width=5)
    multiline_center(d, (505, 880, 625, 970), "F2\n2 A ref.", font(24, True), NAVY, 3)
    arrow(d, (630, 925), (765, 925), "A86816")
    d.rounded_rectangle((765, 850, 1035, 1000), radius=18, fill="FFF4E5", outline="A86816", width=5)
    multiline_center(d, (780, 860, 1020, 990), "Q2 P-MOS\nanti-inversao\nAO3401A ou eq.", font(24, True), "7A4E10", 4)
    d.line((860, 1000, 860, 1080), fill=NAVY, width=4); d.line((820, 1080, 900, 1080), fill=NAVY, width=4)
    arrow(d, (1035, 925), (1200, 925), "A86816", label="EXT_PROT")
    d.line((1125, 925, 1125, 1090), fill=NAVY, width=4); d.line((1075, 1090, 1175, 1090), fill=NAVY, width=4); d.text((990, 1110), "CIN_EXT 10 uF + 100 nF", font=font(21), fill=GREY)
    # ideal or
    d.rounded_rectangle((1290, 420, 1640, 875), radius=25, fill="F4F7F9", outline=NAVY, width=5)
    multiline_center(d, (1320, 440, 1610, 535), "OR de fontes", font(31, True), NAVY, 4)
    d.text((1330, 570), "Opcao de menor perda:", font=font(23, True), fill=NAVY)
    d.text((1330, 610), "dois P-MOS como diodos ideais", font=font(22), fill="black")
    d.text((1330, 650), "com seus circuitos de gate.", font=font(22), fill="black")
    d.text((1330, 720), "Protótipo simples:", font=font(23, True), fill=NAVY)
    d.text((1330, 760), "dois Schottky. Aceita perda", font=font(22), fill="black")
    d.text((1330, 800), "e reduz margem de carregamento.", font=font(22), fill="black")
    arrow(d, (1200, 390), (1290, 530), label="PV_PROT")
    arrow(d, (1200, 925), (1290, 765), label="EXT_PROT")
    arrow(d, (1640, 650), (1990, 650), label="VIN_CHG")
    box(d, (1990, 510, 2470, 790), "Para U1 BQ24650", "CIN 2 x 10 uF X7R\nmais 100 nF junto ao CI\nSelecionar painel com VOC abaixo de 28 V", "EAF2F8", BLUE, NAVY)
    d.text((98, 1228), "IMPORTANTE: J2 de 12 V pode ser ligado a um OR para VSYS e alimentar a carga, mas NAO deve ser usado para carregar a VRLA de 12 V.", font=font(27, True), fill=RED)
    d.text((98, 1275), "Uma VRLA 12 V precisa de tensao de carga superior a 12 V. Para carga, especifique uma fonte externa de 15 a 18 Vcc ou use o painel.", font=font(27), fill=RED)
    img.save(path, dpi=(300, 300))
    return path


def save_charger():
    path = ASSETS / "03_carregador_bq24650.png"
    img, d = title_image(path, "Carregador solar discreto com BQ24650", "Diagrama de conexoes. Os valores do divisor VFB e da compensacao termica sao definidos para a bateria VRLA escolhida.", (2600, 1800))
    # Central IC
    d.rounded_rectangle((850, 390, 1450, 1210), radius=28, fill="EAF2F8", outline=NAVY, width=6)
    multiline_center(d, (890, 420, 1410, 540), "U1  BQ24650", font(42, True), NAVY, 6)
    d.text((930, 600), "VCC     REGN", font=font(26), fill=NAVY)
    d.text((930, 670), "HIDRV   BTST", font=font(26), fill=NAVY)
    d.text((930, 740), "LODRV   PH", font=font(26), fill=NAVY)
    d.text((930, 810), "SRP     SRN", font=font(26), fill=NAVY)
    d.text((930, 880), "VFB     MPPSET", font=font(26), fill=NAVY)
    d.text((930, 950), "ISET    TS", font=font(26), fill=NAVY)
    d.text((930, 1020), "STAT2   CE", font=font(26), fill=NAVY)
    d.text((930, 1090), "GND     VREF", font=font(26), fill=NAVY)
    # vin left
    box(d, (80, 445, 440, 725), "VIN_CHG", "Do OR de fontes\nPainel 18 V ou\nfonte 15 a 18 V", "F2F8FA", BLUE, NAVY)
    arrow(d, (440, 585), (620, 585), label="VIN_CHG")
    d.line((620, 585, 790, 585), fill=NAVY, width=7)
    dot(d, (720, 585)); d.line((720, 585, 720, 770), fill=NAVY, width=4); d.line((675, 770, 765, 770), fill=NAVY, width=4)
    d.text((535, 795), "CIN: 2 x 10 uF X7R\n+ 100 nF", font=font(22), fill=GREY)
    d.line((790, 585, 850, 585), fill=NAVY, width=7); d.text((620, 545), "VCC", font=font(24, True), fill=NAVY)
    # halfbridge right
    d.rounded_rectangle((1600, 330, 1980, 1040), radius=20, fill="F8FBFD", outline=BLUE, width=5)
    d.text((1710, 360), "Estagio buck sincrono", font=font(28, True), fill=NAVY)
    d.rectangle((1715, 465, 1865, 610), outline=NAVY, width=5); multiline_center(d, (1725, 470, 1855, 603), "QH\nN-MOS 30 V", font(22, True), NAVY, 3)
    d.rectangle((1715, 770, 1865, 915), outline=NAVY, width=5); multiline_center(d, (1725, 775, 1855, 908), "QL\nN-MOS 30 V", font(22, True), NAVY, 3)
    d.line((1790, 420, 1790, 465), fill=NAVY, width=7); d.text((1810, 415), "VIN_CHG", font=font(21, True), fill=NAVY)
    d.line((1790, 610, 1790, 770), fill=NAVY, width=7); dot(d, (1790, 690)); d.text((1810, 670), "PH", font=font(21, True), fill=NAVY)
    d.line((1790, 915, 1790, 985), fill=NAVY, width=7); d.line((1740, 985, 1840, 985), fill=NAVY, width=4)
    d.line((1450, 690, 1650, 690), fill=NAVY, width=5); d.text((1480, 650), "HIDRV / LODRV", font=font(21, True), fill=NAVY)
    arrow(d, (1790, 690), (2070, 690), label="PH")
    # Inductor / sense / battery
    d.rounded_rectangle((2070, 620, 2260, 760), radius=12, fill="FFFBEA", outline="A86816", width=5)
    multiline_center(d, (2080, 630, 2250, 750), "L1\n10 uH\nIsat >= 1,5 A", font(22, True), "7A4E10", 3)
    arrow(d, (2260, 690), (2350, 690))
    d.rectangle((2350, 625, 2480, 755), outline=RED, width=5)
    multiline_center(d, (2360, 635, 2470, 745), "RSR\n0,056 ohm\n1%", font(22, True), RED, 3)
    arrow(d, (2480, 690), (2570, 690), RED, label="BAT+")
    d.line((2390, 755, 2390, 850), fill=NAVY, width=4); d.line((2425, 755, 2425, 850), fill=NAVY, width=4)
    d.line((1450, 820, 2390, 820), fill=NAVY, width=4); d.text((1690, 790), "SRP/SRN em Kelvin sobre RSR", font=font(23, True), fill=NAVY)
    d.line((2510, 690, 2510, 920), fill=NAVY, width=4); d.line((2460, 920, 2560, 920), fill=NAVY, width=4); d.text((2270, 945), "CBAT: 100 uF baixo ESR + 1 uF", font=font(22), fill=GREY)
    # MPP and ISET below left
    d.rounded_rectangle((85, 1000, 600, 1445), radius=20, fill="F4F7F9", outline=BLUE, width=5)
    d.text((120, 1030), "Ajustes obrigatorios de U1", font=font(28, True), fill=NAVY)
    d.text((125, 1090), "MPPSET: divisor VIN_CHG -> GND", font=font(22), fill="black")
    d.text((125, 1130), "Define tensao de rastreio do painel.", font=font(22), fill="black")
    d.text((125, 1192), "ISET: resistor para GND.", font=font(22), fill="black")
    d.text((125, 1232), "Com RSR 0,056 ohm: cerca de 0,7 A", font=font(22), fill="black")
    d.text((125, 1272), "se a regulacao de 40 mV do CI for usada.", font=font(22), fill="black")
    d.text((125, 1334), "CE: sinal CHG_EN. Deve desligar", font=font(22), fill="black")
    d.text((125, 1374), "a carga se bateria estiver muito quente.", font=font(22), fill="black")
    # feedback lower center
    d.rounded_rectangle((710, 1290, 1490, 1690), radius=20, fill="FFF8E9", outline="A86816", width=5)
    d.text((745, 1320), "VFB e compensacao termica na bateria", font=font(28, True), fill="7A4E10")
    d.line((820, 1450, 1380, 1450), fill=NAVY, width=5)
    d.text((760, 1410), "BAT+", font=font(23, True), fill=RED)
    d.rectangle((900, 1400, 1050, 1500), outline=NAVY, width=4); multiline_center(d, (905, 1405, 1045, 1495), "RT + NTC\n100 k\nna bateria", font(20, True), NAVY, 2)
    d.line((1050, 1450, 1200, 1450), fill=NAVY, width=5); dot(d, (1200, 1450)); d.text((1220, 1410), "VFB", font=font(23, True), fill=NAVY)
    d.line((1200, 1450, 1200, 1570), fill=NAVY, width=4)
    d.rectangle((1130, 1570, 1270, 1650), outline=NAVY, width=4); multiline_center(d, (1135, 1575, 1265, 1645), "RB\npara GND", font(19, True), NAVY, 2)
    d.line((1200, 1650, 1200, 1680), fill=NAVY, width=4); d.line((1150, 1680, 1250, 1680), fill=NAVY, width=4)
    d.text((730, 1530), "Q4 2N7002 acionado por STAT2 altera o divisor", font=font(21), fill="black")
    d.text((730, 1570), "entre V_BULK e V_RECH/FLOAT, conforme R2.", font=font(21), fill="black")
    # TS note
    d.rounded_rectangle((1600, 1190, 2450, 1580), radius=20, fill="F8E9E8", outline=RED, width=5)
    d.text((1640, 1220), "Ponto critico de seguranca", font=font(29, True), fill=RED)
    d.text((1640, 1280), "A nota da TI usa TS com divisor fixo e leva", font=font(23), fill="black")
    d.text((1640, 1322), "a compensacao para VFB. Para ambiente externo,", font=font(23), fill="black")
    d.text((1640, 1364), "acrescente supervisor de temperatura que force", font=font(23), fill="black")
    d.text((1640, 1406), "CE baixo acima do limite da bateria. O NTC deve", font=font(23), fill="black")
    d.text((1640, 1448), "ficar fisicamente colado a lateral da VRLA, longe", font=font(23), fill="black")
    d.text((1640, 1490), "dos terminais. Use o perfil do fabricante da bateria.", font=font(23), fill="black")
    d.text((80, 1730), "Figura baseada na topologia de TI SLVA437A, redesenhada para legibilidade. Valores de VFB e NTC devem ser recalculados e validados para o modelo final de bateria.", font=font(25, True), fill=RED)
    img.save(path, dpi=(300, 300))
    return path


def save_lvd():
    path = ASSETS / "04_lvd_vrla.png"
    img, d = title_image(path, "Fusivel da bateria e corte por subtensao LVD", "O TPS3700 fica antes do corte, portanto continua medindo BAT_FUSED quando a carga esta desligada.", (2600, 1360))
    box(d, (80, 390, 390, 690), "J3 BATERIA", "+ VRLA 12 V\n- GND\nCabos curtos", "F8E9E8", RED, RED)
    arrow(d, (390, 540), (540, 540), RED, label="BAT+")
    d.rectangle((540, 465, 720, 615), outline=RED, width=5); multiline_center(d, (550, 475, 710, 605), "F3\n2 A lento ref.\nmontar junto ao +", font(24, True), RED, 3)
    arrow(d, (720, 540), (900, 540), RED, label="BAT_FUSED")
    d.rounded_rectangle((900, 425, 1210, 655), radius=20, fill="F8FBFD", outline=BLUE, width=5)
    multiline_center(d, (920, 445, 1190, 635), "Q3 P-MOS\nchave de alta\nSource BAT_FUSED\nDrain VSYS", font(26, True), NAVY, 5)
    arrow(d, (1210, 540), (1510, 540), label="VSYS 10,8 a 14,7 V")
    d.line((1035, 655, 1035, 800), fill=NAVY, width=5); d.text((1055, 770), "Gate", font=font(23, True), fill=NAVY)
    # gate drive
    d.rounded_rectangle((810, 845, 1250, 1130), radius=20, fill="EAF2F8", outline=BLUE, width=5)
    d.text((850, 875), "Driver de gate", font=font(28, True), fill=NAVY)
    d.text((850, 930), "RGS 1 Mohm: gate -> source (OFF)", font=font(21), fill="black")
    d.text((850, 975), "Q4 2N7002: puxa gate a GND (ON)", font=font(21), fill="black")
    d.text((850, 1020), "RG 100 ohm: limita pico de gate", font=font(21), fill="black")
    d.text((850, 1065), "Zener 12 V entre G-S se VGS exigir", font=font(21), fill="black")
    d.line((1035, 800, 1035, 845), fill=NAVY, width=5)
    # supervisor
    d.rounded_rectangle((1450, 735, 1930, 1150), radius=22, fill="FFF8E9", outline="A86816", width=5)
    d.text((1490, 765), "U2 TPS3700", font=font(32, True), fill="7A4E10")
    d.text((1490, 825), "Alimentacao: BAT_FUSED", font=font(22), fill="black")
    d.text((1490, 870), "Consumo tipico: 5,5 uA", font=font(22), fill="black")
    d.text((1490, 930), "INA+ recebe divisor R1/R2", font=font(22), fill="black")
    d.text((1490, 970), "com VREF interno de 400 mV.", font=font(22), fill="black")
    d.text((1490, 1030), "OUTA e drenagem aberta. Em", font=font(22), fill="black")
    d.text((1490, 1070), "subtensao, puxa gate de Q4 baixo", font=font(22), fill="black")
    d.text((1490, 1110), "e Q3 desliga VSYS.", font=font(22), fill="black")
    # divider
    d.line((1640, 540, 1640, 735), fill=NAVY, width=5); d.text((1660, 585), "BAT_FUSED", font=font(21, True), fill=RED)
    d.rectangle((1570, 560, 1710, 650), outline=NAVY, width=4); multiline_center(d, (1575, 565, 1705, 645), "R1\n2,74 Mohm\n1%", font(20, True), NAVY, 2)
    d.line((1640, 650, 1640, 690), fill=NAVY, width=4); dot(d, (1640, 690)); d.text((1660, 675), "SENSE", font=font(21, True), fill=NAVY)
    d.line((1640, 690, 1640, 735), fill=NAVY, width=4)
    d.rectangle((1570, 1160, 1710, 1250), outline=NAVY, width=4); multiline_center(d, (1575, 1165, 1705, 1245), "R2\n100 kohm\n1%", font(20, True), NAVY, 2)
    d.line((1640, 1150, 1640, 1160), fill=NAVY, width=4); d.line((1640, 1250, 1640, 1280), fill=NAVY, width=4); d.line((1590, 1280, 1690, 1280), fill=NAVY, width=4)
    # control relation
    arrow(d, (1450, 1050), (1250, 1020), label="OUTA")
    # notes right
    box(d, (2050, 380, 2520, 825), "Ajuste de corte", "R1/R2 acima da referencia 400 mV gera limiar aproximado de 11,36 V. Adicionar R_HYS ou latch para obter recuperacao entre 12,0 e 12,4 V. Validar sob carga real.", "F4F7F9", BLUE, NAVY)
    box(d, (2050, 900, 2520, 1235), "Protecao adicional", "Use o segundo canal de U2 para alarme de bateria baixa ou para CHG_EN. Acrescente termostato/NTC que leve CE a nivel baixo se a bateria exceder temperatura definida pelo fabricante.", "F8E9E8", RED, RED)
    d.text((86, 1240), "O fusivel e indispensavel: uma VRLA de 7 Ah pode fornecer centenas de amperes em curto.", font=font(25, True), fill=RED)
    d.text((86, 1283), "Escolha o valor final pelo consumo maximo, bitola do cabo e capacidade do conector; 2 A e apenas referencia de baixa potencia.", font=font(24, True), fill=RED)
    img.save(path, dpi=(300, 300))
    return path


def save_regulators():
    path = ASSETS / "05_5v_3v3.png"
    img, d = title_image(path, "Conversao de VSYS para 5 V e 3,3 V", "Circuitos discretos de baixo consumo para a carga eletrônica. Confirmar o consumo maximo do LoRa, SD e MCU antes de fechar a BOM.", (2600, 1540))
    # Buck section
    d.text((95, 250), "U3  Buck de 5 V", font=font(38, True), fill=NAVY)
    arrow(d, (110, 490), (360, 490), label="VSYS")
    d.line((360, 490, 600, 490), fill=NAVY, width=7); dot(d, (480, 490))
    d.line((480, 490, 480, 660), fill=NAVY, width=4); d.line((430, 660, 530, 660), fill=NAVY, width=4); d.text((350, 685), "C1 10 uF 25 V X7R\nC2 100 nF", font=font(21), fill=GREY)
    d.rounded_rectangle((600, 320, 1110, 770), radius=25, fill="EAF2F8", outline=BLUE, width=6)
    d.text((745, 360), "TPS62160DGKR", font=font(35, True), fill=NAVY)
    d.text((665, 450), "VIN              SW", font=font(26), fill=NAVY)
    d.text((665, 520), "EN               FB", font=font(26), fill=NAVY)
    d.text((665, 590), "GND              PG", font=font(26), fill=NAVY)
    d.text((665, 665), "Buck sincrono 3 a 17 V, 1 A", font=font(23), fill=GREY)
    d.line((600, 490, 520, 490), fill=NAVY, width=7)
    arrow(d, (1110, 490), (1280, 490), label="SW")
    d.rounded_rectangle((1280, 420, 1490, 560), radius=14, fill="FFFBEA", outline="A86816", width=5)
    multiline_center(d, (1290, 430, 1480, 550), "L1 2,2 uH\nIsat >= 2,5 A\nDCR baixo", font(23, True), "7A4E10", 3)
    arrow(d, (1490, 490), (1760, 490), label="+5V")
    d.line((1630, 490, 1630, 660), fill=NAVY, width=4); d.line((1580, 660, 1680, 660), fill=NAVY, width=4); d.text((1500, 685), "C3 22 uF 10 V X7R\nC4 100 nF", font=font(21), fill=GREY)
    # Feedback divider is outside U3 so the values remain readable in print.
    d.line((1630, 490, 1630, 755), fill=NAVY, width=4); d.line((1630, 755, 1490, 755), fill=NAVY, width=4)
    d.rectangle((1420, 760, 1560, 840), outline=NAVY, width=4); multiline_center(d, (1425, 765, 1555, 835), "RFB_TOP\n576 kohm 1%", font(18, True), NAVY, 2)
    d.line((1490, 840, 1490, 885), fill=NAVY, width=4); dot(d, (1490, 885)); d.text((1515, 862), "FB", font=font(20, True), fill=NAVY)
    d.line((1110, 520, 1275, 520), fill=NAVY, width=4); d.line((1275, 520, 1275, 885), fill=NAVY, width=4); d.line((1275, 885, 1420, 885), fill=NAVY, width=4)
    d.rectangle((1420, 930, 1560, 1010), outline=NAVY, width=4); multiline_center(d, (1425, 935, 1555, 1005), "RFB_BOT\n110 kohm 1%", font(18, True), NAVY, 2)
    d.line((1490, 885, 1490, 930), fill=NAVY, width=4); d.line((1490, 1010, 1490, 1045), fill=NAVY, width=4); d.line((1440, 1045, 1540, 1045), fill=NAVY, width=4)
    d.text((615, 840), "EN: puxar a VSYS por 100 kohm. Opcional: MCU/alarme pode desabilitar.", font=font(22), fill="black")
    d.text((615, 885), "PG: saida open-drain para MCU com pull-up de 100 kohm em +5V.", font=font(22), fill="black")
    # ldo section
    d.line((90, 1110, 2510, 1110), fill=GRID, width=4)
    d.text((95, 1165), "U4  LDO de 3,3 V", font=font(38, True), fill=NAVY)
    arrow(d, (110, 1350), (510, 1350), label="+5V")
    d.line((510, 1350, 680, 1350), fill=NAVY, width=7); dot(d, (590, 1350)); d.line((590, 1350, 590, 1470), fill=NAVY, width=4); d.line((540, 1470, 640, 1470), fill=NAVY, width=4)
    d.text((450, 1480), "CIN 1 uF X7R", font=font(20), fill=GREY)
    d.rounded_rectangle((680, 1200, 1210, 1480), radius=22, fill="EAF2F8", outline=BLUE, width=6)
    multiline_center(d, (710, 1220, 1180, 1310), "MCP1700T-3302", font(34, True), NAVY, 4)
    d.text((760, 1370), "VIN          VOUT", font=font(26), fill=NAVY)
    d.text((760, 1420), "GND", font=font(26), fill=NAVY)
    arrow(d, (1210, 1350), (1640, 1350), label="+3V3")
    d.line((1470, 1350, 1470, 1470), fill=NAVY, width=4); d.line((1420, 1470, 1520, 1470), fill=NAVY, width=4)
    d.text((1340, 1480), "COUT 1 uF X7R\nperto de U4", font=font(20), fill=GREY)
    box(d, (1810, 1190, 2500, 1485), "Limite termico do LDO", "MCP1700 e adequado para a carga leve e modo baixo consumo. Se a carga continua de 3,3 V exceder aproximadamente 100 a 150 mA, calcule P = (5 - 3,3) x I. Se a temperatura ficar alta, substitua U4 por um buck de 3,3 V e mantenha o LDO apenas para rail analogico leve.", "FFF8E9", "A86816", "A86816")
    img.save(path, dpi=(300, 300))
    return path


def set_cell_shading(cell, fill):
    fill = fill.lstrip("#")
    tcPr = cell._tc.get_or_add_tcPr()
    shd = tcPr.find(qn("w:shd"))
    if shd is None:
        shd = OxmlElement("w:shd")
        tcPr.append(shd)
    shd.set(qn("w:fill"), fill)


def set_cell_borders(cell, color=GRID):
    color = color.lstrip("#")
    tc = cell._tc
    tcPr = tc.get_or_add_tcPr()
    borders = tcPr.first_child_found_in("w:tcBorders")
    if borders is None:
        borders = OxmlElement("w:tcBorders")
        tcPr.append(borders)
    for edge in ("top", "left", "bottom", "right", "insideH", "insideV"):
        tag = qn(f"w:{edge}")
        element = borders.find(tag)
        if element is None:
            element = OxmlElement(f"w:{edge}")
            borders.append(element)
        element.set(qn("w:val"), "single")
        element.set(qn("w:sz"), "6")
        element.set(qn("w:color"), color)


def set_cell_margin(cell, top=90, start=110, bottom=90, end=110):
    tc = cell._tc
    tcPr = tc.get_or_add_tcPr()
    tcMar = tcPr.first_child_found_in("w:tcMar")
    if tcMar is None:
        tcMar = OxmlElement("w:tcMar")
        tcPr.append(tcMar)
    for m, v in (("top", top), ("start", start), ("bottom", bottom), ("end", end)):
        node = tcMar.find(qn(f"w:{m}"))
        if node is None:
            node = OxmlElement(f"w:{m}")
            tcMar.append(node)
        node.set(qn("w:w"), str(v)); node.set(qn("w:type"), "dxa")


def add_hyperlink(paragraph, text, url, color="0563C1"):
    part = paragraph.part
    r_id = part.relate_to(url, "http://schemas.openxmlformats.org/officeDocument/2006/relationships/hyperlink", is_external=True)
    hyperlink = OxmlElement("w:hyperlink")
    hyperlink.set(qn("r:id"), r_id)
    new_run = OxmlElement("w:r")
    rPr = OxmlElement("w:rPr")
    c = OxmlElement("w:color"); c.set(qn("w:val"), color); rPr.append(c)
    u = OxmlElement("w:u"); u.set(qn("w:val"), "single"); rPr.append(u)
    new_run.append(rPr)
    t = OxmlElement("w:t"); t.text = text; new_run.append(t)
    hyperlink.append(new_run)
    paragraph._p.append(hyperlink)


def add_para(doc, text="", style=None, before=0, after=7, align=None):
    p = doc.add_paragraph(style=style)
    p.paragraph_format.space_before = Pt(before)
    p.paragraph_format.space_after = Pt(after)
    p.paragraph_format.line_spacing = 1.12
    if align is not None:
        p.alignment = align
    if text:
        p.add_run(text)
    return p


def add_bold_para(doc, lead, rest, before=0, after=7):
    p = add_para(doc, before=before, after=after)
    p.add_run(lead).bold = True
    p.add_run(rest)
    return p


def add_bullets(doc, items):
    for item in items:
        p = doc.add_paragraph(style="List Bullet")
        p.paragraph_format.space_after = Pt(3)
        p.paragraph_format.line_spacing = 1.05
        p.add_run(item)


def add_caption(doc, text):
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.space_before = Pt(2)
    p.paragraph_format.space_after = Pt(9)
    r = p.add_run(text)
    r.italic = True
    r.font.size = Pt(9.2)
    r.font.color.rgb = RGBColor.from_string(GREY.lstrip("#"))
    return p


def add_figure(doc, path, width=7.1, caption=None):
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.space_before = Pt(3)
    p.paragraph_format.space_after = Pt(1)
    p.add_run().add_picture(str(path), width=Inches(width))
    if caption:
        add_caption(doc, caption)


def add_table(doc, headers, rows, widths=None, font_size=9.0):
    table = doc.add_table(rows=1, cols=len(headers))
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    table.style = "Table Grid"
    hdr = table.rows[0]
    for idx, value in enumerate(headers):
        cell = hdr.cells[idx]
        cell.text = value
        set_cell_shading(cell, NAVY)
        set_cell_borders(cell)
        set_cell_margin(cell)
        cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
        for run in cell.paragraphs[0].runs:
            run.font.color.rgb = RGBColor(255,255,255); run.font.bold = True; run.font.size = Pt(font_size)
        cell.paragraphs[0].alignment = WD_ALIGN_PARAGRAPH.CENTER
    for r_idx, row in enumerate(rows):
        cells = table.add_row().cells
        for idx, value in enumerate(row):
            cell = cells[idx]
            cell.text = str(value)
            set_cell_shading(cell, "F4F7F9" if r_idx % 2 else "FFFFFF")
            set_cell_borders(cell)
            set_cell_margin(cell)
            cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
            for par in cell.paragraphs:
                par.paragraph_format.space_after = Pt(0)
                par.paragraph_format.line_spacing = 1.03
                for run in par.runs:
                    run.font.size = Pt(font_size)
    if widths:
        for row in table.rows:
            for i, width in enumerate(widths):
                row.cells[i].width = Inches(width)
    doc.add_paragraph().paragraph_format.space_after = Pt(1)
    return table


def add_reference(doc, tag, desc, url):
    p = add_para(doc, before=2, after=4)
    r = p.add_run(f"[{tag}] ")
    r.bold = True
    p.add_run(desc + " ")
    add_hyperlink(p, url, url)
    return p


def clear_paragraph_borders(target):
    """Remove the inherited blue Title border that some Word installations add."""
    pPr = target._element.get_or_add_pPr()
    borders = pPr.find(qn("w:pBdr"))
    if borders is not None:
        pPr.remove(borders)


def build_document():
    assets = [save_architecture(), save_input(), save_charger(), save_lvd(), save_regulators()]
    doc = Document()
    # Only the cover needs a hard page break. Natural flow prevents isolated
    # continuation paragraphs and large blank pages in the technical sections.
    original_add_page_break = doc.add_page_break
    page_break_counter = {"count": 0}
    def cover_only_page_break():
        page_break_counter["count"] += 1
        if page_break_counter["count"] == 1:
            return original_add_page_break()
        return None
    doc.add_page_break = cover_only_page_break
    sec = doc.sections[0]
    sec.top_margin = Inches(0.62); sec.bottom_margin = Inches(0.62)
    sec.left_margin = Inches(0.65); sec.right_margin = Inches(0.65)
    sec.page_width = Inches(8.5); sec.page_height = Inches(11)

    styles = doc.styles
    styles["Normal"].font.name = "Arial"; styles["Normal"].font.size = Pt(10.4); styles["Normal"].font.color.rgb = RGBColor(0,0,0)
    styles["Normal"]._element.rPr.rFonts.set(qn("w:ascii"), "Arial"); styles["Normal"]._element.rPr.rFonts.set(qn("w:hAnsi"), "Arial")
    for name, size in (("Title", 26), ("Heading 1", 16), ("Heading 2", 12.5), ("Heading 3", 11.2)):
        st = styles[name]
        st.font.name = "Arial"; st.font.size = Pt(size); st.font.bold = True; st.font.color.rgb = RGBColor(0,0,0)
        st._element.rPr.rFonts.set(qn("w:ascii"), "Arial"); st._element.rPr.rFonts.set(qn("w:hAnsi"), "Arial")
    clear_paragraph_borders(styles["Title"])
    styles["Heading 1"].paragraph_format.space_before = Pt(15); styles["Heading 1"].paragraph_format.space_after = Pt(7)
    styles["Heading 2"].paragraph_format.space_before = Pt(10); styles["Heading 2"].paragraph_format.space_after = Pt(5)

    # Footer
    footer = sec.footer
    fp = footer.paragraphs[0]
    fp.alignment = WD_ALIGN_PARAGRAPH.CENTER
    fr = fp.add_run("Projeto de alimentacao VRLA AGM para TCC")
    fr.font.size = Pt(8.3); fr.font.color.rgb = RGBColor.from_string(GREY.lstrip("#"))

    # cover
    p = doc.add_paragraph(style="Title")
    clear_paragraph_borders(p)
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.space_before = Pt(72); p.paragraph_format.space_after = Pt(18)
    p.add_run("Projeto de alimentacao para placa de TCC com bateria VRLA AGM")
    p = add_para(doc, "Documento de arquitetura, circuitos de referencia, seguranca e implementacao no KiCad", after=14, align=WD_ALIGN_PARAGRAPH.CENTER)
    for r in p.runs: r.font.size = Pt(14)
    p = add_para(doc, "Versao atualizada para bateria chumbo-acida regulada por valvula, 12 V, tipo AGM", after=30, align=WD_ALIGN_PARAGRAPH.CENTER)
    for r in p.runs: r.font.size = Pt(12); r.font.color.rgb = RGBColor.from_string(GREY.lstrip("#"))
    add_bold_para(doc, "Decisao de projeto. ", "A referencia deste documento e uma bateria VRLA AGM de 12 V e 7 Ah, instalada fora da area mais quente da caixa. O painel solar e uma fonte externa de 15 a 18 Vcc alimentam um carregador discreto. A bateria fornece o barramento VSYS, que varia com o estado de carga; a placa gera 5 V e 3,3 V a partir dele.", before=10, after=12)
    add_bold_para(doc, "Limite de escopo. ", "Nao ha rede CA dentro da PCB. A energia eletrica externa deve chegar por uma fonte AC/DC certificada, em baixa tensao continua. Valores de carga e protecao devem ser confirmados na folha de dados da bateria adquirida.", after=18)
    add_para(doc, "Leitura recomendada: comece pela arquitetura na pagina seguinte, depois escolha a bateria real e ajuste o carregador e o corte por subtensao. As notas [R1] a [R7] aparecem explicitamente nas secoes tecnicas e sao listadas ao fim.", after=14)
    doc.add_page_break()

    # Section 1
    doc.add_heading("1 Arquitetura recomendada", level=1)
    add_para(doc, "O encadeamento correto nao e painel solar ou fonte de 12 V -> buck -> bateria. Uma VRLA de 12 V exige tensao de carga maior que 12 V. Por isso, as fontes entram primeiro em um carregador dedicado; a bateria fica entre o carregador e os reguladores da placa. O carregador e construido com CI, MOSFETs, indutor, passivos e sensores na propria PCB, sem modulo pronto.")
    add_figure(doc, assets[0], 7.12, "Figura 1 - Fluxo de energia e nomes dos barramentos. A exposicao solar e exclusiva do painel; a bateria deve permanecer sombreada e ventilada.")
    add_bold_para(doc, "Ponto que muda em relacao a Li-ion. ", "Desaparecem o BMS 3S e o balanceamento de celulas, mas surgem tres itens obrigatorios: perfil de carga bulk/recharge adequado, compensacao por temperatura e corte de descarga profunda. A VRLA ainda pode liberar gas pela valvula em condicao anormal de carga; portanto nao deve ficar em um compartimento hermetico. Ver [R3] e [R4].")
    doc.add_heading("1 1 Barramentos e faixas de tensao", level=2)
    add_table(doc,
              ["Rede", "Faixa esperada", "Funcao", "Observacao"],
              [
                  ["VIN_CHG", "Painel nominal 12 V: VMP tipico 17 a 18 V; VOC conforme painel", "Entrada do carregador", "Escolher painel cujo VOC maximo fique abaixo de 28 V, limite de entrada de U1 [R1]."],
                  ["BAT+", "Cerca de 10,5 a 14,7 V", "Terminal da VRLA", "Nao e um 12 V regulado. A tensao depende de carga, descarga e temperatura."],
                  ["VSYS", "Cerca de 11,3 a 14,7 V", "Entrada dos reguladores", "Sai apos fusivel e LVD. Abaixo do limiar o sistema desliga."],
                  ["+5V", "5,0 V regulados", "Sensores e perifericos de 5 V", "Gerado por buck U3."],
                  ["+3V3", "3,3 V regulados", "MCU, LoRa, SD e logica", "Gerado por LDO U4; avaliar dissipacao."],
              ], widths=[0.85, 1.55, 1.55, 3.2], font_size=8.7)
    doc.add_page_break()

    # battery
    doc.add_heading("2 Bateria VRLA AGM e seguranca", level=1)
    add_para(doc, "VRLA significa valve regulated lead acid. AGM e uma forma de VRLA em que o eletrólito fica absorvido em manta de fibra de vidro. Ela e mais apropriada que uma bateria inundada para este projeto porque nao exige reposicao de agua e tem invólucro selado, mas continua sendo uma bateria chumbo-acida e possui valvulas de alivio de gas.")
    doc.add_heading("2 1 Bateria de referencia", level=2)
    add_para(doc, "Para dimensionar os valores exemplificados, foi usada como referencia uma VRLA AGM 12 V e 7 Ah, similar a Power Sonic PS-1270. Essa bateria e somente um ponto de partida: a marca e o modelo finais governam tensoes, corrente de carga, temperatura e limiares definitivos. A PS-1270 publica 14,1 a 14,4 V para carga ciclica, 13,5 a 13,8 V para flutuacao, corrente inicial maxima de 2,1 A e energia nominal de 84 Wh a 20 h [R3].")
    add_table(doc,
              ["Parametro de projeto", "Valor de referencia", "Como usar"],
              [
                  ["Capacidade", "7 Ah a taxa de 20 h", "Usar para estimativa inicial. A capacidade util cai com corrente alta e temperatura baixa."],
                  ["Corrente bulk inicial", "0,7 A (aprox. C/10)", "Escolha conservadora para painel de 20 a 30 W. Fica abaixo do limite de 2,1 A do exemplo [R3]."],
                  ["Tensao bulk/ciclica", "14,4 V a 20 C", "Ajustar o divisor VFB do carregador. Nunca aplicar este numero a outra bateria sem conferir seu datasheet."],
                  ["Tensao de manutencao", "13,7 V a 20 C", "Usada como VRECH/FLOAT no algoritmo de pulsos de U1."],
                  ["Compensacao termica", "Usar valor do fabricante", "No exemplo PS-1270: -30 mV/C em ciclica e -18 mV/C em flutuacao [R3]."],
              ], widths=[1.55, 1.6, 4.0], font_size=8.8)
    doc.add_heading("2 2 Instalacao fisica segura", level=2)
    add_bullets(doc, [
        "Instalar a bateria fora da PCB e fora da incidencia solar direta, em compartimento sombreado e ventilado. O painel e o elemento que deve receber sol.",
        "Nao selar a bateria dentro de uma caixa hermetica. Baterias VRLA podem liberar hidrogenio; o fabricante alerta para risco de mistura explosiva e pede que nao sejam colocadas em recipiente fechado [R4].",
        "Fixar a bateria mecanicamente e manter os terminais protegidos contra contato acidental. Os cabos do positivo devem ser curtos e ter fusivel junto ao terminal positivo.",
        "Manter a bateria afastada de fontes de calor, do indutor do carregador e de conectores que possam gerar faisca. O NTC de medicao deve tocar a lateral da bateria, nao o ar da caixa.",
        "Providenciar reciclagem adequada no fim da vida. Chumbo e eletrólito nao devem ser descartados no lixo comum [R4].",
    ])
    add_bold_para(doc, "Nota termica. ", "A faixa de operacao recomendada para VRLA pela Yuasa e 10 a 30 C, com 20 C como condicao de referencia; temperaturas mais altas reduzem a vida util [R4]. No projeto, bloquear a carga quando a bateria exceder o limite de seu fabricante e registrar essa temperatura no firmware, se houver ADC disponivel.")
    doc.add_page_break()

    # inputs
    doc.add_heading("3 Entradas de energia antes do carregador", level=1)
    add_para(doc, "A folha de alimentacao deve desenhar as entradas a esquerda e os reguladores a direita. Nao usar a rotulacao 12 V para a entrada solar, porque um painel nominal de 12 V trabalha perto de 17 a 18 V no ponto de maxima potencia. O seu VOC pode ser maior e deve ser conferido no painel real.")
    add_figure(doc, assets[1], 7.12, "Figura 2 - Entradas e OR de fontes. O OR impede que uma fonte empurre corrente para a outra.")
    doc.add_heading("3 1 Painel e fonte externa", level=2)
    add_bullets(doc, [
        "J1 PV: painel nominal 12 V, com VMP proximo de 17 a 18 V. Confirmar que o VOC maximo, inclusive em temperatura baixa, permanece abaixo de 28 V, conforme a faixa de entrada de U1 [R1].",
        "J2 EXT para carga: fonte AC/DC externa certificada de 15 a 18 Vcc, com corrente superior a corrente de carga escolhida mais o consumo simultaneo da placa. Ela ocupa o lugar do painel em VIN_CHG.",
        "Uma fonte de 12 Vcc pode alimentar uma entrada de sistema separada, se desejado, mas nao consegue elevar a VRLA ate 14,1 a 14,4 V. Portanto, nao a ligar diretamente ao carregador esperando recarga completa.",
        "F1 e F2: o valor definitivo depende da corrente do painel/fonte e da bitola. Para painel de 20 a 30 W, 2 A e uma referencia inicial, nao uma especificacao universal.",
    ])
    add_bold_para(doc, "TVS de entrada. ", "Nao foi fixado um codigo de TVS neste documento. Em painel de 18 V, muitos TVS com tensao de trabalho suficiente grampeiam acima do maximo absoluto de 28 V do BQ24650. A protecao contra surto deve ser selecionada depois de conhecer VOC, cabo e ambiente. Para a primeira placa, use cabo curto, painel com VOC dentro da especificacao e a rede de entrada recomendada pelo fabricante de U1 [R1].")
    doc.add_page_break()

    # charger
    doc.add_heading("4 Carregador solar discreto", level=1)
    add_para(doc, "O BQ24650 e um controlador buck para carregamento a partir de painel solar, com rastreio de ponto de maxima potencia. Ele requer MOSFETs externos, indutor, resistor de corrente, capacitores e rede de programacao; isto atende ao requisito de nao usar modulo comercial pronto. A TI publicou uma nota de aplicacao especifica para adaptar esse CI a uma bateria chumbo-acida selada [R2].")
    add_figure(doc, assets[2], 7.12, "Figura 3 - Circuito de referencia do carregador. As redes de VFB, NTC e STAT2 implementam os pontos de carga e manutencao da VRLA.")
    doc.add_heading("4 1 Perfil de carga", level=2)
    add_table(doc,
              ["Etapa", "Acao de U1", "Referencia 7 Ah AGM", "Objetivo"],
              [
                  ["Bulk CC", "Regula corrente pelo resistor RSR", "0,7 A", "Recuperar bateria descarregada sem exceder o limite da bateria e do painel."],
                  ["CV bulk", "Regula VFB no ponto alto", "14,4 V a 20 C", "Completar a carga. A corrente cai gradualmente."],
                  ["Termino", "STAT2 muda quando corrente cai", "Cerca de C/30, conforme perfil final", "Evitar manter tensao bulk por tempo excessivo."],
                  ["Recharge/floating", "STAT2 altera a rede VFB e o carregador entra em pulsos", "13,7 V a 20 C", "Manter a bateria sem sobrecarga continua. Conferir compatibilidade com o fabricante."],
              ], widths=[1.1, 2.2, 1.45, 2.4], font_size=8.6)
    add_bold_para(doc, "Como a rede funciona. ", "A nota [R2] usa carga em corrente constante ate a bateria atingir V_BULK. Depois regula tensao; quando a corrente termina, STAT2 desliga Q4 e reduz o ponto de recarga. Ao cair abaixo desse ponto, o CI volta a carregar em pulsos. A rede NTC no divisor VFB altera ambos os pontos com a temperatura. A propria TI adverte que o perfil do fabricante da bateria deve prevalecer.")
    doc.add_page_break()

    # charge details
    doc.add_heading("5 Como implementar U1 no esquematico", level=1)
    add_para(doc, "Crie uma folha hierarquica chamada ALIM 02 CARREGADOR. Use os mesmos nomes de rede da Figura 3. A tabela separa componentes que ja podem ser escolhidos daqueles que dependem do painel e da bateria efetivamente comprados.")
    add_table(doc,
              ["Ref.", "Componente ou rede", "Sugestao inicial", "Observacao de projeto"],
              [
                  ["U1", "BQ24650", "CI controlador solar MPPT", "Usar layout e componentes basicos do datasheet. O encapsulamento exige soldagem SMD cuidadosa [R1]."],
                  ["QH e QL", "MOSFET N 30 V", "RDS(on) baixo, logic level, QG compativel", "Dimensionar por corrente, tensao e perdas. Manter o loop QH/QL/CIN minimo."],
                  ["L1", "Indutor do carregador", "10 uH, Isat >= 1,5 A", "A margem cobre carga de 0,7 A e ripple. Confirmar pelo calculo do datasheet [R1]."],
                  ["RSR", "Resistor de corrente", "0,056 ohm, 1%, 0,25 W", "Ponto inicial para aproximadamente 0,7 A quando a referencia de 40 mV for aplicavel. Validar em bancada."],
                  ["CIN", "Capacitores de entrada", "2 x 10 uF X7R mais 100 nF", "Colocar junto a QH/QL e GND de U1. Considerar reducao de capacitancia por bias."],
                  ["CBAT", "Capacitor no lado da bateria", "100 uF baixo ESR mais 1 uF", "Reduz ripple na bateria e estabiliza a etapa buck."],
                  ["RMP", "Divisor MPPSET", "Calcular com VMP real", "Define o ponto de rastreio. Nao copiar valor sem conhecer o painel."],
                  ["RT, RB, NTC, Q4", "Rede de VFB e STAT2", "Seguir SLVA437A e recalcular", "Define V_BULK, V_RECH/FLOAT e compensacao. NTC de 100 k e exemplo da TI, nao regra universal."],
              ], widths=[0.52, 1.25, 1.9, 3.45], font_size=8.1)
    doc.add_heading("5 1 Valores que nao devem ser copiados cegamente", level=2)
    add_bullets(doc, [
        "O exemplo de [R2] foi feito para uma VRLA de 2,4 Ah e usa V_BULK de 14,85 V e V_RECH de 14,1 V. Esses numeros nao sao automaticamente corretos para uma AGM de 7 Ah.",
        "Para a bateria de referencia de 7 Ah, usar inicialmente 14,4 V ciclica e 13,7 V de manutencao a 20 C, somente se a folha de dados da bateria final concordar [R3].",
        "O painel define o divisor MPPSET. Medir VMP e VOC no modelo real; alterar painel implica revisar RMP e a protecao de entrada.",
        "O NTC deve compensar a bateria e nao a temperatura da PCB. Se a bateria e a placa ficarem fisicamente separadas, levar dois fios de sensor ao conector da bateria.",
    ])
    add_bold_para(doc, "Verificacao obrigatoria. ", "Antes de conectar a VRLA, testar o carregador com fonte de bancada limitada e carga eletrônica/resistiva. Confirmar V_BULK, V_RECH, corrente de carga, transicao de STAT2 e variacao com NTC. So entao conectar a bateria verdadeira, com fusivel instalado.")
    doc.add_page_break()

    # LVD
    doc.add_heading("6 Protecao da bateria e corte por subtensao", level=1)
    add_para(doc, "A VRLA nao possui BMS de celulas. O corte de descarga profunda e, portanto, uma funcao da sua placa. U2 monitora BAT_FUSED antes da chave de carga e comanda Q3. Isso desliga a eletrônica quando a bateria cai e reduz o risco de descarga lenta continua durante a noite.")
    add_figure(doc, assets[3], 7.12, "Figura 4 - Fusivel e LVD. O supervisor e alimentado antes de Q3, permitindo que VSYS seja desligado e depois religado quando a bateria se recuperar.")
    doc.add_heading("6 1 Escolha de limiares", level=2)
    add_para(doc, "O divisor R1/R2 mostrado usa a referencia interna de 400 mV do TPS3700 e coloca o limiar base perto de 11,36 V. Esse valor e uma partida para uma AGM de 12 V em carga leve, nao um valor garantido de estado de carga. A tensao em carga cai por resistencia interna e por corrente do sistema.")
    add_table(doc,
              ["Sinal", "Alvo inicial", "Uso"],
              [
                  ["Aviso BAT_LOW", "11,8 a 12,0 V sob condicao definida", "Enviar ao MCU, reduzir telemetria e registrar evento."],
                  ["Corte VSYS", "Aproximadamente 11,3 a 11,5 V", "Q3 abre e remove carga eletrônica. Ajustar com teste de autonomia e bateria real."],
                  ["Religamento", "12,0 a 12,4 V ou apos carga", "Usar R_HYS ou latch. Evita liga/desliga repetitivo devido a recuperacao elástica."],
                  ["Carga quente", "Limite do fabricante", "Levar CHG_EN/CE para desabilitar o carregador. Em campo quente, nao ignorar esse sinal."],
              ], widths=[1.25, 2.0, 3.85], font_size=8.7)
    add_bold_para(doc, "Componente recomendado. ", "TPS3700 e uma alternativa de baixo consumo para o LVD porque opera de 1,8 a 18 V e consome tipicamente 5,5 uA, com saídas open-drain [R5]. AO3401A e 2N7002 sao opcoes comuns e baratas para a chave P-MOS e seu driver, desde que as margens de VDS, VGS e corrente sejam conferidas no lote adquirido.")
    doc.add_page_break()

    # regs
    doc.add_heading("7 Reguladores de 5 V e 3,3 V", level=1)
    add_para(doc, "O barramento VSYS de uma VRLA nao deve alimentar diretamente circuitos de 5 V ou 3,3 V. A sequencia recomendada e VSYS -> buck 5 V -> LDO 3,3 V. O buck reduz a perda principal, e o LDO fornece um rail simples e de corrente de repouso muito baixa para a logica.")
    add_figure(doc, assets[4], 7.12, "Figura 5 - Circuitos de 5 V e 3,3 V. Os valores do buck seguem o uso de 5 V ajustaveis e devem ser confrontados com o datasheet do CI antes da PCB final.")
    doc.add_heading("7 1 Escolha dos reguladores", level=2)
    add_table(doc,
              ["Bloco", "CI recomendado", "Por que", "Atenção"],
              [
                  ["Buck 5 V", "TPS62160DGKR", "Entrada 3 a 17 V, saida ate 1 A e Iq tipico de 17 uA [R6]. Encapsulamento VSSOP e mais montavel que WSON.", "A entrada de 17 V deixa margem para VRLA ate 14,7 V, mas nao para surto sem analise."],
                  ["LDO 3,3 V", "MCP1700T-3302", "Ate 250 mA e Iq tipico 1,6 uA; estavel com 1 uF [R7]. Bom para standby e carga leve.", "Avaliar calor para consumo continuo alto em 3,3 V."],
                  ["Nao recomendado", "AMS1117", "Muito popular e barato, mas corrente quiescente na ordem de mA nao favorece autonomia.", "Nao usar como regulador low power da placa."],
              ], widths=[1.0, 1.45, 2.55, 2.1], font_size=8.35)
    add_bold_para(doc, "Dissipacao do LDO. ", "Calcule P_LDO = (5,0 - 3,3) x I_3V3. Em 150 mA, a perda e aproximadamente 0,255 W. Se o LoRa, SD e MCU mantiverem consumo alto por tempo prolongado, prefira um buck adicional de 3,3 V e reserve o MCP1700 para sensores analogicos ou standby.")
    doc.add_page_break()

    # kicad and PCB
    doc.add_heading("8 Como organizar a folha de alimentacao no KiCad", level=1)
    add_para(doc, "Organize a folha principal da esquerda para a direita e separe os blocos em folhas hierarquicas. Evite um fio longo de painel atravessando toda a pagina. Use etiquetas globais de rede e conectores bem identificados.")
    add_table(doc,
              ["Folha", "Conteudo", "Entradas", "Saidas"],
              [
                  ["ALIM 01 ENTRADAS", "J1, J2, fusivel, anti-inversao e OR", "PV+, EXT+, GND", "VIN_CHG"],
                  ["ALIM 02 CARREGADOR", "BQ24650, MOSFETs, indutor, NTC e divisores", "VIN_CHG, BAT+, CHG_EN", "BAT+, STAT, TP_CHG"],
                  ["ALIM 03 BATERIA LVD", "J3, F3, TPS3700 e chave P-MOS", "BAT+", "BAT_FUSED, VSYS, BAT_LOW"],
                  ["ALIM 04 BUCK 5V", "TPS62160 e filtros", "VSYS", "+5V, PG_5V"],
                  ["ALIM 05 LDO 3V3", "MCP1700 e desacoplamentos", "+5V", "+3V3"],
              ], widths=[1.45, 3.15, 1.05, 1.45], font_size=8.7)
    doc.add_heading("8 1 Etiquetas e pontos de teste", level=2)
    add_bullets(doc, [
        "Usar etiquetas: PV_PROT, EXT_PROT, VIN_CHG, BAT+, BAT_FUSED, VSYS, +5V, +3V3, BAT_LOW, CHG_EN, NTC_BAT, PG_5V.",
        "Adicionar TP_PV, TP_VIN_CHG, TP_BAT, TP_VSYS, TP_5V, TP_3V3 e TP_NTC. Os testes de ajuste sao muito mais seguros quando nao exigem tocar em pinos de CI.",
        "Manter GND comum, plano de terra continuo e retorno de corrente do carregador separado da referencia de VFB/MPPSET ate um ponto calmo de terra.",
    ])
    doc.add_heading("8 2 Regras de layout", level=2)
    add_bullets(doc, [
        "No carregador, deixar CIN, QH, QL e GND muito proximos. O no PH e ruidoso; manter pequeno e distante de VFB, MPPSET e NTC.",
        "Ligar SRP e SRN em Kelvin diretamente aos dois lados de RSR. Nao usar o caminho de alta corrente para a medicao.",
        "Colocar U3, indutor e capacitores de 5 V juntos. O loop VIN-U3-SW-indutor-COUT deve ser compacto.",
        "Dimensionar trilhas e conectores de BAT+ pelo fusivel e pela corrente maxima. A corrente de curto de uma VRLA e muito maior que o consumo normal.",
        "Separar fisicamente a bateria da PCB e do indutor. Se ficar dentro do mesmo gabinete, prever entrada/saida de ar sem permitir agua na eletrônica.",
    ])
    doc.add_page_break()

    # BOM / budget
    doc.add_heading("9 Lista de materiais e custo beneficio", level=1)
    add_para(doc, "A lista abaixo privilegia componentes amplamente distribuídos e montagem possivel em uma PCB de TCC. Valores de preco nao foram fixados porque variam com fornecedor, quantidade e disponibilidade; a decisao de custo deve considerar tambem o painel, bateria, cabos e gabinete.")
    add_table(doc,
              ["Grupo", "Item", "Recomendacao", "Criterio de custo beneficio"],
              [
                  ["Armazenamento", "VRLA AGM 12 V 7 Ah", "Modelo com datasheet de carga ciclica e float", "Mais barata e simples de adquirir que Li-ion montada, mas pesada. Boa para sistema estacionario."],
                  ["Carga solar", "BQ24650 + MOSFETs + L1", "Implementacao discreta conforme [R1] e [R2]", "Mais complexa que modulo, porem documentavel, ajustavel e adequada ao requisito do TCC."],
                  ["Corte de bateria", "TPS3700 + AO3401A + 2N7002", "Baixo Iq e componentes pequenos", "Evita substituir bateria precocemente por descarga profunda."],
                  ["5 V", "TPS62160DGKR", "VSSOP, L 2,2 uH, X7R", "Bom compromisso entre Iq, eficiencia e dificuldade de montagem."],
                  ["3,3 V", "MCP1700T-3302", "SOT-23, 1 uF entrada/saida", "Muito baixo consumo e barato; avaliar corrente continua."],
                  ["Protecao", "Fusiveis, conectores e NTC", "Nao economizar nestes itens", "Sao pequenos no custo e determinantes para seguranca e manutencao."],
              ], widths=[1.05, 1.45, 2.2, 2.95], font_size=8.35)
    add_bold_para(doc, "Conclusao de custo. ", "A VRLA AGM tem melhor custo inicial quando o peso nao importa. O custo total piora se ela for profundamente descarregada todos os dias ou ficar quente; por isso carregamento correto, sombra e LVD nao sao opcional. Para uma estacao meteorologica fixa, esse conjunto e uma escolha defensavel e de boa relacao custo beneficio.")
    doc.add_heading("9 1 Estimativa simples de autonomia", level=2)
    add_para(doc, "Para uma bateria de 7 Ah, energia nominal aproximada e 12 V x 7 Ah = 84 Wh. Para preservar vida util, trabalhar inicialmente com 50% de descarga diaria: 42 Wh. Considerando 85% para perdas de conversao, energia em carga aproximada: 36 Wh. Autonomia aproximada = 36 Wh / potencia media da placa. Exemplo: com media de 2 W, cerca de 18 h. Medir a potencia real em modo de transmissao, escrita SD e sono antes de definir o painel.")
    doc.add_page_break()

    # test plan
    doc.add_heading("10 Plano de testes e criterios de aceite", level=1)
    add_para(doc, "O primeiro comissionamento deve reduzir a energia disponivel e validar cada bloco isoladamente. Uma VRLA pode fornecer corrente alta; nao usar curto acidental como teste de fusivel.")
    add_table(doc,
              ["Ordem", "Teste", "Criterio de aceite"],
              [
                  ["1", "Inspecao e continuidade", "Sem curto entre BAT+, VIN_CHG, VSYS, +5V, +3V3 e GND. Polaridade de Q3, QH/QL e diodos conferida."],
                  ["2", "Buck 5 V com fonte limitada", "Aplicar 12 a 14,7 V em VSYS com limite de corrente baixo. Confirmar 5,0 V e ripple aceitavel antes de conectar carga."],
                  ["3", "LDO 3,3 V", "Confirmar 3,3 V com carga leve e medir temperatura. Verificar consumo em standby."],
                  ["4", "LVD", "Variar BAT_FUSED lentamente. Registrar desligamento, religamento e ausencia de oscilacao. Ajustar R_HYS/latch se necessario."],
                  ["5", "Carregador sem bateria", "Com fonte limitada em VIN_CHG, conferir sinais de gate, tensao de VFB e ausencia de aquecimento anormal."],
                  ["6", "Carregador com carga simulada", "Usar carga capaz de absorver corrente. Medir limite CC e ponto CV; conferir CHG_EN e NTC."],
                  ["7", "Bateria real e painel", "Conectar bateria com fusivel. Medir corrente, V_BULK, V_RECH, temperatura e tensao do painel em diferentes irradiancias."],
              ], widths=[0.6, 2.1, 4.95], font_size=8.5)
    add_bold_para(doc, "Criterio final. ", "O projeto so deve ficar fechado quando os limiares e a temperatura forem medidos no modelo real de bateria, painel, caixa e cabos. Os valores deste documento sao um ponto de partida de engenharia, nao substituem a especificacao da bateria comprada.")
    doc.add_page_break()

    # refs
    doc.add_heading("11 Notas de referencia", level=1)
    add_para(doc, "As notas abaixo sao citadas explicitamente ao longo do texto. Elas foram usadas para escolher a topologia e os valores de referencia; a folha de dados da bateria efetivamente adquirida continua sendo a autoridade para tensao, corrente e temperatura.")
    add_reference(doc, "R1", "Texas Instruments. BQ24650, controlador solar buck com MPPT. Base para faixa de entrada, componentes externos e procedimento de projeto.", "https://www.ti.com/lit/ds/symlink/bq24650.pdf")
    add_reference(doc, "R2", "Texas Instruments. SLVA437A, Using the bq24650 to Charge a Sealed, Lead-Acid Battery. Base para carga CC/CV, transicao por STAT2, pulsos de recarga e compensacao termica na rede VFB.", "https://www.ti.com/lit/an/slva437a/slva437a.pdf")
    add_reference(doc, "R3", "Power Sonic. PS-1270, VRLA AGM 12 V 7 Ah. Referencia de tensoes ciclica e float, limite de corrente, capacidade e advertencias de uso. O modelo foi usado como exemplo, nao como compra obrigatoria.", "https://www.power-sonic.com/product/ps-1270/")
    add_reference(doc, "R4", "GS Yuasa. Operating Instructions for VRLA batteries. Referencia para temperatura recomendada, compensacao de carga, ventilacao, gases e reciclagem.", "https://www.yuasa.com/media/akeneo_connector/asset_files/O/p/Operating_Instructions_EN_ENL_UXF_H_L_9947.pdf")
    add_reference(doc, "R5", "Texas Instruments. TPS3700, detector de janela de tensao de 1,8 a 18 V, referencia interna de 400 mV e corrente quiescente tipica de 5,5 uA. Base do LVD.", "https://www.ti.com/lit/ds/symlink/tps3700.pdf")
    add_reference(doc, "R6", "Texas Instruments. TPS62160, buck 3 a 17 V, 1 A, corrente quiescente tipica de 17 uA. Base do regulador de 5 V.", "https://www.ti.com/lit/ds/symlink/tps62160.pdf")
    add_reference(doc, "R7", "Microchip. MCP1700, LDO de baixa corrente quiescente, ate 250 mA e 1,6 uA tipicos. Base do regulador de 3,3 V.", "https://ww1.microchip.com/downloads/en/DeviceDoc/MCP1700-Data-Sheet-20001826F.pdf")
    doc.add_heading("12 Pendencias antes de fabricar", level=1)
    add_bullets(doc, [
        "Escolher o modelo exato de painel e registrar VMP, VOC, corrente de curto e potencia nominal.",
        "Escolher o modelo exato de VRLA AGM e substituir os valores de V_BULK, VRECH/FLOAT, I_BULK, temperatura e NTC pelos do fabricante.",
        "Calcular RMP, VFB, R_HYS e componentes de potencia com a ferramenta/datasheet de [R1] e a nota [R2].",
        "Confirmar a corrente maxima simultanea de LoRa, cartao SD, MCU, sensores e eventuais atuadores para fechar o indutor, fusivel, conectores e dissipacao de U4.",
        "Revisar ERC, DRC e layout de alta corrente antes de gerar a PCB."
    ])

    doc.core_properties.title = "Projeto de alimentacao VRLA AGM para TCC"
    doc.core_properties.subject = "Arquitetura e circuitos de referencia para painel solar, VRLA AGM, carregador e reguladores"
    doc.core_properties.author = ""
    doc.save(OUT)
    print(OUT)


if __name__ == "__main__":
    build_document()
