# Geracao de documentos

Os scripts Python geram os documentos e figuras de alimentacao a partir desta
pasta. Eles resolvem a raiz do repositorio dinamicamente, portanto nao dependem
de `D:\\TCC`.

- `build_vrla_document.py` gera o documento VRLA em `docs/tcc/fontes/`.
- `build_referencia_alimentacao_modular.py` gera a referencia tecnica em
  `docs/engineering/hardware/alimentacao/`.

Use `work/render/` para inspecoes e renderizacoes temporarias, nunca como fonte
canonico do TCC.
