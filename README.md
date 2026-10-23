# CloudpilotEmu — Sony CLIÉ PEG-TG50 port

> **Work in progress · native ROM boots through Setup to the CLIÉ Launcher.** This is a development fork of [cloudpilot-emu/cloudpilot-emu](https://github.com/cloudpilot-emu/cloudpilot-emu), focused on running the **Sony CLIÉ PEG-TG50 (Palm OS 5)** in CloudpilotEmu. The existing emulator's Palm OS 1–4 and E2 implementations remain the upstream baseline; TG50 runs in the experimental native build and, since 2026-10-23, in the browser: **[muns5965.github.io/cloudpilot-emu](https://muns5965.github.io/cloudpilot-emu/)** (bring your own ROM; see [Browser](#browser-wasm)).

[**TG50 port plan and technical notes**](docs/sony-tg50-port.md) · [**Original CloudpilotEmu README**](doc/README-upstream.md) · [**Upstream repository**](https://github.com/cloudpilot-emu/cloudpilot-emu) · [**Original uARM TG50 code**](https://github.com/uARM-Palm/uARM/blob/master/deviceSonyTG50.c)


# CloudpilotEmu — Sony CLIÉ PEG-TG50 포팅

> **개발 진행 중 · 네이티브 ROM이 Setup을 거쳐 CLIÉ 런처까지 부팅됨.** 이 저장소는 [CloudpilotEmu 원본](https://github.com/cloudpilot-emu/cloudpilot-emu)을 포크하여 **Sony CLIÉ PEG-TG50(Palm OS 5)** 실행을 목표로 개발하는 프로젝트입니다. 기존 Palm OS 1~4 및 Tungsten E2 에뮬레이션은 원본 프로젝트를 기반으로 하며, TG50은 실험용 네이티브 빌드와, 2026-10-23부터 브라우저에서도 실행됩니다: **[muns5965.github.io/cloudpilot-emu](https://muns5965.github.io/cloudpilot-emu/)**(ROM은 직접 준비, [브라우저 실행](#브라우저wasm-실행) 참고).

[**TG50 상세 이식 계획(영문)**](docs/sony-tg50-port.md) · [**CloudpilotEmu 기존 README(영문)**](doc/README-upstream.md) · [**원본 저장소**](https://github.com/cloudpilot-emu/cloudpilot-emu) · [**원본 uARM TG50 코드**](https://github.com/uARM-Palm/uARM/blob/master/deviceSonyTG50.c)



## Goal

Reuse CloudpilotEmu's existing ARM/PXA OS 5 engine and port Sony-specific hardware models from [uARM-Palm/uARM](https://github.com/uARM-Palm/uARM). The native TG50 build now completes **Setup (including pen calibration) and reaches the CLIÉ Launcher**. The built-in QWERTY keyboard is wired to host keys. The power key now sleeps and wakes the device. Session files save and resume. Next: browser/WASM selection and display. A TG50 ROM must be obtained separately; no proprietary ROM is included.

## 개발 목표

CloudpilotEmu에 이미 구현된 ARM/PXA 기반 OS 5 실행 엔진을 재사용하고, [uARM 원본](https://github.com/uARM-Palm/uARM)의 Sony 전용 하드웨어 구현을 이식합니다. 현재 네이티브 TG50 빌드는 **펜 보정을 포함한 Setup을 마치고 CLIÉ 런처까지 진입**합니다. TG50 내장 QWERTY 키보드는 PC 키에 연결됐습니다. 전원 키로 절전·복귀도 됩니다. 세션 파일 저장·재개도 됩니다. 다음 단계는 브라우저/WASM 기기 선택과 화면 출력입니다. TG50 ROM은 별도로 준비해야 하며, 저작권이 있는 ROM 파일은 이 저장소에 포함하지 않습니다.

---

## Current progress

Verified on 2026-10-23 with a native Linux SDL2 build of this branch and a TG50 NOR ROM whose SHA-256 matches the uARM reference (`c6d5985c…860b94f`).

| Component | Status | Code / notes |
| --- | --- | --- |
| Native build (common + cp-uarm) | **Builds locally** (Linux, no warnings); CI result not checked | `src/uarm/Makefile`, `.github/workflows/tg50-native-build.yml` |
| Native launch | **Works**; the ROM header (`sony`/`vrna`) is identified, `--tg50` is optional | `src/uarm/native/main.cpp`, `src/common/rom_info5.cpp`; 17 MiB RAM, 320×320 |
| Boot: GPIO13/14 controller IRQ lines | **Works** | `src/uarm/uarm/deviceSonyTG50.cpp` |
| Boot: power key GPIO0 idle level | **Fixed**, boots past the debounce loop | `sonyTG50Initialize()` |
| Setup 1–4 incl. pen calibration | **Works** | E2 touch-translation patch disabled for TG50 (`soc_PXA.cpp`) |
| CLIÉ Launcher | **Reached; responds to taps** | — |
| AD7873 touch controller | **Works** on the 320×320 screen | `src/uarm/uarm/sspdev_AD7873.cpp` |
| Sony controller MMIO / 8×8 keyboard matrix | **Works**: typing in Memo Pad verified | `src/uarm/uarm/mmiodev_TG50uc.cpp`, `sony_tg50_hardware.h` |
| Host keys → TG50 keys | **Works** for letters, Space, Enter, Backspace, Shift, Ctrl, Tab, Caps Lock, `,` `.` `-`, Blue (F7), Red (F9), Home, PageUp/PageDown; jog select moved to F11 | `keys.h`, `SdlEventHandler.cpp` (TG50 only), `sonyTG50HostKey()`. PC digit keys are not mapped (use Blue + top row). Fast typing (40 ms per key) works: each key is held down for at least ~100 ms |
| Host power key (Esc), sleep/resume | **Works**: Esc sleeps (screen off), Esc again wakes to the previous screen; keyboard and touch work after resume | Esc drives GPIO0 and pulls GPIO13 low; PXA deep sleep (PWRMODE 3) waits for a PWER wake edge; serial EEPROM data line idles high; controller state kept across resume. RTC alarm wake not checked |
| Native window skin | Shows the E2 Graffiti silkscreen, which has no function on TG50 | TG50 has a QWERTY keyboard and no silkscreen; shares the E2 `mode_320x320` display configuration |
| AN32502A power (I²C) | Ported, not individually tested | `src/uarm/uarm/i2cdev_AN32502A.cpp` |
| AK4534 audio codec (I²C) | Register model only, no PCM output | `src/uarm/uarm/i2sdev_AK4534.cpp` |
| Memory Stick controller | Register model only, no media | `src/uarm/uarm/mmiodev_MemoryStickController.cpp` |
| Savestates / session files | **Works** (native): `save-session <file>` saves, starting the emulator with the session file resumes (also while asleep) | Sony peripherals in one `sonyTG50` chunk, deep sleep in a `pxaDeepSleep` chunk; the device type comes from the session file |
| Browser/WASM | **Works** (PC Chrome and phone browser): ROM import, boot, touch, hard buttons. PC keyboard mapped as in native, typing in the browser not yet hand-tested | Device `PEG-TG50` (320×320, no silkscreen, NR70 button image); raw key path `web/common/helper/hardwareKeyboard.ts` → worker → `Uarm.KeyDown/KeyUp`. **No text input on phones** yet (no on-screen keyboard). Deployed by `.github/workflows/fork-pages.yml` |

## 현재 진행 상태

2026-10-23, 이 브랜치의 Linux SDL2 네이티브 빌드와 uARM 기준 해시(`c6d5985c…860b94f`)가 같은 TG50 NOR ROM으로 확인한 결과입니다.

| 구성 요소 | 상태 | 코드 / 비고 |
| --- | --- | --- |
| 네이티브 빌드(common + cp-uarm) | **로컬 빌드 성공**(Linux, 경고 없음), CI 결과는 미확인 | `src/uarm/Makefile`, `.github/workflows/tg50-native-build.yml` |
| 네이티브 실행 | **동작**. ROM 헤더(`sony`/`vrna`)로 자동 인식, `--tg50`은 생략 가능 | `src/uarm/native/main.cpp`, `src/common/rom_info5.cpp`, RAM 17MiB, 320×320 |
| 부팅: GPIO13/14 컨트롤러 IRQ 신호선 | **동작** | `src/uarm/uarm/deviceSonyTG50.cpp` |
| 부팅: 전원 키 GPIO0 대기 상태 | **수정 완료**, 키 해제 대기 루프 통과 | `sonyTG50Initialize()` |
| Setup 1~4(펜 보정 포함) | **동작** | TG50에서 E2용 펜 좌표 변환 패치 해제(`soc_PXA.cpp`) |
| CLIÉ 런처 | **진입, 터치 반응 확인** | — |
| AD7873 터치 컨트롤러 | 320×320 화면에서 **동작** | `src/uarm/uarm/sspdev_AD7873.cpp` |
| Sony 컨트롤러 MMIO·8×8 키 매트릭스 | **동작**: 메모장 입력 확인 | `src/uarm/uarm/mmiodev_TG50uc.cpp`, `sony_tg50_hardware.h` |
| PC 키 → TG50 키 연결 | **동작**: 글자, Space, Enter, Backspace, Shift, Ctrl, Tab, Caps Lock, `,` `.` `-`, Blue(F7), Red(F9), Home, PageUp/PageDown. 조그 선택은 F11로 이동 | `keys.h`, `SdlEventHandler.cpp`(TG50 전용), `sonyTG50HostKey()`. PC 숫자 키는 미연결(Blue + 윗줄 사용). 빠른 입력(키당 40ms)도 정확: 키를 최소 약 100ms 눌린 상태로 유지 |
| PC 전원 키(Esc), 절전·복귀 | **동작**: Esc로 절전(화면 꺼짐), 다시 Esc로 이전 화면 복귀. 복귀 후 키보드·터치 정상 | Esc가 GPIO0과 GPIO13(Low)을 구동. PXA 절전(PWRMODE 3)은 PWER 깨우기 신호를 기다림. 직렬 EEPROM 데이터선 High 유지. 복귀 때 마이컴 상태 유지. 알람(RTC)으로 깨우기는 미확인 |
| 네이티브 창 스킨 | E2 그래피티 실크스크린이 그려지며 TG50에서는 기능 없음 | TG50은 QWERTY 키보드가 있고 실크스크린이 없음. E2와 같은 `mode_320x320` 화면 설정을 공유 |
| AN32502A 전원 관리(I²C) | 이식 완료, 개별 시험 안 함 | `src/uarm/uarm/i2cdev_AN32502A.cpp` |
| AK4534 오디오 코덱(I²C) | 레지스터 모델만, PCM 출력 없음 | `src/uarm/uarm/i2sdev_AK4534.cpp` |
| Memory Stick 컨트롤러 | 레지스터 모델만, 카드 미지원 | `src/uarm/uarm/mmiodev_MemoryStickController.cpp` |
| 상태 저장·세션 파일 | **동작**(네이티브): `save-session <파일>`로 저장, 세션 파일로 실행하면 이어서 동작(절전 중 저장도 가능) | Sony 주변장치는 `sonyTG50` 저장 단위 하나, 절전 상태는 `pxaDeepSleep` 저장 단위. 기종은 세션 파일에서 읽음 |
| 브라우저/WASM | **동작**(PC Chrome, 휴대폰 브라우저): ROM 불러오기, 부팅, 터치, 하드 버튼. PC 키보드는 네이티브와 같게 연결, 브라우저에서 직접 입력 시험은 아직 | 기종 `PEG-TG50`(320×320, 실크스크린 없음, NR70 버튼 그림). 키 경로 `web/common/helper/hardwareKeyboard.ts` → 워커 → `Uarm.KeyDown/KeyUp`. **휴대폰에서는 아직 글자 입력 불가**(화면 키보드 없음). `.github/workflows/fork-pages.yml`로 배포 |

---

## Development order

1. **Browser on-screen keyboard**: a TG50 keyboard image below the buttons, sending the same keys, so phones can type.
2. **Sleep follow-ups**: check RTC alarm wake; optionally map PC digit keys to Blue + top row.
3. **Native display configuration**: drop the E2 silkscreen in the native window (the browser already has none) without changing the E2's shared configuration.
4. **Savestates in the browser**: native done; browser save/restore not checked yet.
5. **Optional touch translation**: a TG50-specific raw-to-screen patch (inverse of `3200 − 10·x/y`, after verifying the X/Y cross-wiring) would make touch independent of calibration, as on the E2.
6. Audio and Memory Stick.

Detailed file-level checklist and fix history: [docs/sony-tg50-port.md](docs/sony-tg50-port.md).

## 개발 순서

1. **브라우저 화면 키보드:** 버튼 줄 아래에 TG50 키보드 그림을 넣고 같은 키 신호를 보내 휴대폰에서도 입력할 수 있게 합니다.
2. **절전 후속:** 알람(RTC)으로 깨우기를 확인하고, 필요하면 PC 숫자 키를 Blue + 윗줄로 연결합니다.
3. **네이티브 화면 설정:** E2와 공유하는 설정은 건드리지 않고 네이티브 창의 E2 실크스크린을 없앱니다(브라우저에는 이미 없음).
4. **브라우저 상태 저장:** 네이티브는 완료, 브라우저 저장·복원은 아직 확인하지 않음.
5. **선택: 펜 좌표 변환:** TG50 전용 원시값→화면 변환 패치(`3200 − 10·x/y`의 역계산, X/Y 교차 배선 확인 후)를 넣으면 E2처럼 보정과 관계없이 터치가 정확해집니다.
6. 오디오와 Memory Stick.

파일별 상세 점검표와 수정 이력: [TG50 포팅 기술 문서](docs/sony-tg50-port.md).

---

## Branch and upstream

Active development branch: `feature/sony-tg50`; `master` is the original fork baseline. This is an independent experimental fork, **not an official CloudpilotEmu release**.

The original project's emulator overview, device compatibility, existing build instructions and licensing information have been preserved in [doc/README-upstream.md](doc/README-upstream.md). Refer to that document for the underlying CloudpilotEmu project. Existing functionality and licensing obligations remain applicable to this fork.

## 개발 브랜치와 원본 프로젝트

개발 브랜치는 `feature/sony-tg50`이며, `master`는 원본 포크 기준 상태로 유지합니다. 본 프로젝트는 **공식 CloudpilotEmu 배포판이 아닌 독립적인 실험용 포크**입니다.

기존 CloudpilotEmu의 소개, 지원 기종, 빌드 안내 및 라이선스 설명은 [원본 README 보존 문서](doc/README-upstream.md)에 유지했습니다. 기존 기능과 라이선스 의무는 이 포크에도 적용됩니다.

---

## Fixes that made the ROM boot

| Stage | Symptom | Cause | Fix |
| --- | --- | --- | --- |
| Early boot | Wait on the controller IRQ lines | GPIO13/GPIO14 idle levels | GPIO13 idle high, controller IRQ on GPIO14 (as in original uARM) |
| Mid boot | Blank screen, CPU spinning at `0x20036DBC` | Power key on GPIO0 (active low) left low, so the ROM waited for release | Set GPIO0 high (released) at init |
| Setup | Calibration restarts at the second target | E2 touch-translation patch (`280 + 10x`, `210 + 7y`) applied to the TG50 AD7873 (`3200 − 10·x/y`) | Register patches without `PATCH_FEATURE_TOUCH_TRANSLATION` in TG50 mode; Palm OS uses its own calibration |

Each case was found by comparing against original uARM running the same ROM. Details: [docs/sony-tg50-port.md](docs/sony-tg50-port.md).

## ROM 부팅까지 고친 문제

| 단계 | 증상 | 원인 | 해결 |
| --- | --- | --- | --- |
| 부팅 초반 | 컨트롤러 IRQ 신호선에서 대기 | GPIO13/GPIO14 기본 상태 | GPIO13은 High로 대기, 컨트롤러 IRQ는 GPIO14(원본 uARM과 같음) |
| 부팅 중반 | 화면 안 뜸, CPU가 `0x20036DBC`에서 무한 대기 | 전원 키 GPIO0(Low가 눌림)이 Low로 남아 ROM이 키 해제를 기다림 | 초기화 때 GPIO0을 High(해제)로 설정 |
| Setup | 펜 보정이 두 번째 점에서 처음으로 돌아감 | E2용 펜 좌표 변환 패치(`280 + 10x`, `210 + 7y`)가 TG50 AD7873(`3200 − 10·x/y`)에 적용됨 | TG50에서는 `PATCH_FEATURE_TOUCH_TRANSLATION`을 빼고 패치 등록, Palm OS 자체 보정 사용 |

모두 같은 ROM을 원본 uARM에서 실행해 비교하는 방법으로 찾았습니다. 상세: [TG50 포팅 기술 문서](docs/sony-tg50-port.md).

---

## GPIO interrupt diagnostics

A diagnostic-only build may define `TG50_TRACE_GPIO` (PXA GPIO13/14 levels and GRER0/GFER0/GEDR0 state) together with `TG50_TRACE_IRQ` (Sony controller IRQ transitions). Compare the results with original uARM using the same ROM. These traces are not needed for normal builds.

## GPIO 인터럽트 진단

진단용 빌드에서는 `TG50_TRACE_GPIO`(PXA GPIO13·14 입력 상태와 GRER0·GFER0·GEDR0 설정) 및 `TG50_TRACE_IRQ`(Sony 컨트롤러 인터럽트 전이)를 함께 사용할 수 있습니다. 동일한 TG50 ROM을 사용하는 원본 uARM과 기록을 비교하는 용도이며, 일반 빌드에는 필요하지 않습니다.

---

## Experimental native TG50 ROM launch

The native emulator identifies a TG50 NOR image from its ROM header (company `sony`, HAL `vrna`). `--tg50` still selects TG50 explicitly:

```sh
./src/uarm/cp-uarm --tg50 /path/to/TG50-ROM.bin
```

Other Sony ROMs are not identified, and ROM-format validation is not bypassed. TG50 defaults to 17 MiB RAM and does not take a NAND image. To resume, start the emulator with a saved session file instead of the ROM (`--tg50` is not needed; the session records the device type).

What to expect on first launch:

1. Setup 1–2: tap anywhere to continue.
2. Pen calibration: tap the center of each of the three targets accurately (top-left, bottom-right, center).
3. Setup 3: tap **Next**; Setup 4: tap **Done**. The CLIÉ Launcher appears.

The E2 silkscreen drawn below the screen does nothing on TG50. Type with the PC keyboard: letters, Space, Enter, Backspace, Shift, Ctrl; F7 = Blue (digits/symbols), F9 = Red, Home = launcher, F11 = jog select, Esc = power (sleep/wake).

## 실험용 TG50 네이티브 ROM 실행

네이티브 에뮬레이터는 ROM 헤더(회사 `sony`, HAL `vrna`)로 TG50 NOR 이미지를 인식합니다. `--tg50`으로 명시적으로 고를 수도 있습니다.

```sh
./src/uarm/cp-uarm --tg50 /path/to/TG50-ROM.bin
```

다른 Sony ROM은 인식하지 않으며 ROM 형식 검사도 생략하지 않습니다. TG50 기본 RAM은 17MiB이며 NAND 이미지는 받지 않습니다. 이어서 실행하려면 ROM 대신 저장한 세션 파일로 실행합니다(세션에 기종이 기록되므로 `--tg50`은 필요 없음).

첫 실행 순서:

1. Setup 1~2: 화면 아무 곳이나 눌러 넘깁니다.
2. 펜 보정: 표적 3개(왼쪽 위, 오른쪽 아래, 가운데)의 중심을 정확히 누릅니다.
3. Setup 3에서 **Next**, Setup 4에서 **Done**을 누르면 CLIÉ 런처가 나타납니다.

화면 아래에 그려지는 E2 실크스크린은 TG50에서 동작하지 않습니다. 입력은 PC 키보드로 합니다: 글자, Space, Enter, Backspace, Shift, Ctrl, F7 = Blue(숫자·기호), F9 = Red, Home = 런처, F11 = 조그 선택, Esc = 전원(절전·복귀).

## Browser (WASM)

Open **https://muns5965.github.io/cloudpilot-emu/**. No ROM is hosted; the ROM file stays in your browser.

1. **Sessions → +**, choose the TG50 NOR ROM. The device shows as **Sony PEG-TG50** with 17 MB RAM.
2. Start the session; Setup and pen calibration work as in native (tap with the mouse or finger).
3. Hard buttons below the screen work (the button image is borrowed from the NR70).

PC keyboard (same mapping as native):

| Host key | TG50 |
| --- | --- |
| Letters, Space, Enter, Backspace, Shift, Ctrl, Tab, Caps Lock, `,` `.` `-` | Same keys |
| F7 / F9 | Blue / Red |
| F1–F4 | Hard buttons 1–4 |
| Arrows | Up / Down / Left / Right |
| PageUp / PageDown / End / F11 | Jog up / down / back / select |
| Home | Home |
| F5 / `` ` `` | Voice / Graffiti |
| Esc | Power (sleep/wake) |
| Ctrl+Shift | Toggle game mode (keys drive buttons, as for other devices) |

Limits: phones have no way to type yet (no on-screen keyboard). F5 is captured while the emulator runs, so it does not reload the page.

Deployment: `.github/workflows/fork-pages.yml` builds the WASM core and the PWA on each push to `feature/sony-tg50` (or manually) and deploys to GitHub Pages. One-time setup: Settings → Pages → Source = GitHub Actions, and allow the branch under Settings → Environments → github-pages → Deployment branches.

## 브라우저(WASM) 실행

**https://muns5965.github.io/cloudpilot-emu/** 를 엽니다. ROM은 올라가 있지 않으며, ROM 파일은 브라우저 안에서만 쓰입니다.

1. **Sessions → +** 에서 TG50 NOR ROM을 고릅니다. 기종은 **Sony PEG-TG50**, RAM은 17MB로 표시됩니다.
2. 세션을 실행하면 네이티브와 같이 Setup과 펜 보정이 진행됩니다(마우스나 손가락으로 누름).
3. 화면 아래 하드 버튼이 동작합니다(버튼 그림은 NR70 것을 빌려 씀).

PC 키보드(네이티브와 같은 연결):

| PC 키 | TG50 |
| --- | --- |
| 글자, Space, Enter, Backspace, Shift, Ctrl, Tab, Caps Lock, `,` `.` `-` | 같은 키 |
| F7 / F9 | Blue / Red |
| F1~F4 | 하드 버튼 1~4 |
| 방향키 | 위 / 아래 / 왼쪽 / 오른쪽 |
| PageUp / PageDown / End / F11 | 조그 위 / 아래 / 뒤로 / 선택 |
| Home | 홈 |
| F5 / `` ` `` | 음성 / 그래피티 |
| Esc | 전원(절전·복귀) |
| Ctrl+Shift | 게임 모드 전환(다른 기종처럼 키가 버튼을 누름) |

제한: 휴대폰에서는 아직 글자를 입력할 방법이 없습니다(화면 키보드 없음). 에뮬레이터 실행 중에는 F5를 에뮬레이터가 받으므로 페이지 새로고침이 되지 않습니다.

배포: `.github/workflows/fork-pages.yml`이 `feature/sony-tg50`에 push할 때마다(또는 수동 실행) WASM과 PWA를 빌드해 GitHub Pages에 올립니다. 처음 한 번 설정: Settings → Pages → Source = GitHub Actions, 그리고 Settings → Environments → github-pages → Deployment branches에서 이 브랜치를 허용합니다.
