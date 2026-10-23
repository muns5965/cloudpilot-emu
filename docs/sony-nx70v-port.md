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
- Native build (`cp-uarm`, scratch copy with diagnostic logging only) and CPU execution with the TG50 peripherals (`--tg50`): **done** (2026-10-23). With a local composite image and four NX70V-specific settings it completes Setup (including pen calibration) and reaches the CLIÉ Launcher; see below.
- Next: see "Next" at the end of the boot diagnostics. Do not merge into TG50 until hardware differences and boot have been validated.

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

### Cause: the small boot ROM is missing from this dump

The ROM contains no other copy routine for `0xA04Cxxxx` (the only `0xA04C....` literals are a page-table mapping argument near trimmed `0x158BC` and a variable at `0xA04C1600`). Comparison with the TG50 NOR image:

| Location | TG50 | NX70V dump |
| --- | --- | --- |
| `0x0` | Small boot ROM (`PalmCard` header, `b 0x328`) | Device data area (not code) |
| `0x20000` | Main OS ROM header | Main OS ROM header |
| `0x9600`-byte loader copy to `0xA04D0000` | Two: small ROM (`0xCF8`) and inside the main ROM (`0x3F73C`) | One: inside the main ROM only |

The loader copy inside TG50's main ROM also calls below its copied block (`bl 0x004C7D7C`), exactly like NX70V. TG50 boots because it starts from the small ROM at `0x0`. So NX70V needs its own small boot ROM at `0x0`–`0x1FFFF` with the main ROM left at `0x20000`; trimming the first 128 KiB is not equivalent. The dump probably read a different area (the device data) instead of the boot block.

### Local test: TG50 small ROM + NX70V main ROM (not committed)

Composite image, local only: TG50 `0x0`–`0x17FFF` + NX70V `0x18000`– (the original files are unchanged). Purpose: confirm the cause, not a usable image.

- Stage 2 no longer loops; the NX70V main ROM runs its DAL at VA `0x20xxxxxx`. The screen stays blank.
- Without further changes it reboots about 4 times per second. Each cycle:
  - The Sony DAL boot step 2 (`0x209D4EF8`, called with 1, 2, 3 by the kernel) writes 1-byte registers of a device at `0x10000000` (CS4; offsets `0x24`–`0x44`, stride 4). Nothing is emulated there, so the write fails at PC `0x209D9C28` and the system reboots. **This is the cause of the reboot.**
  - A NULL-pointer write to `0x00000004` (PC `0x209D60F8`, Memory Stick routine) also appears, but TG50 hits the same fault once at the same routine (`0x20A86200`) and boots normally, so it is not the cause.
  - `0x14000000` is the same Memory Stick controller as TG50 (the TG50 port maps it at `0x14000000`; its init routine matches TG50 `0x20A862DC`).

### Local test: emulating `0x10000000` (scratch build only, not committed)

A diagnostic region at `0x10000000`–`0x1000FFFF` that accepts writes (enabled only with `NX70V_DIAG`, so TG50 runs are unchanged). It was later identified as the CF slot controller (below):

- No more reboot loop. **NX70V Palm OS reaches the Setup screen ("Setup 1 of 4").**
- Ignoring NOR flash writes was also tested and made no difference (reverted).

### Display: 320×480 (confirmed)

The ROM programs the PXA LCD controller for 320×480; TG50 programs 320×320 (writes logged in the scratch build):

| ROM | `LCCR1` | Pixels per line | `LCCR2` | Lines |
| --- | --- | --- | --- | --- |
| NX70V | `0x1F070D3F` | 320 | `0x080801DF` | 480 |
| TG50 | `0x1B070D3F` | 320 | `0x0808013F` | 320 |

With a scratch-only change that lets `--tg50` use the existing 320×480 display mode when `NX70V_DIAG` is set, the whole screen is visible: Setup 1 of 4 on top, and the NX70V virtual Graffiti area (Home/Menu/Calc/Find, `a`/`1` panes, brightness and keyboard icons) plus the status bar drawn by the ROM itself at the bottom.

### `0x10000000`: CF slot controller

The code that accesses it (`0x209A05xx`–`0x209A0Dxx`) is in the "Sony CfSlot Library" module (`CFSlot.c`, `CFLib.c`, "CF+ Card", "Comm Slot"). The base address comes from a configuration table; the driver handles up to 2 sockets. The DAL boot step that clears `MECR` (`0x48000014`, PXA expansion memory: CF/PCMCIA) fits this.

Register map inferred from the bit handling (8-bit registers, 4-byte stride):

| Offset | Use |
| --- | --- |
| `+0x10` | **Status (read).** Either bit of `0xC0` set = no card; both clear = card present. Bits 0–4 are passed on as separate status flags (likely ready, write-protect, battery; not confirmed) |
| `+0x24` | Control A: bit `0x80` power-on step, `0x20`/`0x10` socket option flags, `0x08` init |
| `+0x28` | Control B: bit `0x02` power-on step. The HAL idle loop (`0x20032688`, near `HwrInterruptsAries.c`) sets bit `0x01` before sleep and clears it after waking |
| `+0x2C` | Probably the interrupt mask (separate set/clear functions) |
| `+0x30`, `+0x34` | Bit set/clear control registers |
| `+0x38` | Bits are only ever set (interrupt-acknowledge style) |
| `+0x3C` | Bit `0x08` init, `0x10` socket mode switch |

Power-on (`0x209A0A1C`) for a 3.3 V setting (`0x21`): set `+0x28` bit `0x02`, wait 150 ms, set `+0x24` bit `0x80`, wait 50 ms. Power-off reverses this and waits 300 ms. The DAL boot step 2 (`0x209D9BD0`, mode 3) writes 0 to `+0x24`–`+0x44` and ORs 3 into `+0x40`.

Effect of the status value in the diagnostic CF slot model:

| `+0x10` returns | `+0x10` reads | `+0x24` writes | `+0x28` writes | Result |
| --- | --- | --- | --- | --- |
| `0x00` | 300 | 11 | 4 | Driver treats it as a card present and runs power-on; boots to Setup |
| `0xC0` | 2 | 2 | 1 | "No card" path; boots to Setup, same screen |

So a minimal model is: `+0x10` returns `0xC0`, other registers accept writes.

### Touch: works after clearing the HOLD switch

- The digitizer is the same AD7873 as TG50 (same SSP commands), and the pen interrupt is the same GPIO9: the HAL interrupt handler tables of both ROMs (dumped in the scratch build) register the same HAL pen handler on GPIO9 (TG50 `0x20030F4C`, NX70V `0x20030F78`). An earlier note that GPIO9 is the remote-control interrupt on NX70V was wrong.
- The "Sony Sys Library" turns the pen interrupt off when the switch state is `Hold_By_HoldKey` (other states: `Hold_OFF`, `Hold_By_FlipClose`). The state is polled from the Sony microcontroller port A (`0x08000000`); **bit `0x10` = HOLD switch on**. The TG50 microcontroller model returns `0xF010`, so NX70V saw HOLD on and disabled touch.
- With bit `0x10` cleared, taps reach the HAL. Of five coordinate mappings tried, only **X and Y both inverted** (relative to TG50) passed pen calibration. The 480 lines are currently scaled into the TG50 raw range (`×2/3`).
- Result: Setup 1–4 including calibration, then the **CLIÉ Launcher** (320×480, input area collapsed). Tapping the status bar opens the virtual Graffiti area.

### NX70V settings (implemented as `--nx70v`)

First found with scratch-only `NX70V_DIAG` changes, then implemented in the branch as the `--nx70v` mode (`deviceTypeSonyNX70V`, sharing the TG50 peripherals with a variant flag). `--boot-rom <TG50 NOR>` copies the TG50 boot ROM (`0x0`–`0x17FFF`) over the device data in memory only. Verified 2026-10-23: Setup, pen calibration and the CLIÉ Launcher; TG50 `--tg50` Setup/calibration unchanged.

| # | Setting | Value |
| --- | --- | --- |
| 1 | CF slot controller at `0x10000000` | Minimal model: `+0x10` returns `0xC0` (no card), other registers accept writes |
| 2 | Display | 320×480 |
| 3 | Sony microcontroller port A (`0x08000000`) | Bit `0x10` (HOLD switch) cleared |
| 4 | Pen coordinates | X and Y inverted versus TG50; 480 lines scaled into the TG50 raw range |

### Next

1. Best: obtain the real NX70V small boot ROM (dump flash `0x0`–`0x1FFFF` again from the device).
2. CF cards, RTC alarm wake.

Session files work (shared TG50 change): saved after typing a memo and resumed with typing, saved while asleep and woken with Esc. A NX70V session stores the running ROM image including the TG50 boot ROM, so it must not be shared.

### `--nx70v` usage test (2026-10-23)

| Item | Result |
| --- | --- |
| Hard buttons | F2 Address, F3 To Do, F4 Memo launch; F1 shows no change (NX70V button 1 not checked) |
| Keyboard | ROM key table equals TG50's (only `,`/`;` positions differ); a–z correct in Memo Pad. Fast typing used to drop keys (held < ~60 ms, as on TG50); fixed by keeping each key down for at least 3 event ticks (~100 ms), verified with 40 ms per key and repeated letters |
| Apps | CLIÉ Album ("No Album"), Calculator (`1 + 3 =` by taps → 4), Memo Pad |
| Power key (Esc) | Sleep and wake work (shared TG50 fix, see the TG50 notes "Sleep and resume"); two cycles, then Memo Pad typing |

## 한국어

이 브랜치는 `feature/sony-tg50`에서 분기했으며 **NX70V 하드웨어 에뮬레이션은 아직 구현되지 않았습니다.** 첨부 `nx70v.rom`(12MiB)의 Palm OS 헤더(`0x20000`)와 제조사 `Sony Corporation`, Company ID `sony`, HAL ID `crdb`를 실제 확인했습니다. ROM 헤더 형식은 현행 파서와 호환되지만 NX70V 기종 선택 및 전용 주변장치 구현은 없습니다. 기존 `--tg50`으로 실행해도 NX70V 기종 지원이 되는 것은 아닙니다.

### 부팅 진단(2026-10-23, 진단 빌드·로컬 사본만 사용, ROM 수정 없음)

- **ROM 앞 128KiB(`0x00000`~`0x1FFFF`)는 코드가 아님**: 반복 구조 + `0x18000` 기기 기록(일련번호 성격 값 포함 → 이 영역과 ROM은 절대 커밋·공개 금지). 원본 그대로 실행하면 `0x0001800c`에서 잘못된 명령으로 멈춤. 앞 128KiB를 뺀 로컬 사본은 코드가 실행됨.
- **Sony 마이컴**: TG50과 같은 초기화 순서, ID `0x4741` 일치.
- **CPU ID**: 코드는 `0x69052903`을 기대, 에뮬레이터는 `0x69052D06`. 바꿔 봐도 결과 같음 → 원인 아님(되돌림).
- **반복 원인**: 1단계가 `0x1E55C`부터 `0x9600`바이트만 RAM `0xA04D0000`에 복사하고 MMU를 켬. 2단계가 `bl 0x004C7E30`(주 초기화)을 부르는데, 그 주소(PA `0xA04C7E30`)는 복사 범위 아래라 **0으로 비어 있음**. CPU가 0(아무 일 안 하는 명령)을 `0x8208`바이트 지나 2단계 시작 `0x004D0038`로 미끄러져 들어가고, 마이컴을 다시 리셋하며 무한 반복.
- 그 함수 자체는 ROM에 있음(로컬 사본 `0x1638C`). 즉 이미지는 `0x1E55C` 앞부분도 RAM에 있다고 가정하고 링크돼 있음.
- **남은 질문**: 실기에서는 무엇이 `0xA04C0000`~`0xA04CFFFF`를 채우는가? (가) 실제 플래시 배치가 이 덤프와 다름(앞 128KiB를 빼는 것이 실기와 다름), (나) 다른 복사 루틴(`0xA04C0000` 상수 근처)이 이 경로에서 건너뛰어짐. 둘 다 미확인.
- **원인 확정(가)**: 이 덤프에는 `0x0`의 **작은 부트 ROM이 없음**. TG50은 `0x0`에 작은 ROM(PalmCard 헤더, `b 0x328`)과 `0x20000`에 본 ROM이 있고, 본 ROM 안의 부트 사본은 NX70V와 똑같이 복사 범위 아래(`bl 0x004C7D7C`)를 부름. TG50은 작은 ROM으로 시작해서 정상 부팅. `0xA04C....`를 채우는 다른 복사 루틴(나)은 없음.
- **로컬 조합 시험(커밋 안 함)**: TG50 `0x0`~`0x17FFF` + NX70V `0x18000`~. 2단계 반복은 사라지고 NX70V 본 ROM의 DAL(`0x20xxxxxx`)까지 실행됨. 화면은 빈 채로 초당 약 4회 재부팅.
  - **재부팅 원인**: Sony DAL 초기화 2단계가 `0x10000000`(CS4) 장치의 1바이트 레지스터(`+0x24`~`+0x44`)에 쓰는데, 에뮬레이트된 장치가 없어 쓰기 실패 → 재부팅.
  - 주소 `0x4` NULL 쓰기 실패도 보이지만 TG50도 같은 루틴에서 한 번 똑같이 실패하고 정상 부팅 → 원인 아님.
  - `0x14000000`은 TG50과 같은 메모리스틱 컨트롤러(포트도 `0x14000000`에 매핑) → 원인 아님.
- **로컬 시험(진단 빌드, 커밋 안 함)**: `0x10000000`에 쓰기를 받아 주는 진단 영역(`NX70V_DIAG` 있을 때만, 나중에 CF 슬롯 컨트롤러로 확인)을 두자 재부팅이 사라지고 **NX70V Palm OS가 Setup 화면(1 of 4)까지 부팅**. 플래시 쓰기 무시도 시험했으나 영향 없어 되돌림.
- **화면 320×480 확인**: ROM이 LCD를 320×480으로 설정(`LCCR1` `0x1F070D3F` = 320, `LCCR2` `0x080801DF` = 480). TG50은 320×320(`0x0808013F`). 진단 빌드에서 320×480 모드로 띄우니 Setup 화면 아래에 NX70V 가상 그래피티 영역과 상태 표시줄이 ROM 자체로 그려짐.
- **`0x10000000` = CF 슬롯 컨트롤러**: 접근 코드가 "Sony CfSlot Library" 모듈(`CFSlot.c`, `CFLib.c`) 안에 있음. 슬롯 최대 2개.
  - `+0x10` 상태(읽기): `0xC0` 비트 중 하나라도 1이면 카드 없음. 비트 0~4는 별도 상태 플래그(준비·쓰기 금지·배터리로 추정).
  - `+0x24`·`+0x28` 전원 제어: 3.3V 켜기 = `+0x28` 비트 `0x02` → 150ms → `+0x24` 비트 `0x80` → 50ms. 끄기는 반대 순서 후 300ms. HAL 절전 루프가 절전 전후로 `+0x28` 비트 `0x01`을 켜고 끔.
  - `+0x2C` 인터럽트 마스크 추정, `+0x30`·`+0x34`·`+0x38`·`+0x3C` 제어(상세는 영문 표).
  - 진단용 CF 슬롯 모델의 `+0x10` 값 비교: `0x00`이면 카드 있음으로 보고 전원 켜기 진행(`+0x10` 300회 읽기), `0xC0`이면 카드 없음 경로(2회 읽기). 둘 다 Setup까지 부팅. **최소 모델은 `+0x10` = `0xC0`, 나머지 쓰기 수용.**
- **터치: HOLD 스위치 비트를 끄자 동작**
  - 터치 칩(AD7873)과 펜 인터럽트(GPIO9) 모두 TG50과 같음. 두 ROM의 HAL 인터럽트 처리 함수 표를 덤프하니 GPIO9에 같은 HAL 펜 처리 함수가 등록돼 있음(TG50 `0x20030F4C`, NX70V `0x20030F78`). 앞서 적은 "GPIO9 = 리모컨 인터럽트"는 틀림.
  - "Sony Sys Library"가 스위치 상태가 `Hold_By_HoldKey`이면 펜 인터럽트를 끔(다른 상태: `Hold_OFF`, `Hold_By_FlipClose`). 상태는 Sony 마이컴 포트 A(`0x08000000`)에서 읽고 **비트 `0x10` = HOLD**. TG50 마이컴 모델이 `0xF010`을 돌려줘 NX70V가 HOLD 켜짐으로 판단.
  - 비트 `0x10`을 지우자 탭이 전달됨. 좌표 변환 5가지 중 **X·Y 모두 반전**(TG50 기준)만 펜 보정 통과. 480줄은 TG50 원시값 범위로 줄여 넣음(`×2/3`).
  - 결과: 펜 보정 포함 Setup 1~4 완료 → **CLIÉ 런처**(320×480, 입력 영역 접힘). 상태 표시줄 탭으로 가상 그래피티 영역 열림.
- **NX70V 설정 4가지**(처음엔 작업 사본 `NX70V_DIAG`로 찾고, 이후 `--nx70v` 모드로 브랜치에 구현. `--boot-rom <TG50 NOR>`이 TG50 부트 ROM을 메모리에서만 덮어씀. 2026-10-23 Setup·펜 보정·CLIÉ 런처 확인, TG50 `--tg50` 보정 영향 없음):
  1. `0x10000000` CF 슬롯 컨트롤러 최소 모델: `+0x10` = `0xC0`(카드 없음), 나머지 쓰기 수용
  2. 화면 320×480
  3. Sony 마이컴 포트 A 비트 `0x10`(HOLD) 끄기
  4. 펜 좌표 X·Y 반전, 480줄을 TG50 원시값 범위로 축소
- **`--nx70v` 사용 시험(2026-10-23)**: F2 주소록·F3 할 일·F4 메모장 실행(F1은 변화 없음). 키 표는 TG50과 같고 a~z 정확. 빠른 입력 누락(키를 약 60ms보다 짧게 누르면 놓침, TG50과 같음)은 키를 최소 3틱(약 100ms) 눌린 상태로 유지하도록 고쳐 해결(키당 40ms·반복 글자 확인). CLIÉ 앨범·계산기(탭으로 `1 + 3 =` → 4)·메모장 동작. 전원 키(Esc)는 변화 없음(TG50과 같음).
- **절전·복귀**: TG50과 공통 수정으로 동작(Esc로 절전·복귀 2회, 복귀 후 메모장 입력 확인).
- **세션 파일**: TG50과 공통 수정으로 동작(메모 입력 후 저장·재개, 절전 중 저장 후 Esc로 복귀). NX70V 세션에는 TG50 부트 ROM이 들어간 ROM 이미지가 저장되므로 공유 금지.
- **다음**: 실기에서 플래시 `0x0`~`0x1FFFF` 재덤프가 가장 확실. CF 카드, 알람(RTC)으로 깨우기.
