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
- Native build (`cp-uarm`, scratch copy with diagnostic logging only) and CPU execution with the TG50 peripherals (`--tg50`): **done** (2026-10-23). It does **not** reach the Palm OS screen; see below.
- Next: confirm how the stage-2 boot area below `0xA04D0000` is supposed to be filled (see "Open question"). Do not merge into TG50 until hardware differences and boot have been validated.

## Boot diagnostics (2026-10-23)

All results below come from a local diagnostic build (branch ring buffer, step log and MMU dump in `CPU.cpp`; not committed). The ROM was not modified; the "trimmed" image is a local copy only.

### ROM layout

| Range | Content |
| --- | --- |
| `0x00000`–`0x1FFFF` (128 KiB) | Not code. A repeated `0x4000`-byte structure plus a device record at `0x18000` with byte-reversed tags (`snum`, `time`, `sony`). It holds a per-device serial-like value: **never commit or publish this area or the ROM.** |
| `0x20000`– | Palm OS ROM (header above). Its first word is `b 0x754` (reset vector). |

- Raw image as NOR: the CPU executes the data area and stops with `Invalid instr 0x56514b30 at 0x0001800c`.
- Local trimmed copy (`rom[0x20000:]` padded with `0xFF` to 12 MiB): the code runs.

### Hardware checks during boot

| Check | Result |
| --- | --- |
| Sony microcontroller (`0x08000000`) | Same init sequence as TG50; ID register `0x100` read as `0x4741` and accepted |
| CPU ID | The code compares against `0x69052903`; the emulator reports PXA255 `0x69052D06`. Changing it locally did **not** change the result → not the cause (reverted) |
| Memory faults / CPU reset / sleep | None (reset vector `0x754` not re-entered, `RCSR` = 1, no sleep instruction) |

### Boot loop

The boot repeats "UC reset" (write `0xFF` to `0x08000210`) 60,000–90,000 times per minute without drawing anything.

Path (trimmed copy, offsets in the trimmed image):

1. Reset `0x0` → `0x754` → stage 1 at `0x1E55C`.
2. Stage 1 copies `0x9600` bytes from `0x1E55C` to RAM `0xA04D0000` (loop at `0x1ED00`), builds the page table at `0xA0004000` (VA `0x004xxxxx` → PA `0xA04xxxxx`), turns the MMU on, and continues at VA `0x004D0038`.
3. Stage 2 at VA `0x004D0AE8` executes `bl 0x004C7E30` (main init).
4. VA `0x004C7E30` → PA `0xA04C7E30` is **below the copied block and contains only zeros**. The CPU executes `0x00000000` (`andeq r0,r0,r0`, a no-op) for `0x8208` bytes, registers unchanged, and slides into `0x004D0038` (stage 2 start).
5. Stage 2 re-initialises the microcontroller ("UC reset") and repeats from step 3.

The function expected at `0x004C7E30` exists in the ROM: the same link offset maps it to trimmed offset `0x1638C` (`push {r3,r4,r5,lr}` right after a `bx lr`). So the image is linked as if the area before `0x1E55C` were also in RAM below `0xA04D0000`, but the stage-1 copy shown above does not put it there.

### Open question

Who fills RAM `0xA04C0000`–`0xA04CFFFF` on real hardware? Candidates (not verified):

- The real flash layout differs from this dump (for example a boot block or copy step outside the dumped 12 MiB, or a different start address), so trimming the first 128 KiB is not equivalent.
- Another routine (the ROM contains literals `0xA04C0000`/`0xA04C2000` near trimmed `0x15E34`) is supposed to copy that area and is skipped on this path.

Proposed next diagnostic (local only): find code that writes to `0xA04Cxxxx`, then test the first candidate by checking whether a `0x20000`-shifted mapping explains the link offset.

## 한국어

이 브랜치는 `feature/sony-tg50`에서 분기했으며 **NX70V 하드웨어 에뮬레이션은 아직 구현되지 않았습니다.** 첨부 `nx70v.rom`(12MiB)의 Palm OS 헤더(`0x20000`)와 제조사 `Sony Corporation`, Company ID `sony`, HAL ID `crdb`를 실제 확인했습니다. ROM 헤더 형식은 현행 파서와 호환되지만 NX70V 기종 선택 및 전용 주변장치 구현은 없습니다. 기존 `--tg50`으로 실행해도 NX70V 기종 지원이 되는 것은 아닙니다.

### 부팅 진단(2026-10-23, 진단 빌드·로컬 사본만 사용, ROM 수정 없음)

- **ROM 앞 128KiB(`0x00000`~`0x1FFFF`)는 코드가 아님**: 반복 구조 + `0x18000` 기기 기록(일련번호 성격 값 포함 → 이 영역과 ROM은 절대 커밋·공개 금지). 원본 그대로 실행하면 `0x0001800c`에서 잘못된 명령으로 멈춤. 앞 128KiB를 뺀 로컬 사본은 코드가 실행됨.
- **Sony 마이컴**: TG50과 같은 초기화 순서, ID `0x4741` 일치.
- **CPU ID**: 코드는 `0x69052903`을 기대, 에뮬레이터는 `0x69052D06`. 바꿔 봐도 결과 같음 → 원인 아님(되돌림).
- **반복 원인**: 1단계가 `0x1E55C`부터 `0x9600`바이트만 RAM `0xA04D0000`에 복사하고 MMU를 켬. 2단계가 `bl 0x004C7E30`(주 초기화)을 부르는데, 그 주소(PA `0xA04C7E30`)는 복사 범위 아래라 **0으로 비어 있음**. CPU가 0(아무 일 안 하는 명령)을 `0x8208`바이트 지나 2단계 시작 `0x004D0038`로 미끄러져 들어가고, 마이컴을 다시 리셋하며 무한 반복.
- 그 함수 자체는 ROM에 있음(로컬 사본 `0x1638C`). 즉 이미지는 `0x1E55C` 앞부분도 RAM에 있다고 가정하고 링크돼 있음.
- **남은 질문**: 실기에서는 무엇이 `0xA04C0000`~`0xA04CFFFF`를 채우는가? (가) 실제 플래시 배치가 이 덤프와 다름(앞 128KiB를 빼는 것이 실기와 다름), (나) 다른 복사 루틴(`0xA04C0000` 상수 근처)이 이 경로에서 건너뛰어짐. 둘 다 미확인.
