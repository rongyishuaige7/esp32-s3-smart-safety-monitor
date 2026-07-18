# Media and EDA evidence

> Publication review: 2026-07-18

## Source and handling

- Authoritative originals remain unchanged in the local Hardware Lab material center.
- This repository contains reviewable, sanitized derivatives only; it does not contain or modify the authoritative originals.
- Media/EDA prove only the narrow historical facts stated below. They do **not** prove that the current public commit was flashed, run, electrically reviewed, or re-tested on physical hardware.
- No physical-device re-test was run in this publication pass.

## Published derivatives

| Public path | Historical date / source date | Sanitization | Narrow evidence boundary |
|:--|:--|:--|:--|
| `hardware/eda/schematic.pdf` | Export supplied 2026-07-18 | PDF metadata removed from a copy | Historical EDA export. The schematic shows one SG90 on GPIO9 while current source defines two servos on GPIO9 and GPIO10; treat the EDA as incomplete and not as current wiring authority. |
| `hardware/eda/pcb-preview.pdf` | Export supplied 2026-07-18 | PDF metadata removed from a copy | Historical EDA export. The schematic shows one SG90 on GPIO9 while current source defines two servos on GPIO9 and GPIO10; treat the EDA as incomplete and not as current wiring authority. |
| `hardware/eda/bom-export.csv` | Export supplied 2026-07-18 | Converted from UTF-16LE tabular export to UTF-8 CSV; supplier/store/price fields omitted | Historical EDA export. The schematic shows one SG90 on GPIO9 while current source defines two servos on GPIO9 and GPIO10; treat the EDA as incomplete and not as current wiring authority. |
| `hardware/eda/gerber/` | Export supplied 2026-07-18 | Extracted Gerber/Drill members from a copy; third-party order-instruction text omitted | Historical EDA export. The schematic shows one SG90 on GPIO9 while current source defines two servos on GPIO9 and GPIO10; treat the EDA as incomplete and not as current wiring authority. |

## Rejected or omitted material

Files not listed above were not copied into this repository. Typical reasons include a duplicate/misfiled document, private identifiers, stale UI semantics, incomplete wiring information, or weak value relative to the public evidence boundary. The original local files remain unchanged.

## Status rule

Current hardware re-test not run. Historical media or EDA must not be promoted to current-commit hardware verification.
