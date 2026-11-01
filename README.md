# CloudpilotEmu — Sony CLIÉ PEG-VZ90 branch

> **Experimental.** VZ90 work and tests happen on this branch. The emulator takes the user's own original VZ90 ROM and
> rebuilds it at load time into an image for CloudpilotEmu's paravirtualized ("PV") machine, together with the
> independent DAL (device abstraction layer) [`pv-vz90-dal`](https://github.com/muns5965/pv-vz90-dal-public), which is
> built into the emulator. No ROM is included in this repository.

[**Original CloudpilotEmu README**](doc/README-upstream.md) · [**Upstream repository**](https://github.com/cloudpilot-emu/cloudpilot-emu) · [**Web test runner**](tools/vz90-webtest/README.md)

[한국어](#cloudpilotemu--sony-clié-peg-vz90-브랜치)

## How it runs

- The original ROM (Palm OS 5.2.1, 320×480) is read, never changed, and rebuilt in memory into a PV image with the
  DAL bundle. The Sony hardware drivers in the ROM do not run; the DAL implements the Palm OS hardware interface on
  the PV machine.
- The DAL is built from the submodule `external/pv-vz90-dal` (pinned commit, GPL-3.0-or-later) by
  `tools/vz90-dal/gen.py`, which needs the Arm GNU toolchain (`arm-none-eabi-gcc`). After cloning run
  `git submodule update --init --recursive`. `VZ90_DAL=none` builds without the DAL (VZ90 ROMs then cannot be started
  without the development option below).
- Native: `cp-uarm <original VZ90 ROM>`. The DAL in use is printed with its build ID and SHA-256. A saved session
  (`save-session <file>`) is started with the session file alone; it holds the rebuilt image with its DAL, and a
  message says whether that DAL is the built-in one.
- Browser (build of this branch; the deployed browser version does not include VZ90 yet): **Sessions → +**, choose the
  ROM. The DAL is a separate file of the build, loaded only for a VZ90 ROM or session and checked against the SHA-256
  recorded at build time; a short message shows its build ID and hash.
- Development option: `--vz90-dal <bundle>` (native) or the page parameter `?vz90dal=select` (browser, then
  **Select DAL bundle**) uses another DAL bundle. A bundle given this way that cannot be used stops the start; there
  is no fallback to the built-in DAL.
- Two PV additions are enabled only for VZ90 images: a 4 KB Sony shared page at `0x3F843000` and 4 KB of RAM at
  physical address 0. Sessions saved before the address-0 RAM existed are restored by the current build; a session
  saved by the current build is not accepted by older builds.

## Status (2026-11-01)

Status: **works (where)** = implemented and tested in the environment named; **untested**; **not implemented** / known
limit.

| Item | Status | Notes |
| --- | --- | --- |
| Boot, Welcome setup, Launcher | **Works** (native, browser) | |
| Preferences, launcher apps, Calculator, hard buttons | **Works** (browser test runner) | |
| CLIE Organizer: address book (add, edit, delete), ToDo, Memo Pad (text) | **Works** (browser test runner) | Text entry with the on-screen kana keyboard |
| Session save and restore | **Works** (native, browser) | Including sessions from builds before the address-0 RAM |
| Stability | **Works**: one-hour browser run (launcher apps opened and closed, 26 loops) | Nothing longer tested |
| Sound | **Not implemented** | No audio output on PV |
| Memory Stick, CF, NAND slots | **Not implemented** | Slot drivers are not loaded on PV, so 辞書 (dictionary) refuses to start |
| Serial ports | **Not implemented** | CFUtility refuses to start (Serial Manager features not registered) |
| CLIE Organizer 手書きメモ (handwritten memo) | **Known defect** | Creating a new memo ends in a fatal error. The app clears each new record with a length read from the record's own uninitialised first word; same code in the Japanese and English ROMs; why a real device is not affected is not established |
| Power key | **Known limit** | A 68k fault, then the DAL stops |
| Landscape | **Known limit** | One intermittent fatal error in 19 runs, cause not found |

TG50 and NX70V: browser boot to the Setup screen was compared before and after the VZ90 changes, with no difference
other than animation frame and clock.

## Tests

`tools/vz90-webtest/` drives the browser build with Playwright (setup, rounds of steps T0–T9 with golden images,
soak). The ROM is given on the command line (a DAL bundle only to test another DAL). Diagnostic builds (`-DUARM_DIAG`) print `[diag]` lines; regular
builds compile none of it.

## Notes

- ROM files and session files are never committed or published; the DAL is only built from its source.
- Each branch keeps its own README; see [`master`](https://github.com/muns5965/cloudpilot-emu) for the TG50 and
  NX70V branches.

---

# CloudpilotEmu — Sony CLIÉ PEG-VZ90 브랜치

> **실험용입니다.** VZ90 작업과 시험은 이 브랜치에서 합니다. 에뮬레이터는 사용자가 가진 원본 VZ90 ROM을 읽어,
> 에뮬레이터에 내장된 독립 DAL(장치 추상화 계층) [`pv-vz90-dal`](https://github.com/muns5965/pv-vz90-dal-public)과 함께
> CloudpilotEmu의 PV(paravirtualized) 머신용 이미지로 실행 시점에 다시 구성합니다. 이 저장소에 ROM은 들어 있지 않습니다.

## 실행 방식

- 원본 ROM(Palm OS 5.2.1, 320×480)은 읽기만 하고, 메모리 안에서 DAL 번들과 함께 PV 이미지로 다시 구성합니다. ROM의
  Sony 하드웨어 드라이버는 실행되지 않고, DAL이 PV 머신에서 Palm OS 하드웨어 인터페이스를 구현합니다.
- DAL은 서브모듈 `external/pv-vz90-dal`(고정 커밋, GPL-3.0-or-later)에서 `tools/vz90-dal/gen.py`가 빌드하며, Arm
  GNU 툴체인(`arm-none-eabi-gcc`)이 필요합니다. 내려받은 뒤 `git submodule update --init --recursive`를 실행합니다.
  `VZ90_DAL=none`이면 DAL 없이 빌드합니다(이 경우 아래 개발용 옵션 없이는 VZ90 ROM을 시작할 수 없음).
- 네이티브: `cp-uarm <원본 VZ90 ROM>`. 사용한 DAL의 빌드 ID와 SHA-256을 출력합니다. 저장한 세션(`save-session
  <파일>`)은 세션 파일만으로 시작하며, 세션에는 다시 구성한 이미지와 그 DAL이 들어 있고 그 DAL이 내장 DAL과 같은지
  알려 줍니다.
- 브라우저(이 브랜치의 빌드. 배포된 브라우저 버전에는 아직 VZ90이 없음): **Sessions → +** 에서 ROM을 고릅니다. DAL은
  빌드에 포함된 별도 파일로, VZ90 ROM·세션일 때만 불러오며 빌드 때 기록한 SHA-256과 맞는지 확인하고 빌드 ID와 해시를
  짧게 표시합니다.
- 개발용 옵션: `--vz90-dal <번들>`(네이티브) 또는 페이지 매개변수 `?vz90dal=select`(브라우저, **Select DAL bundle**)로
  다른 DAL 번들을 씁니다. 이렇게 지정한 번들을 쓸 수 없으면 시작을 중단하며, 내장 DAL로 대체하지 않습니다.
- VZ90 이미지에서만 켜지는 PV 추가 사항: `0x3F843000`의 4 KB Sony 공유 페이지, 물리 주소 0의 4 KB RAM. 주소 0 RAM이
  생기기 전에 저장한 세션은 현재 빌드에서 복원되며, 현재 빌드가 저장한 세션은 이전 빌드가 받아들이지 않습니다.

## 현황 (2026-11-01)

상태 표기: **동작(환경)** = 구현하고 적힌 환경에서 시험함 · **미검증** · **미구현**·알려진 제약.

| 항목 | 상태 | 비고 |
| --- | --- | --- |
| 부팅, 초기 설정(Welcome), 런처 | **동작**(네이티브, 브라우저) | |
| 환경설정, 런처 앱, 계산기, 하드 버튼 | **동작**(브라우저 시험 도구) | |
| CLIE Organizer: 주소록(추가·수정·삭제), ToDo, 메모장(텍스트) | **동작**(브라우저 시험 도구) | 화면 가나 키보드로 입력 |
| 세션 저장·복원 | **동작**(네이티브, 브라우저) | 주소 0 RAM 이전 빌드의 세션 포함 |
| 안정성 | **동작**: 브라우저 1시간 실행(런처 앱 열고 닫기 26회) | 더 긴 시험은 하지 않음 |
| 소리 | **미구현** | PV에 오디오 출력 없음 |
| Memory Stick·CF·NAND 슬롯 | **미구현** | PV에서 슬롯 드라이버를 올리지 않아 辞書(사전)가 실행을 거부 |
| 시리얼 포트 | **미구현** | CFUtility가 실행을 거부(Serial Manager 기능 미등록) |
| CLIE Organizer 手書きメモ(손글씨 메모) | **알려진 결함** | 새 메모를 만들면 치명적 오류. 앱이 새 레코드를 그 레코드의 초기화되지 않은 첫 워드를 길이로 삼아 지움. 일본어·영어 ROM 모두 같은 코드. 실기에서 문제가 없는 이유는 밝히지 못함 |
| 전원 키 | **알려진 제약** | 68k 오류 후 DAL이 멈춤 |
| 가로 화면 | **알려진 제약** | 19회 중 1회 간헐적 치명적 오류, 원인 미확인 |

TG50·NX70V: VZ90 변경 전후로 브라우저에서 Setup 화면까지의 부팅을 비교했고, 애니메이션 프레임과 시계 외 차이는 없었습니다.

## 시험

`tools/vz90-webtest/`가 Playwright로 브라우저 빌드를 조작합니다(setup, 기준 이미지가 있는 T0–T9 반복, soak). ROM은
명령줄로 넘깁니다(DAL 번들은 다른 DAL을 시험할 때만). 진단 빌드(`-DUARM_DIAG`)는 `[diag]` 줄을 출력하고, 일반 빌드에는 포함되지 않습니다.

## 참고

- ROM 파일과 세션 파일은 커밋·공개하지 않으며, DAL은 소스에서 빌드만 합니다.
- README는 브랜치마다 따로 둡니다. TG50·NX70V 브랜치는 [`master`](https://github.com/muns5965/cloudpilot-emu) 참고.
