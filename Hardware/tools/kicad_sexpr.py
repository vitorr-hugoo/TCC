"""Small lossless S-expression reader for reviewing KiCad 9 design files.

Keeps source spans so targeted patches need not reformat an existing schematic.
"""
from dataclasses import dataclass
import json
import re


@dataclass
class Node:
    values: list
    start: int
    end: int

    @property
    def tag(self):
        return self.values[0] if self.values else ''

    def all(self, tag):
        return [x for x in self.values if isinstance(x, Node) and x.tag == tag]

    def one(self, tag):
        return next(iter(self.all(tag)), None)

    def get(self, tag, default=None):
        n = self.one(tag)
        return n.values[1] if n else default

    def prop(self, name, default=None):
        return next((n.values[2] for n in self.all('property') if n.values[1] == name), default)


def parse(text):
    tokens = list(re.finditer(r'"(?:\\.|[^"\\])*"|[()]|[^\s()]+', text))
    pos = 0

    def read():
        nonlocal pos
        token = tokens[pos]
        pos += 1
        if token.group() != '(':
            s = token.group()
            return json.loads(s, strict=False) if s.startswith('"') else s
        children = []
        while tokens[pos].group() != ')':
            children.append(read())
        end = tokens[pos].end()
        pos += 1
        return Node(children, token.start(), end)

    result = read()
    if pos != len(tokens):
        raise ValueError('Unexpected trailing tokens')
    return result


def replace_spans(text, replacements):
    for start, end, value in sorted(replacements, reverse=True):
        text = text[:start] + value + text[end:]
    return text


def patch_file(path, old, new):
    import difflib
    lines = list(difflib.unified_diff(old.splitlines(), new.splitlines(), n=3))
    if not lines:
        return ''
    return f'*** Update File: {path.as_posix()}\n' + '\n'.join(
        '@@' if x.startswith('@@') else x for x in lines[2:]
    ) + '\n'


if __name__ == '__main__':
    from pathlib import Path
    import sys
    for fn in sys.argv[1:]:
        text = Path(fn).read_text(encoding='utf-8-sig')
        root = parse(text)
        print(fn)
        for n in root.values:
            if not isinstance(n, Node):
                continue
            if n.tag == 'symbol':
                print(n.prop('Reference'), n.prop('Value'), n.get('unit'), n.one('at').values[1:], n.get('uuid'))
            elif n.tag == 'sheet':
                print('SHEET', n.prop('Sheetname'), n.prop('Sheetfile'), n.one('at').values[1:], n.get('uuid'))
            elif n.tag in ('global_label', 'hierarchical_label', 'label'):
                print(n.tag, n.values[1], n.one('at').values[1:])
