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
| TG50 device factory, ROM/RAM mapping | Not implemented | Add separate factory dispatch and ROM/RAM configuration |
| Native compile / ROM boot | **Not verified / not attempted** | First validation milestone |
| Browser/WASM TG50 UI and input | Not implemented | After successful native boot |

**Important:** These commits stage the Sony controllers and a peripheral assembly module but do not register or activate TG50 in the emulator. The current fork must **not** be presented as able to run a TG50 ROM.

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
| TG50 기기 생성·ROM/RAM 매핑 | 미구현 | 기기별 팩토리 분기·ROM/RAM 구성 연결 |
| 네이티브 컴파일·ROM 부팅 | **미검증·미시도** | 첫 번째 실행 검증 목표 |
| 브라우저/WASM TG50 화면·입력 | 미구현 | 네이티브 부팅 성공 후 진행 |

**주의:** 현재 Sony 컨트롤러와 주변장치 통합 모듈은 작성됐지만 실제 기기 선택·실행 경로에 연결되지 않았습니다. 따라서 **아직 TG50 ROM을 실행할 수 있는 상태가 아닙니다.**

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
