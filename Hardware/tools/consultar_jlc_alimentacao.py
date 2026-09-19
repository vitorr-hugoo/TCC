"""Read public JLC catalog metadata; never orders/reserves components."""
import concurrent.futures
import json
import re
import sys
import urllib.request

def query(code):
    url = 'https://jlcpcb.com/partdetail/' + code
    html = urllib.request.urlopen(url, timeout=45).read().decode()
    records = {}
    chunks = []
    for script in re.findall(r'self\.__next_f\.push\((.*?)\)</script>', html):
        try:
            payload = json.loads(script)[1]
            if not isinstance(payload, str):
                continue
            chunks.append(payload)
        except (ValueError, IndexError):
            pass
    flight = ''.join(chunks)
    decoder = json.JSONDecoder()
    for match in re.finditer(r'([0-9a-f]+):(?=[\[{])', flight):
        try:
            records[match[1]] = decoder.raw_decode(flight, match.end())[0]
        except ValueError:
            pass
    candidates = [r for r in records.values() if isinstance(r, dict) and r.get('componentCode') == code]
    if not candidates:
        raise ValueError('Catalog record not found: ' + code)
    record = {}
    for candidate in candidates:
        record.update(candidate)
    def resolve(v):
        if isinstance(v, str) and v.startswith('$') and v[1:] in records:
            return resolve(records[v[1:]])
        if isinstance(v, list):
            return [resolve(i) for i in v]
        if isinstance(v, dict):
            return {k: resolve(i) for k, i in v.items()}
        return v
    keys = ['componentCode','componentModelEn','componentBrandEn','componentSpecificationEn','describe','componentLibraryType','assemblyMode','dataManualUrl','isBuyComponent','allowPostFlag','canPresaleNumber','overseasStockCount','stockCount','initialPrice','preMinPurchaseNum','leastPatchNumber','prices']
    return {'url': url, **{k: resolve(record[k]) for k in keys if k in record}}

if __name__ == '__main__':
    sys.stdout.reconfigure(encoding='utf-8')
    def safe(c):
        try:
            return query(c)
        except Exception as e:
            return {'code': c, 'error': str(e)}
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        for result in pool.map(safe, sys.argv[1:]):
            print(json.dumps(result, ensure_ascii=False))
