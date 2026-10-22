# Sony CLIÉ NX70V experimental branch

This branch `feature/sony-nx70v` was created from `feature/sony-tg50` so all TG50 work remains available while NX70V investigation is isolated. **It does not yet emulate NX70V hardware.** The existing `--tg50` switch initializes TG50 peripherals, even if handed an NX70V image; that is not an NX70V boot implementation.

## Inspected NOR image (local file, not committed)

The supplied `nx70v.rar` contains a single `nx70v.rom` raw image. Independently inspected bytes:

| Property | Observed value |
| --- | --- |
| Length | 12,582,912 bytes / 12 MiB |
| SHA-256 | `d491f2c18ba2ade87683bed705ef9ffb7e1ca1b1a5b9ad219ec34e043d0ebb86` |
| Header offset | `0x20000` (signature at `0x20008`) |
| Signature | `0xFEEDBEEF` little-endian |
| Card | `PalmCard` |
| Manufacturer | `Sony Corporation` |
| Company ID | `0x736F6E79` (`sony`) |
| HAL ID | `0x63726462` (`crdb`) |

These bytes pass the structural header check performed by `RomInfo5::Parse()`, but `RomInfo5::GetDeviceType()` does not yet map Sony NX70V; launching without a device override therefore rejects it. The TG50-only override is **not an appropriate NX70V implementation**.

Reproduce the header check: `python3 tools/probe-nx70v-rom.py /path/to/nx70v.rom`.

## Validation status

- Branch created and isolated from TG50: **done**.
- Local header parsing of user-supplied ROM: **done**.
- Native C++ compile and link: **not performed** (SDL2 development files unavailable here).
- Actual emulator CPU execution / NX70V screen boot: **not performed**.
- Next: implement a separate NX70V device type and hardware mapping, beginning with read-only startup diagnostics. Do not merge into TG50 until hardware differences and boot have been validated.

## 한국어

이 브랜치는 `feature/sony-tg50`에서 분기했으며 **NX70V 하드웨어 에뮬레이션은 아직 구현되지 않았습니다.** 첨부 `nx70v.rom`(12MiB)의 Palm OS 헤더(`0x20000`)와 제조사 `Sony Corporation`, Company ID `sony`, HAL ID `crdb`를 실제 확인했습니다. ROM 헤더 형식은 현행 파서와 호환되지만 NX70V 기종 선택 및 전용 주변장치 구현은 없습니다. 기존 `--tg50`으로 실행해도 NX70V 기종 지원이 되는 것은 아닙니다. 실행 환경에 SDL2 개발 파일이 없어 실제 네이티브 부팅은 수행하지 못했습니다.
