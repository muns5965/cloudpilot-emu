# CloudpilotEmu — Sony CLIÉ PEG-NX70V investigation branch

> **Investigation only. There is no NX70V device in the code yet.** In a local diagnostic build, NX70V Palm OS now completes Setup and reaches the CLIÉ Launcher. This branch was created from [`feature/sony-tg50`](https://github.com/muns5965/cloudpilot-emu/tree/feature/sony-tg50) and uses its TG50 code (the `--tg50` path and the TG50 device models) as the base. All boot results below come from a local diagnostic build that is not committed.

[**NX70V notes**](docs/sony-nx70v-port.md) · [**TG50 branch README**](https://github.com/muns5965/cloudpilot-emu/blob/feature/sony-tg50/README.md) · [**TG50 technical notes**](docs/sony-tg50-port.md) · [**Original CloudpilotEmu README**](doc/README-upstream.md) · [**Upstream repository**](https://github.com/cloudpilot-emu/cloudpilot-emu)

## Status (2026-10-23)

NX70V ROM: Palm OS 5.0, Sony, HAL ID `crdb`, 12 MiB. Not included in this repository.

| Item | Status | Notes |
| --- | --- | --- |
| ROM header | **Parsed** | Header at `0x20000`; checked by `tools/probe-nx70v-rom.py` |
| ROM as dumped | **Does not boot** | `0x0`–`0x1FFFF` is device data (with a serial-like value), not the small boot ROM that TG50 has at `0x0`. The main ROM's own loader copy then calls code that was never copied and loops |
| Local composite image (TG50 small ROM + NX70V main ROM) | **Setup with pen calibration → CLIÉ Launcher** | Local file only, never committed. Needs the four NX70V settings below |
| `0x10000000` (CS4) | **CF slot controller** (Sony CfSlot Library) | Unemulated writes caused the reboot loop. Minimal model: status `+0x10` = `0xC0` (no card), other registers accept writes |
| `0x14000000` | Same Memory Stick controller as TG50 | |
| Sony microcontroller | Same as TG50 (ID `0x4741`) | |
| Display | **320×480** (TG50: 320×320) | The ROM draws its own virtual Graffiti area and status bar |
| Touch | **Works** (diagnostic build) | Same AD7873 chip and pen interrupt (GPIO9) as TG50. It was blocked because the TG50 microcontroller model reports the NX70V HOLD switch (port A bit `0x10`) as on. X and Y are inverted versus TG50 |

## NX70V settings needed (diagnostic build)

| # | Setting | Value |
| --- | --- | --- |
| 1 | CF slot controller at `0x10000000` | Minimal model: `+0x10` = `0xC0` (no card), other registers accept writes |
| 2 | Display | 320×480 |
| 3 | Sony microcontroller port A | Bit `0x10` (HOLD switch) cleared |
| 4 | Pen coordinates | X and Y inverted versus TG50; 480 lines scaled into the TG50 raw range |

These exist only in a local diagnostic build and are not committed.

## Next

1. Decide whether to add a real NX70V device type with the four settings above.
2. Best fix for booting: a dump of the device's own small boot ROM (flash `0x0`–`0x1FFFF`). Without it, NX70V still needs a TG50 small boot ROM to start.

## Running

There is no committed way to boot NX70V. With this branch you can only check the ROM header:

```sh
python3 tools/probe-nx70v-rom.py /path/to/nx70v.rom
```

Starting the raw ROM with `--tg50` stops at `Invalid instr 0x56514b30 at 0x0001800c`, because the CPU runs the device data area. For building and running TG50, see the [TG50 branch README](https://github.com/muns5965/cloudpilot-emu/blob/feature/sony-tg50/README.md).

**Never commit or publish** NX70V or TG50 ROM files, any composite image, or the device data area at `0x18000`.

---

# CloudpilotEmu — Sony CLIÉ PEG-NX70V 조사 브랜치

> **조사 단계입니다. 코드에는 아직 NX70V 기종이 없습니다.** 로컬 진단 빌드에서는 NX70V Palm OS가 Setup을 마치고 CLIÉ 런처까지 들어갑니다. 이 브랜치는 [`feature/sony-tg50`](https://github.com/muns5965/cloudpilot-emu/tree/feature/sony-tg50)에서 갈라져 나왔고, 그 TG50 코드(`--tg50` 실행 경로와 TG50 장치 모델)를 바탕으로 씁니다. 아래 부팅 결과는 모두 커밋하지 않은 로컬 진단 빌드에서 확인한 것입니다.

[**NX70V 조사 기록(영문·한국어)**](docs/sony-nx70v-port.md) · [**TG50 브랜치 README**](https://github.com/muns5965/cloudpilot-emu/blob/feature/sony-tg50/README.md) · [**TG50 기술 문서(영문)**](docs/sony-tg50-port.md) · [**CloudpilotEmu 기존 README(영문)**](doc/README-upstream.md) · [**원본 저장소**](https://github.com/cloudpilot-emu/cloudpilot-emu)

## 현황(2026-10-23)

NX70V ROM: Palm OS 5.0, Sony, HAL ID `crdb`, 12MiB. 저장소에 포함하지 않습니다.

| 항목 | 상태 | 비고 |
| --- | --- | --- |
| ROM 헤더 | **인식** | `0x20000`에 헤더. `tools/probe-nx70v-rom.py`로 확인 |
| 덤프 그대로 | **부팅 안 됨** | `0x0`~`0x1FFFF`가 기기 데이터(일련번호 성격 값 포함)이고, TG50의 `0x0`에 있는 작은 부트 ROM이 없음. 본 ROM 안의 부트 사본이 복사되지 않은 코드를 불러 무한 반복 |
| 로컬 조합 이미지(TG50 작은 ROM + NX70V 본 ROM) | **펜 보정 포함 Setup → CLIÉ 런처** | 로컬 파일만, 커밋 안 함. 아래 NX70V 설정 4가지 필요 |
| `0x10000000`(CS4) | **CF 슬롯 컨트롤러**(Sony CfSlot Library) | 에뮬레이트되지 않은 쓰기가 재부팅 반복의 원인. 최소 모델: 상태 `+0x10` = `0xC0`(카드 없음), 나머지 쓰기 수용 |
| `0x14000000` | TG50과 같은 메모리스틱 컨트롤러 | |
| Sony 마이컴 | TG50과 같음(ID `0x4741`) | |
| 화면 | **320×480**(TG50은 320×320) | ROM이 가상 그래피티 영역과 상태 표시줄을 직접 그림 |
| 터치 | **동작**(진단 빌드) | 터치 칩(AD7873)과 펜 인터럽트(GPIO9) 모두 TG50과 같음. TG50 마이컴 모델이 NX70V HOLD 스위치(포트 A 비트 `0x10`)를 켜짐으로 알려 막혀 있었음. X·Y가 TG50과 반대 |

## 필요한 NX70V 설정(진단 빌드)

| # | 설정 | 값 |
| --- | --- | --- |
| 1 | `0x10000000` CF 슬롯 컨트롤러 | 최소 모델: `+0x10` = `0xC0`(카드 없음), 나머지 쓰기 수용 |
| 2 | 화면 | 320×480 |
| 3 | Sony 마이컴 포트 A | 비트 `0x10`(HOLD 스위치) 끄기 |
| 4 | 펜 좌표 | TG50 대비 X·Y 반전, 480줄을 TG50 원시값 범위로 축소 |

로컬 진단 빌드에만 있고 커밋하지 않았습니다.

## 다음 단계

1. 위 4가지 설정으로 정식 NX70V 기종을 코드에 넣을지 결정.
2. 부팅 문제의 가장 확실한 해결: 실기에서 작은 부트 ROM(플래시 `0x0`~`0x1FFFF`)을 다시 덤프. 그 전까지 NX70V 시작에는 TG50 작은 부트 ROM이 필요.

## 실행

커밋된 코드로 NX70V를 부팅하는 방법은 아직 없습니다. 이 브랜치로는 ROM 헤더만 확인할 수 있습니다.

```sh
python3 tools/probe-nx70v-rom.py /path/to/nx70v.rom
```

원본 ROM을 `--tg50`으로 실행하면 CPU가 기기 데이터 영역을 실행해 `Invalid instr 0x56514b30 at 0x0001800c`에서 멈춥니다. TG50 빌드·실행 방법은 [TG50 브랜치 README](https://github.com/muns5965/cloudpilot-emu/blob/feature/sony-tg50/README.md)를 보세요.

NX70V·TG50 ROM 파일, 조합 이미지, `0x18000`의 기기 데이터 영역은 **절대 커밋·공개하지 않습니다.**
