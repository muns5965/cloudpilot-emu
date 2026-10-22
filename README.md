# CloudpilotEmu — Sony CLIÉ PEG-TG50 port

> **Work in progress · not bootable yet.** This is a development fork of [cloudpilot-emu/cloudpilot-emu](https://github.com/cloudpilot-emu/cloudpilot-emu), focused on running the **Sony CLIÉ PEG-TG50 (Palm OS 5)** in CloudpilotEmu. The existing emulator's Palm OS 1–4 and E2 implementations remain the upstream baseline; **TG50 is not yet a supported device**.

[**TG50 port plan and technical notes**](docs/sony-tg50-port.md) · [**Original CloudpilotEmu README**](doc/README-upstream.md) · [**Upstream repository**](https://github.com/cloudpilot-emu/cloudpilot-emu) · [**Original uARM TG50 code**](https://github.com/uARM-Palm/uARM/blob/master/deviceSonyTG50.c)


# CloudpilotEmu — Sony CLIÉ PEG-TG50 포팅

> **개발 진행 중 · 아직 부팅할 수 없습니다.** 이 저장소는 [CloudpilotEmu 원본](https://github.com/cloudpilot-emu/cloudpilot-emu)을 포크하여 **Sony CLIÉ PEG-TG50(Palm OS 5)** 실행을 목표로 개발하는 프로젝트입니다. 기존 Palm OS 1~4 및 Tungsten E2 에뮬레이션은 원본 프로젝트를 기반으로 하며, **현재 TG50은 지원 기종으로 등록되지 않았습니다.**

[**TG50 상세 이식 계획(영문)**](docs/sony-tg50-port.md) · [**CloudpilotEmu 기존 README(영문)**](doc/README-upstream.md) · [**원본 저장소**](https://github.com/cloudpilot-emu/cloudpilot-emu) · [**원본 uARM TG50 코드**](https://github.com/uARM-Palm/uARM/blob/master/deviceSonyTG50.c)



## Goal

Reuse CloudpilotEmu's existing ARM/PXA OS 5 engine and port Sony-specific hardware models from [uARM-Palm/uARM](https://github.com/uARM-Palm/uARM). First bring up a **native TG50 ROM boot**, then verify LCD, stylus and physical keyboard input, and only then add browser/WASM selection and display. A TG50 ROM must be obtained separately; no proprietary ROM is included.

## 개발 목표

CloudpilotEmu에 이미 구현된 ARM/PXA 기반 OS 5 실행 엔진을 재사용하고, [uARM 원본](https://github.com/uARM-Palm/uARM)의 Sony 전용 하드웨어 구현을 이식합니다. 먼저 **네이티브 환경에서 TG50 ROM 부팅**을 성공시킨 뒤 LCD 화면, 펜 입력, 물리 키보드를 확인하고, 이후 브라우저/WASM 기기 선택과 화면 출력을 연결합니다. TG50 ROM은 별도로 준비해야 하며, 저작권이 있는 ROM 파일은 이 저장소에 포함하지 않습니다.

---

## Current progress

| Component | Status | Code / next step |
| --- | --- | --- |
| Upstream uARM TG50 implementation reviewed | Done (source review) | `deviceSonyTG50.c`, `mmiodev_TG50uc.c` |
| TG50 hardware constants / 8×8 keyboard matrix | Added, not active | `src/uarm/uarm/sony_tg50_hardware.h` |
| Sony controller MMIO, GPIO and keyboard matrix | Initial C++ port; matrix bug fixed, **runtime unverified** | `src/uarm/uarm/mmiodev_TG50uc.cpp` and `.h` |
| Controller added to native/WASM build source list | Added; syntax-check CI configured, **build result not verified** | `src/uarm/Makefile` |
| AD7873 touch controller | Initial C++ SSP port, **unverified** | `src/uarm/uarm/sspdev_AD7873.cpp` and `.h` |
| AN32502A power (I²C) | Initial C++ port, **unverified** | `src/uarm/uarm/i2cdev_AN32502A.cpp` and `.h` |
| AK4534 audio codec (I²C) | Initial register model port, **unverified** | `src/uarm/uarm/i2sdev_AK4534.cpp` and `.h`; no PCM output |
| Memory Stick controller | Register-only C++ port, **unverified** | `src/uarm/uarm/mmiodev_MemoryStickController.cpp` and `.h`; no media emulation |
| TG50 peripheral assembly | Initial C++ module, **not active / unverified** | `src/uarm/uarm/deviceSonyTG50.cpp` and `.h` |
| TG50 device type / preliminary config | Staged; **not bootable** | `src/common/device_type5.h`, `device_configuration.cpp`; 17 MiB / 320×320 |
| TG50 runtime dispatch | Initial dispatch implemented, **unverified** | E2 NAND/AC97 bypassed; touch and hardware keys forwarded |
| TG50 RAM / extra SRAM map | Staged, **unverified** | 17 MiB default RAM, 1 KiB at `0xAC000000` |
| TG50 ROM boot & savestates | Incomplete | ROM format, startup, device serialization remain outstanding |
| Native build CI (common + cp-uarm) | Workflow added; **result unverified** | `.github/workflows/tg50-native-build.yml` |
| Native ROM boot | **Not attempted** | Requires TG50 ROM and successful native build |
| Browser/WASM TG50 UI and input | Not implemented | After successful native boot |

**Important:** These commits stage the Sony controllers and a peripheral assembly module an initial native device dispatch, and staged RAM/SRAM maps; ROM boot and savestates remain unverified or incomplete. The current fork must **not** be presented as able to run a TG50 ROM.

## 현재 진행 상태

| 구성 요소 | 상태 | 코드 / 다음 작업 |
| --- | --- | --- |
| 원본 uARM의 TG50 구현 검토 | 완료(소스 검토) | `deviceSonyTG50.c`, `mmiodev_TG50uc.c` |
| TG50 하드웨어 상수·8×8 키 매트릭스 | 추가됨, 미연결 | `src/uarm/uarm/sony_tg50_hardware.h` |
| Sony 컨트롤러 MMIO·GPIO·키 매트릭스 | C++ 1차 이식·키 매트릭스 오류 수정, **실행 미검증** | `src/uarm/uarm/mmiodev_TG50uc.cpp`, `.h` |
| 네이티브/WASM 빌드 소스 목록 등록 | 추가됨·문법 검사 CI 구성, **빌드 결과 미검증** | `src/uarm/Makefile` |
| AD7873 터치 컨트롤러 | C++ SSP 1차 이식, **미검증** | `src/uarm/uarm/sspdev_AD7873.cpp`, `.h` |
| AN32502A 전원 관리(I²C) | C++ 1차 이식, **미검증** | `src/uarm/uarm/i2cdev_AN32502A.cpp`, `.h` |
| AK4534 오디오 코덱(I²C) | 레지스터 모델 1차 이식, **미검증** | `src/uarm/uarm/i2sdev_AK4534.cpp`, `.h`; PCM 출력 미구현 |
| Memory Stick 컨트롤러 | 레지스터 모델 C++ 1차 이식, **미검증** | `src/uarm/uarm/mmiodev_MemoryStickController.cpp`, `.h`; 카드 미지원 |
| TG50 주변장치 통합 모듈 | C++ 1차 작성, **미연결·미검증** | `src/uarm/uarm/deviceSonyTG50.cpp`, `.h` |
| TG50 기종 유형·기본 설정 | 추가됨, **부팅 불가** | `src/common/device_type5.h`, `device_configuration.cpp`; 17MiB·320×320 |
| TG50 실행 초기화 분기 | 1차 연결, **미검증** | E2 NAND·AC97 대신 TG50 장치 초기화, 터치·키 입력 전달 |
| TG50 RAM·별도 SRAM 매핑 | 1차 연결, **미검증** | 기본 RAM 17MiB, `0xAC000000`에 1KiB SRAM |
| TG50 ROM 부팅·상태 저장 | 미완성 | ROM 형식·시작 과정·장치 상태 직렬화 필요 |
| 네이티브 전체 빌드 CI | 워크플로 추가, **결과 미확인** | `.github/workflows/tg50-native-build.yml` |
| 네이티브 ROM 부팅 | **미실시** | TG50 ROM 및 네이티브 빌드 성공 필요 |
| 브라우저/WASM TG50 화면·입력 | 미구현 | 네이티브 부팅 성공 후 진행 |

**주의:** 현재 Sony 컨트롤러와 주변장치 통합 모듈은 작성됐지만 기기별 초기화 분기까지는 연결됐지만 RAM·SRAM 초기 매핑이 추가됐지만 ROM 부팅과 상태 저장은 여전히 미완성이며 실제 부팅은 검증되지 않았습니다. 따라서 **아직 TG50 ROM을 실행할 수 있는 상태가 아닙니다.**

---

## Development order

1. **Validate the controller port compiles** in the existing CloudpilotEmu build, without changing E2 or other supported devices. Address build errors and write a small MMIO/keyboard regression test.
2. **Validate the staged AD7873 touch port** and port remaining TG50 hardware: the staged AN32502A power model, the staged AK4534 audio codec, and the staged Memory Stick register model.
3. **Register TG50 for native execution**: device type, ROM and RAM mapping, GPIO initialization, PXA25x revision and host keyboard mapping. Keep it out of the browser device list at this stage.
4. **Prove a real TG50 ROM boot** in the native emulator and test display, touch and QWERTY keyboard.
5. **Add browser/WASM support** after native boot, then handle savestates and optional peripherals.

Detailed file-level checklist: [docs/sony-tg50-port.md](docs/sony-tg50-port.md).

## 개발 순서

1. **컨트롤러 코드 컴파일 확인:** 기존 E2 등 지원 기종을 변경하지 않고 빌드 오류를 점검하며, MMIO·키보드 회귀 시험을 추가합니다.
2. **TG50 주변장치 이식:** AD7873 터치 이식본을 검증하고 AN32502A 전원 모델을 검증하고 AK4534 오디오 코덱 이식본을 검증하고 Memory Stick 레지스터 모델을 검증합니다.
3. **네이티브용 TG50 기기 등록:** 기기 유형, ROM·RAM 매핑, GPIO, PXA25x 리비전, 호스트 키보드 매핑을 연결합니다. 이 단계까지는 브라우저 기종 목록에 추가하지 않습니다.
4. **실제 TG50 ROM 부팅 검증:** 네이티브 에뮬레이터에서 부팅한 뒤 화면, 터치, QWERTY 키보드를 시험합니다.
5. **브라우저/WASM 지원:** 네이티브 부팅 성공 후 웹 기기 선택과 입력을 추가하고, 저장 상태 및 선택적 주변장치는 후속으로 구현합니다.

파일별 상세 점검표: [TG50 포팅 기술 문서](docs/sony-tg50-port.md).

---

## Branch and upstream

Active development branch: `feature/sony-tg50`; `master` is the original fork baseline. This is an independent experimental fork, **not an official CloudpilotEmu release**.

The original project's emulator overview, device compatibility, existing build instructions and licensing information have been preserved in [doc/README-upstream.md](doc/README-upstream.md). Refer to that document for the underlying CloudpilotEmu project. Existing functionality and licensing obligations remain applicable to this fork.

## 개발 브랜치와 원본 프로젝트

개발 브랜치는 `feature/sony-tg50`이며, `master`는 원본 포크 기준 상태로 유지합니다. 본 프로젝트는 **공식 CloudpilotEmu 배포판이 아닌 독립적인 실험용 포크**입니다.

기존 CloudpilotEmu의 소개, 지원 기종, 빌드 안내 및 라이선스 설명은 [원본 README 보존 문서](doc/README-upstream.md)에 유지했습니다. 기존 기능과 라이선스 의무는 이 포크에도 적용됩니다.

---

## TG50 GPIO13 / GPIO14 (upstream-compatible)
The original uARM initializes GPIO13 high (inactive) and connects the Sony microcontroller's IRQ output to GPIO14. These are **two separate lines**. Its annotation about GPIO13 being the overall uC IRQ is tentative; it does not supersede the implemented GPIO14 connection. This fork reproduces the original line separation and initializes both pins inactive before the controller begins driving GPIO14. A real ROM trace should still verify interrupt edge masks and key handling.

## TG50 GPIO13 / GPIO14 (원본 동작 재현)
원본 uARM은 GPIO13을 비활성(High)으로 초기화하고 Sony 컨트롤러의 IRQ 출력을 GPIO14에 연결합니다. 두 선은 **서로 다른 신호선**입니다. GPIO13이 uC 전체 IRQ일 수 있다는 주석은 추정이며, 실제 초기화 코드의 GPIO14 연결과 모순된다고 볼 근거는 아닙니다. 현재 포크도 두 선을 분리하고 컨트롤러가 GPIO14를 구동하기 전에 두 핀을 비활성 상태로 초기화합니다. 향후 ROM 추적으로 엣지 설정과 키 입력 처리를 검증해야 합니다.

---

## GPIO interrupt diagnostics

A diagnostic-only build may define `TG50_TRACE_GPIO` (PXA GPIO13/14 levels and GRER0/GFER0/GEDR0 state) together with `TG50_TRACE_IRQ` (Sony controller IRQ transitions). Compare the results with original uARM using the same ROM. **The diagnostic build and ROM boot remain unverified.**

## GPIO 인터럽트 진단

진단용 빌드에서는 `TG50_TRACE_GPIO`(PXA GPIO13·14 입력 상태와 GRER0·GFER0·GEDR0 설정) 및 `TG50_TRACE_IRQ`(Sony 컨트롤러 인터럽트 전이)를 함께 사용할 수 있습니다. 동일한 TG50 ROM을 사용하는 원본 uARM과 기록을 비교하는 용도입니다. **진단 빌드 및 ROM 부팅 성공은 아직 검증되지 않았습니다.**

---

## Experimental native TG50 ROM launch

The native emulator now offers an explicit device override for a Palm OS 5 NOR image:

```sh
./src/uarm/cp-uarm --tg50 /path/to/TG50-ROM.bin
```

This does not identify arbitrary Sony ROM files automatically or bypass ROM-format validation. It defaults to 17 MiB RAM, and does not support NAND or resuming existing session files in TG50 override mode. **Native compilation and TG50 boot are not yet confirmed.**

## 실험용 TG50 네이티브 ROM 실행

Palm OS 5 NOR 이미지로 TG50 기기를 명시적으로 선택하는 옵션이 추가됐습니다.

```sh
./src/uarm/cp-uarm --tg50 /path/to/TG50-ROM.bin
```

이 옵션은 Sony ROM을 자동 판별하거나 ROM 형식 검사를 생략하지 않습니다. 기본 RAM은 17MiB이며, TG50 강제 선택 모드에서는 별도 NAND 및 기존 세션 파일을 지원하지 않습니다. **네이티브 컴파일 성공과 TG50 ROM 부팅은 아직 확인되지 않았습니다.**
