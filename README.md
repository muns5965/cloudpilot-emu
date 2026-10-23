# CloudpilotEmu — Sony CLIÉ PEG-NX70V investigation branch

> **Experimental.** The native build has an `--nx70v` mode: NX70V Palm OS completes Setup (with pen calibration) and reaches the CLIÉ Launcher. Because the known dump lacks the small boot ROM, it still borrows the boot ROM of a TG50 NOR image at run time (`--boot-rom`). This branch was created from [`feature/sony-tg50`](https://github.com/muns5965/cloudpilot-emu/tree/feature/sony-tg50) and uses its TG50 code (the `--tg50` path and the TG50 device models) as the base. NX70V reuses the TG50 peripheral code with four NX70V differences.

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

## NX70V differences in `--nx70v`

| # | Setting | Value |
| --- | --- | --- |
| 1 | CF slot controller at `0x10000000` | Minimal model: `+0x10` = `0xC0` (no card), other registers accept writes |
| 2 | Display | 320×480 |
| 3 | Sony microcontroller port A | Bit `0x10` (HOLD switch) cleared |
| 4 | Pen coordinates | X and Y inverted versus TG50; 480 lines scaled into the TG50 raw range |

Implemented in `src/uarm/uarm/deviceSonyTG50.cpp` (variant flag), `sony_tg50_hardware.h` and `device_configuration.cpp`. Not yet emulated: the CF card itself, sleep/resume, savestates, the flip and HOLD switches as user controls.

## Next

1. Best fix for booting: a dump of the device's own small boot ROM (flash `0x0`–`0x1FFFF`). `--nx70v` uses it automatically when the ROM has a header at `0x0`.
2. Check apps, keyboard and sleep on NX70V; savestates.

## Running

Native build only (see the TG50 branch README for building):

```sh
./src/uarm/cp-uarm --nx70v --boot-rom /path/to/TG50-ROM.bin /path/to/nx70v.rom
```

- Without `--boot-rom`, `--nx70v` stops with a message, because the known dump has device data at `0x0`. Starting it with `--tg50` stops at `Invalid instr 0x56514b30 at 0x0001800c`.
- Window: 320×480. The pen calibration targets are at the screen corners; tap them in Setup.
- Check a ROM header: `python3 tools/probe-nx70v-rom.py /path/to/nx70v.rom`. For building and running TG50, see the [TG50 branch README](https://github.com/muns5965/cloudpilot-emu/blob/feature/sony-tg50/README.md).

**Never commit or publish** NX70V or TG50 ROM files or the device data area at `0x18000`.

---

# CloudpilotEmu — Sony CLIÉ PEG-NX70V 조사 브랜치

> **실험 단계입니다.** 네이티브 빌드에 `--nx70v` 모드가 있으며, NX70V Palm OS가 펜 보정을 포함한 Setup을 마치고 CLIÉ 런처까지 들어갑니다. 알려진 덤프에 작은 부트 ROM이 없어서, 실행할 때 TG50 NOR 이미지의 부트 ROM을 빌려 씁니다(`--boot-rom`). 이 브랜치는 [`feature/sony-tg50`](https://github.com/muns5965/cloudpilot-emu/tree/feature/sony-tg50)에서 갈라져 나왔고, 그 TG50 코드(`--tg50` 실행 경로와 TG50 장치 모델)를 바탕으로 씁니다. NX70V는 TG50 장치 코드를 그대로 쓰고 NX70V 차이 4가지만 따로 둡니다.

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

## `--nx70v`의 NX70V 차이

| # | 설정 | 값 |
| --- | --- | --- |
| 1 | `0x10000000` CF 슬롯 컨트롤러 | 최소 모델: `+0x10` = `0xC0`(카드 없음), 나머지 쓰기 수용 |
| 2 | 화면 | 320×480 |
| 3 | Sony 마이컴 포트 A | 비트 `0x10`(HOLD 스위치) 끄기 |
| 4 | 펜 좌표 | TG50 대비 X·Y 반전, 480줄을 TG50 원시값 범위로 축소 |

`src/uarm/uarm/deviceSonyTG50.cpp`(기종 구분 플래그), `sony_tg50_hardware.h`, `device_configuration.cpp`에 구현. 아직 없는 것: CF 카드 자체, 절전·복귀, 상태 저장, 접힘·HOLD 스위치 조작.

## 다음 단계

1. 부팅 문제의 가장 확실한 해결: 실기에서 작은 부트 ROM(플래시 `0x0`~`0x1FFFF`)을 다시 덤프. ROM의 `0x0`에 헤더가 있으면 `--nx70v`가 그대로 사용.
2. NX70V에서 앱·키보드·절전 확인, 상태 저장.

## 실행

네이티브 빌드 전용입니다(빌드 방법은 TG50 브랜치 README 참고).

```sh
./src/uarm/cp-uarm --nx70v --boot-rom /path/to/TG50-ROM.bin /path/to/nx70v.rom
```

- `--boot-rom` 없이 실행하면 안내 메시지와 함께 멈춥니다. 알려진 덤프의 `0x0`이 기기 데이터이기 때문입니다. `--tg50`으로 실행하면 `Invalid instr 0x56514b30 at 0x0001800c`에서 멈춥니다.
- 창 크기 320×480. Setup의 펜 보정 표적은 화면 모서리에 나옵니다.
- ROM 헤더 확인: `python3 tools/probe-nx70v-rom.py /path/to/nx70v.rom`. TG50 빌드·실행 방법은 [TG50 브랜치 README](https://github.com/muns5965/cloudpilot-emu/blob/feature/sony-tg50/README.md)를 보세요.

NX70V·TG50 ROM 파일과 `0x18000`의 기기 데이터 영역은 **절대 커밋·공개하지 않습니다.**
