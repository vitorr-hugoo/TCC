# BOM da alimentação — revisão PWR-1

Consulta: 2026-09-18 America/Sao_Paulo. Lista extraída dos símbolos implementados, incluindo C69 opcional. Subtotal indicativo dos **24 componentes montados: US$ 2.8466**. Não é orçamento nem reserva: exclui quantidade mínima, excedentes de compra, taxas de componentes Extended, PCB, montagem, THT, frete e impostos. Para poucas placas, essas taxas podem dominar o custo.

Os números são `canPresaleNumber` do catálogo público JLCPCB: disponibilidade de **pré-compra/fornecedor**, não comprovação de estoque interno reservado para PCBA. Confirmar disponibilidade efetiva e pacote de montagem ao enviar a BOM. `initialPrice` é o preço indicativo consultado, não garantia de faixa de quantidade.

| Referências | Qtd. | Montagem | MPN | Código JLC / fonte | Pré-compra (un.) | US$/un. | Categoria retornada |
|---|---:|---|---|---|---:|---:|---|
| J60 | 1 | Montar | DC-005-2.0 | [C22459640](https://jlcpcb.com/partdetail/C22459640) | 14581 | 0.0329 | Extended |
| J61 | 1 | Montar | B2B-XH-A(LF)(SN) | [C158012](https://jlcpcb.com/partdetail/C158012) | 373029 | 0.0406 | Extended |
| F60 | 1 | Montar | S1206-S-1.5A | [C553929](https://jlcpcb.com/partdetail/C553929) | 2081 | 0.0726 | Extended |
| D60 | 1 | Montar | SMBJ16CA | [C3001977](https://jlcpcb.com/partdetail/C3001977) | 51467 | 0.0403 | Extended |
| Q60 | 1 | Montar | DMP6023LE-13 | [C154901](https://jlcpcb.com/partdetail/C154901) | 6062 | 0.5844 | Extended |
| D61 | 1 | Montar | BZT52C10 | [C19077408](https://jlcpcb.com/partdetail/C19077408) | 643527 | 0.0175 | Extended |
| R60, R63 | 2 | Montar | 0603WAF1003T5E | [C25803](https://jlcpcb.com/partdetail/C25803) | 20983861 | 0.0032 | Basic |
| C60 | 1 | Montar | UWT1V470MCL1GS | [C87068](https://jlcpcb.com/partdetail/C87068) | 3920 | 0.1757 | Extended |
| C61, C62 | 2 | Montar | CL31A106KBHNNNE | [C13585](https://jlcpcb.com/partdetail/C13585) | 2326751 | 0.2658 | Basic |
| C63, C64 | 2 | Montar | CC0603KRX7R9BB104 | [C14663](https://jlcpcb.com/partdetail/C14663) | 40128061 | 0.0124 | Basic |
| U60 | 1 | Montar | AP63205WU-7 | [C2071056](https://jlcpcb.com/partdetail/C2071056) | 48807 | 0.4216 | Extended |
| L60 | 1 | Montar | MWSA0603S-4R7MT | [C408447](https://jlcpcb.com/partdetail/C408447) | 63210 | 0.1320 | Extended |
| C65, C66 | 2 | Montar | CL31A226KAHNNNE | [C12891](https://jlcpcb.com/partdetail/C12891) | 739093 | 0.1759 | Basic |
| U61 | 1 | Montar | TLV62569DBVR | [C141836](https://jlcpcb.com/partdetail/C141836) | 123438 | 0.0795 | Extended |
| C67, C68 | 2 | Montar | CL21A106KAYNNNE | [C15850](https://jlcpcb.com/partdetail/C15850) | 5099099 | 0.0844 | Basic |
| L61 | 1 | Montar | MWSA0402S-2R2MT | [C408335](https://jlcpcb.com/partdetail/C408335) | 27058 | 0.1527 | Extended |
| C69 | 1 | DNP | CL31A226KAHNNNE | [C12891](https://jlcpcb.com/partdetail/C12891) | 739093 | 0.1759 | Basic |
| R61 | 1 | Montar | 0603WAF3003T5E | [C23024](https://jlcpcb.com/partdetail/C23024) | 582108 | 0.0054 | Basic |
| R62 | 1 | Montar | 0603WAF1503T5E | [C22807](https://jlcpcb.com/partdetail/C22807) | 690719 | 0.0044 | Basic |
| C70 | 1 | Montar | 0603CG6R8C500NT | [C1679](https://jlcpcb.com/partdetail/C1679) | 100245 | 0.0036 | Extended |

JP60 e TP60–TP63 são cobre da própria PCB, não peças compradas. Os quatro PWR_FLAG são declarações elétricas, sem footprint. C69 permanece **DNP**; não deve entrar na montagem. J60/J61 são THT: confirmar serviço de montagem ou soldar manualmente. Chicote, carcaça/crimps JST e plugue P4 não estão nesta BOM.

## Símbolos e footprints auditados

Todos os footprints existem e seus números de pads correspondem aos pinos dos símbolos. Isso não substitui cotejo dimensional final, inspeção de montagem e teste mecânico de J60 com o plugue real.

| Ref. | Símbolo | Footprint | Pinos/pads | Origem |
|---|---|---|---|---|
| J60 | Hardware_Power:DC-005-2.0_C22459640 | Hardware_Power:DC-IN-TH_DC-005-2.0_C22459640 | 1, 2, 3 | EasyEDA convertido e normalizado KiCad 9 |
| J61 | Connector_Generic:Conn_01x02 | Connector_JST:JST_XH_B2B-XH-A_1x02_P2.50mm_Vertical | 1, 2 | KiCad 9 padrão |
| JP60 | Jumper:SolderJumper_3_Open | Jumper:SolderJumper-3_P2.0mm_Open_TrianglePad1.0x1.5mm_NumberLabels | 1, 2, 3 | KiCad 9 padrão |
| F60 | Device:Fuse | Fuse:Fuse_1206_3216Metric | 1, 2 | KiCad 9 padrão |
| D60 | Device:D_TVS | Diode_SMD:D_SMB | 1, 2 | KiCad 9 padrão |
| Q60 | Hardware_Power:DMP6023LE-13 | Hardware_Power:SOT-223-4_L6.5-W3.5-P2.30-LS7.0-BR | 1, 2, 3, 4 | EasyEDA convertido e normalizado KiCad 9 |
| D61 | Device:D_Zener | Diode_SMD:D_SOD-123 | 1, 2 | KiCad 9 padrão |
| R60 | Device:R | Resistor_SMD:R_0603_1608Metric | 1, 2 | KiCad 9 padrão |
| C60 | Device:C_Polarized | Capacitor_SMD:CP_Elec_6.3x5.8 | 1, 2 | KiCad 9 padrão |
| C61 | Device:C | Capacitor_SMD:C_1206_3216Metric | 1, 2 | KiCad 9 padrão |
| C62 | Device:C | Capacitor_SMD:C_1206_3216Metric | 1, 2 | KiCad 9 padrão |
| C63 | Device:C | Capacitor_SMD:C_0603_1608Metric | 1, 2 | KiCad 9 padrão |
| TP60 | Connector:TestPoint | TestPoint:TestPoint_Pad_D1.5mm | 1 | KiCad 9 padrão |
| U60 | Regulator_Switching:AP63205WU | Package_TO_SOT_SMD:TSOT-23-6 | 1, 2, 3, 4, 5, 6 | KiCad 9 padrão |
| L60 | Device:L | Inductor_SMD:L_Sunlord_MWSA0603S | 1, 2 | KiCad 9 padrão |
| C64 | Device:C | Capacitor_SMD:C_0603_1608Metric | 1, 2 | KiCad 9 padrão |
| C65 | Device:C | Capacitor_SMD:C_1206_3216Metric | 1, 2 | KiCad 9 padrão |
| C66 | Device:C | Capacitor_SMD:C_1206_3216Metric | 1, 2 | KiCad 9 padrão |
| TP61 | Connector:TestPoint | TestPoint:TestPoint_Pad_D1.5mm | 1 | KiCad 9 padrão |
| U61 | Regulator_Switching:TLV62569DBV | Package_TO_SOT_SMD:SOT-23-5 | 1, 2, 3, 4, 5 | KiCad 9 padrão |
| C67 | Device:C | Capacitor_SMD:C_0805_2012Metric | 1, 2 | KiCad 9 padrão |
| L61 | Device:L | Inductor_SMD:L_Sunlord_MWSA0402S | 1, 2 | KiCad 9 padrão |
| C68 | Device:C | Capacitor_SMD:C_0805_2012Metric | 1, 2 | KiCad 9 padrão |
| C69 | Device:C | Capacitor_SMD:C_1206_3216Metric | 1, 2 | KiCad 9 padrão |
| R61 | Device:R | Resistor_SMD:R_0603_1608Metric | 1, 2 | KiCad 9 padrão |
| R62 | Device:R | Resistor_SMD:R_0603_1608Metric | 1, 2 | KiCad 9 padrão |
| R63 | Device:R | Resistor_SMD:R_0603_1608Metric | 1, 2 | KiCad 9 padrão |
| C70 | Device:C | Capacitor_SMD:C_0603_1608Metric | 1, 2 | KiCad 9 padrão |
| TP62 | Connector:TestPoint | TestPoint:TestPoint_Pad_D1.5mm | 1 | KiCad 9 padrão |
| TP63 | Connector:TestPoint | TestPoint:TestPoint_Pad_D1.5mm | 1 | KiCad 9 padrão |

## Importações necessárias

AP63205WU e TLV62569DBV já existem no KiCad 9. Passivos, TVS, zener, JST, jumper e pontos de teste usam símbolos/footprints padrão. Para o jack exato C22459640 e o PMOS C154901, foram importados símbolo, footprint e 3D com **easyeda2kicad 1.0.1**, depois normalizados para KiCad 9. Pinagem elétrica e courtyards foram revisados. No PMOS importado, o tab é pad4 e deve permanecer ligado ao dreno pad2; não trocar por um footprint de três pads sem adaptar o mapeamento.

Biblioteca local: `Hardware/libraries/Hardware_Power/`, registrada com `${KIPRJMOD}` em ambas as tabelas. J60: 1=centro, 2=retorno, 3=contato chaveado não utilizado. Q60: 1=G, 2=D, 3=S, 4=D/tab.

Comando de conversão utilizado, executado na pasta Hardware (não repetir com --overwrite sobre a biblioteca revisada):

```powershell
python -X utf8 -m easyeda2kicad --lcsc_id C22459640 C154901 --symbol --footprint --output D:/TCC/Hardware/libraries/Hardware_Power/Hardware_Power.kicad_sym --project-relative --overwrite
```

A opção `--3d` também foi executada para ambos. Os modelos STEP/WRL são auxiliares de visualização, não autoridade para numeração elétrica ou tolerâncias mecânicas.
