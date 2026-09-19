"""Read supplier/manufacturer PDFs and render their drawings for QA."""
import json
from pathlib import Path
import sys
import urllib.request
import pypdfium2 as pdfium

sys.stdout.reconfigure(encoding='utf-8')
base = Path(__file__).resolve().parents[1]
data = json.loads((base/'docs/alimentacao/catalogo_jlc_final.json').read_text(encoding='utf-8'))
out = base/'docs/alimentacao/validacao/datasheets'
out.mkdir(parents=True, exist_ok=True)
for code in sys.argv[1:]:
    part = next(p for p in data['partes'] if p['componentCode'] == code)
    url = part['dataManualUrl']
    official = {'C22459640':'0b943d4c409056643475de1b2f68984a', 'C553929':'e14cebd5b8cf04e322288dba224b0dfb', 'C3001977':'3e73b3257a9400b62dc65075545704a0', 'C87068':'b29e6f29d90af3e9ff13901fa9b8172a'}
    if code in official:
        url = f'https://datasheet.lcsc.com/datasheet/pdf/{official[code]}.pdf?productCode={code}'
    if code == 'C154901':
        url = 'https://www.diodes.com/datasheet/download/DMP6023LE.pdf'
    try:
        blob = urllib.request.urlopen(url, timeout=40).read()
        doc = pdfium.PdfDocument(blob)
        print(code, 'PAGES', len(doc), 'SOURCE', url)
        for i, page in enumerate(doc):
            print('PAGE', i+1, page.get_textpage().get_text_range())
            if i == 0 or len(doc) <= 2 or code == 'C154901' and i == 5:
                page.render(scale=1.5).to_pil().save(out/f'{code}-{i+1}.png')
    except Exception as e:
        print(code, type(e).__name__, str(e))
