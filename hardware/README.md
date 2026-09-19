# Hardware

O projeto principal esta em `kicad/estacao-meteorologica/`. Abra o arquivo
`estacao-meteorologica.kicad_pro` no KiCad. As folhas hierarquicas ficam em
`sheets/` e as bibliotecas locais do projeto ficam em `libraries/`.

As tabelas de simbolos e footprints usam caminhos relativos para
`../../libraries`, de modo que o projeto permanece portatil apos o clone.
`kicad/legacy/` preserva folhas antigas que nao fazem parte da hierarquia ativa.
Pacotes de fabricacao aprovados devem ser organizados por versao em `releases/`.
