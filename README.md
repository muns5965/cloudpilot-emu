# CloudpilotEmu — Sony CLIÉ PEG-NX70V investigation branch

> **Experimental.** NX70V also runs in the browser at **[muns5965.github.io/cloudpilot-emu](https://muns5965.github.io/cloudpilot-emu/)** (see [Browser](#browser); TG50 runs there too). The native build has an `--nx70v` mode: NX70V Palm OS completes Setup (with pen calibration) and reaches the CLIÉ Launcher. Because the known dump lacks the small boot ROM, it still borrows the boot ROM of a TG50 NOR image at run time (`--boot-rom`). This branch was created from [`feature/sony-tg50`](https://github.com/muns5965/cloudpilot-emu/tree/feature/sony-tg50) and uses its TG50 code (the `--tg50` path and the TG50 device models) as the base. NX70V reuses the TG50 peripheral code with four NX70V differences.

[**NX70V notes**](docs/sony-nx70v-port.md) · [**TG50 branch README**](https://github.com/muns5965/cloudpilot-emu/blob/feature/sony-tg50/README.md) · [**TG50 technical notes**](docs/sony-tg50-port.md) · [**Original CloudpilotEmu README**](doc/README-upstream.md) · [**Upstream repository**](https://github.com/cloudpilot-emu/cloudpilot-emu)

## Status (2026-10-23)

NX70V ROM: Palm OS 5.0, Sony, HAL ID `crdb`, 12 MiB. Not included in this repository.

| Item | Status | Notes |
| --- | --- | --- |
| ROM header | **Parsed** | Header at `0x20000`; checked by `tools/probe-nx70v-rom.py` |
| ROM as dumped | **Does not boot** | `0x0`–`0x1FFFF` is device data (with a serial-like value), not the small boot ROM that TG50 has at `0x0`. The main ROM's own loader copy then calls code that was never copied and loops |
| `--nx70v --boot-rom <TG50 NOR>` | **Setup with pen calibration → CLIÉ Launcher** | The TG50 boot ROM (`0x0`–`0x17FFF`) is copied over the device data in memory only; no file is changed |
| `0x10000000` (CS4) | **CF slot controller** (Sony CfSlot Library) | Unemulated writes caused the reboot loop. Minimal model: status `+0x10` = `0xC0` (no card), other registers accept writes |
| `0x14000000` | Same Memory Stick controller as TG50 | |
| Sony microcontroller | Same as TG50 (ID `0x4741`) | |
| Display | **320×480** (TG50: 320×320) | The ROM draws its own virtual Graffiti area and status bar |
| Touch | **Works** | Same AD7873 chip and pen interrupt (GPIO9) as TG50. It was blocked because the TG50 microcontroller model reports the NX70V HOLD switch (port A bit `0x10`) as on. X and Y are inverted versus TG50 |
| Keyboard | **Works** | Same key table as TG50; fast typing (40 ms per key, repeated letters) is correct since keys are held for at least ~100 ms. PC digit keys are not mapped (Blue + top row) |
| Hard buttons | **Works**: F1 Date Book, F2 Address, F3 To Do, F4 Memo (factory setting per the NX70V manual, p. 45) | The first press after boot used to be lost (shared with TG50; fixed by swapping the controller's edge-detect registers) |
| Apps | CLIÉ Album, Calculator, Memo Pad tested | Calculator `1 + 3 =` by taps gives 4 |
| Power key (Esc), sleep/resume | **Works** | Shared with TG50: Esc sleeps (screen off), Esc again wakes; keyboard works after resume |
| Browser | **Boots** (user test on a phone): Date Book, virtual Graffiti area; typing not yet reported | Device `PEG-NX70V`, TG50 ROM as boot ROM in the session settings, on-screen keyboard with the NX70V layout |
| Session files | **Works** | Shared with TG50: `save-session <file>`, then start the emulator with the session file (no `--nx70v`/`--boot-rom` needed); also while asleep |

## NX70V differences in `--nx70v`

| # | Setting | Value |
| --- | --- | --- |
| 1 | CF slot controller at `0x10000000` | Minimal model: `+0x10` = `0xC0` (no card), other registers accept writes |
| 2 | Display | 320×480 |
| 3 | Sony microcontroller port A | Bit `0x10` (HOLD switch) cleared |
| 4 | Pen coordinates | X and Y inverted versus TG50; 480 lines scaled into the TG50 raw range |

Implemented in `src/uarm/uarm/deviceSonyTG50.cpp` (variant flag), `sony_tg50_hardware.h` and `device_configuration.cpp`. Not yet emulated: the CF card itself (per the manual the slot only takes a CF network card such as PEGA-WL100; a dummy card ships in it), the flip and HOLD switches as user controls.

## Next

1. Booting borrows the TG50 boot ROM (no NX70V device is available for a new dump). A dump that includes the device's own boot ROM (header at `0x0`) is used as is, natively and in the browser.
2. RTC alarm wake; CF network card (low priority, the slot takes no memory cards).
3. Browser: hand test (boot, keyboard incl. Fn/Alt, sessions).

## Browser

1. Open **https://muns5965.github.io/cloudpilot-emu/**. ROM files stay in your browser.
2. **Sessions → +**, choose the NX70V ROM. The device shows as **Sony PEG-NX70V** (320×480, 17 MB).
3. In the settings dialog, tap **Boot ROM (TG50 ROM, required)** and choose a TG50 NOR ROM. Its first 0x18000 bytes are copied over the start of the NX70V ROM in this browser only.
4. Save, select the session and press ▶. Setup and calibration as on the device.

On-screen keyboard: NX70V layout (positions measured on a device photo; keys drawn as plain shapes). Tap **Fn**, **Alt**, **Shift** or **Ctrl** to latch it for the next key. Blue characters: Fn, purple: Alt, as the European NX70V ROM types them. PC keys are the same as for the TG50 (F7 = Fn, F9 = Alt).

Do not export or share NX70V sessions: they contain the TG50 boot ROM.

## Running (native)

See the TG50 branch README for building:

```sh
./src/uarm/cp-uarm --nx70v --boot-rom /path/to/TG50-ROM.bin /path/to/nx70v.rom
```

- Without `--boot-rom`, `--nx70v` stops with a message, because the known dump has device data at `0x0`. Starting it with `--tg50` stops at `Invalid instr 0x56514b30 at 0x0001800c`.
- Window: 320×480. The pen calibration targets are at the screen corners; tap them in Setup.
- Check a ROM header: `python3 tools/probe-nx70v-rom.py /path/to/nx70v.rom`. For building and running TG50, see the [TG50 branch README](https://github.com/muns5965/cloudpilot-emu/blob/feature/sony-tg50/README.md).

**Never commit or publish** NX70V or TG50 ROM files, the device data area at `0x18000`, or NX70V session files (a session stores the whole ROM image as running, including the TG50 boot ROM copied in by `--boot-rom`).

---

# CloudpilotEmu — Sony CLIÉ PEG-NX70V 조사 브랜치

> **실험 단계입니다.** NX70V는 브라우저 **[muns5965.github.io/cloudpilot-emu](https://muns5965.github.io/cloudpilot-emu/)** 에서도 실행됩니다([브라우저 실행](#브라우저-실행) 참고, TG50도 같은 주소). 네이티브 빌드에 `--nx70v` 모드가 있으며, NX70V Palm OS가 펜 보정을 포함한 Setup을 마치고 CLIÉ 런처까지 들어갑니다. 알려진 덤프에 작은 부트 ROM이 없어서, 실행할 때 TG50 NOR 이미지의 부트 ROM을 빌려 씁니다(`--boot-rom`). 이 브랜치는 [`feature/sony-tg50`](https://github.com/muns5965/cloudpilot-emu/tree/feature/sony-tg50)에서 갈라져 나왔고, 그 TG50 코드(`--tg50` 실행 경로와 TG50 장치 모델)를 바탕으로 씁니다. NX70V는 TG50 장치 코드를 그대로 쓰고 NX70V 차이 4가지만 따로 둡니다.

[**NX70V 조사 기록(영문·한국어)**](docs/sony-nx70v-port.md) · [**TG50 브랜치 README**](https://github.com/muns5965/cloudpilot-emu/blob/feature/sony-tg50/README.md) · [**TG50 기술 문서(영문)**](docs/sony-tg50-port.md) · [**CloudpilotEmu 기존 README(영문)**](doc/README-upstream.md) · [**원본 저장소**](https://github.com/cloudpilot-emu/cloudpilot-emu)

## 현황(2026-10-23)

NX70V ROM: Palm OS 5.0, Sony, HAL ID `crdb`, 12MiB. 저장소에 포함하지 않습니다.

| 항목 | 상태 | 비고 |
| --- | --- | --- |
| ROM 헤더 | **인식** | `0x20000`에 헤더. `tools/probe-nx70v-rom.py`로 확인 |
| 덤프 그대로 | **부팅 안 됨** | `0x0`~`0x1FFFF`가 기기 데이터(일련번호 성격 값 포함)이고, TG50의 `0x0`에 있는 작은 부트 ROM이 없음. 본 ROM 안의 부트 사본이 복사되지 않은 코드를 불러 무한 반복 |
| `--nx70v --boot-rom <TG50 NOR>` | **펜 보정 포함 Setup → CLIÉ 런처** | TG50 부트 ROM(`0x0`~`0x17FFF`)을 메모리에서만 기기 데이터 위에 덮어씀. 파일은 바꾸지 않음 |
| `0x10000000`(CS4) | **CF 슬롯 컨트롤러**(Sony CfSlot Library) | 에뮬레이트되지 않은 쓰기가 재부팅 반복의 원인. 최소 모델: 상태 `+0x10` = `0xC0`(카드 없음), 나머지 쓰기 수용 |
| `0x14000000` | TG50과 같은 메모리스틱 컨트롤러 | |
| Sony 마이컴 | TG50과 같음(ID `0x4741`) | |
| 화면 | **320×480**(TG50은 320×320) | ROM이 가상 그래피티 영역과 상태 표시줄을 직접 그림 |
| 터치 | **동작** | 터치 칩(AD7873)과 펜 인터럽트(GPIO9) 모두 TG50과 같음. TG50 마이컴 모델이 NX70V HOLD 스위치(포트 A 비트 `0x10`)를 켜짐으로 알려 막혀 있었음. X·Y가 TG50과 반대 |
| 키보드 | **동작** | 키 표가 TG50과 같음. 키를 최소 약 100ms 눌린 상태로 유지하도록 고쳐 빠른 입력(키당 40ms, 반복 글자)도 정확. PC 숫자 키 미연결(Blue + 윗줄) |
| 하드 버튼 | **동작**: F1 일정(Date Book), F2 주소록, F3 할 일, F4 메모장(NX70V 매뉴얼 45쪽의 기본 설정) | 부팅 후 첫 누름이 무시되던 문제(TG50과 공통)는 마이컴 엣지 감지 레지스터를 바로잡아 해결 |
| 앱 | CLIÉ 앨범, 계산기, 메모장 시험 | 계산기에서 탭으로 `1 + 3 =` → 4 |
| 전원 키(Esc), 절전·복귀 | **동작** | TG50과 공통: Esc로 절전(화면 꺼짐), 다시 Esc로 복귀. 복귀 후 키보드 정상 |
| 브라우저 | **부팅 확인**(휴대폰 사용자 시험): 일정(Date Book), 가상 그래피티 영역. 키 입력 결과는 아직 | 기종 `PEG-NX70V`, 세션 설정에서 TG50 ROM을 부트 ROM으로 지정, NX70V 배열 화면 키보드 |
| 세션 파일 | **동작** | TG50과 공통: `save-session <파일>` 후 세션 파일로 실행(`--nx70v`·`--boot-rom` 불필요). 절전 중 저장도 가능 |

## `--nx70v`의 NX70V 차이

| # | 설정 | 값 |
| --- | --- | --- |
| 1 | `0x10000000` CF 슬롯 컨트롤러 | 최소 모델: `+0x10` = `0xC0`(카드 없음), 나머지 쓰기 수용 |
| 2 | 화면 | 320×480 |
| 3 | Sony 마이컴 포트 A | 비트 `0x10`(HOLD 스위치) 끄기 |
| 4 | 펜 좌표 | TG50 대비 X·Y 반전, 480줄을 TG50 원시값 범위로 축소 |

`src/uarm/uarm/deviceSonyTG50.cpp`(기종 구분 플래그), `sony_tg50_hardware.h`, `device_configuration.cpp`에 구현. 아직 없는 것: CF 카드 자체(매뉴얼상 이 슬롯은 PEGA-WL100 같은 CF 네트워크 카드 전용이고 출고 때 빈 보호 카드가 꽂혀 있음), 접힘·HOLD 스위치 조작.

## 다음 단계

1. 부팅은 TG50 부트 ROM을 빌려 씀(새로 덤프할 NX70V 실기가 없음). 기기 자체 부트 ROM이 들어 있는 덤프(`0x0`에 헤더)가 있으면 네이티브·브라우저 모두 그대로 사용.
2. 알람(RTC)으로 깨우기. CF 네트워크 카드(메모리 카드를 받지 않는 슬롯이라 우선순위 낮음).
3. 브라우저 직접 시험(부팅, Fn·Alt 포함 키보드, 세션).

## 브라우저 실행

1. **https://muns5965.github.io/cloudpilot-emu/** 를 엽니다. ROM 파일은 브라우저 안에서만 쓰입니다.
2. **Sessions → +** 에서 NX70V ROM을 고릅니다. 기종 **Sony PEG-NX70V**(320×480, 17MB)로 표시됩니다.
3. 설정 창의 **Boot ROM (TG50 ROM, required)** 칸을 눌러 TG50 NOR ROM을 고릅니다. 그 앞 0x18000바이트를 이 브라우저 안에서만 NX70V ROM 앞부분에 덮어씁니다.
4. 저장한 뒤 세션을 선택하고 ▶를 누릅니다. Setup과 펜 보정은 실기와 같습니다.

화면 키보드: NX70V 배열(실기 사진에서 위치를 측정, 키는 단순 도형으로 그림). **Fn**, **Alt**, **Shift**, **Ctrl**을 탭하면 다음 키 한 번에 적용됩니다. 파란 글자는 Fn, 보라 글자는 Alt로, 유럽판 NX70V ROM이 실제로 내는 글자입니다. PC 키는 TG50과 같습니다(F7 = Fn, F9 = Alt).

NX70V 세션은 TG50 부트 ROM을 포함하므로 내보내거나 공유하지 않습니다.

## 네이티브 실행

빌드 방법은 TG50 브랜치 README를 참고합니다.

```sh
./src/uarm/cp-uarm --nx70v --boot-rom /path/to/TG50-ROM.bin /path/to/nx70v.rom
```

- `--boot-rom` 없이 실행하면 안내 메시지와 함께 멈춥니다. 알려진 덤프의 `0x0`이 기기 데이터이기 때문입니다. `--tg50`으로 실행하면 `Invalid instr 0x56514b30 at 0x0001800c`에서 멈춥니다.
- 창 크기 320×480. Setup의 펜 보정 표적은 화면 모서리에 나옵니다.
- ROM 헤더 확인: `python3 tools/probe-nx70v-rom.py /path/to/nx70v.rom`. TG50 빌드·실행 방법은 [TG50 브랜치 README](https://github.com/muns5965/cloudpilot-emu/blob/feature/sony-tg50/README.md)를 보세요.

NX70V·TG50 ROM 파일, `0x18000`의 기기 데이터 영역, NX70V 세션 파일은 **절대 커밋·공개하지 않습니다**(세션은 `--boot-rom`으로 덮어쓴 TG50 부트 ROM을 포함한 실행 중 ROM 전체를 저장함).
