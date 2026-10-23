# CloudpilotEmu — Sony CLIÉ PEG-TG50 port

> **Experimental.** This fork of [CloudpilotEmu](https://github.com/cloudpilot-emu/cloudpilot-emu) runs the **Sony CLIÉ PEG-TG50 (Palm OS 5)**: Setup with pen calibration, the CLIÉ Launcher, apps, the QWERTY keyboard, sleep/resume and session files. It runs in the browser at **[muns5965.github.io/cloudpilot-emu](https://muns5965.github.io/cloudpilot-emu/)** and as a native Linux build. No ROM is included; bring your own TG50 ROM.

[**Technical notes and fix history**](docs/sony-tg50-port.md) · [**NX70V branch**](https://github.com/muns5965/cloudpilot-emu/tree/feature/sony-nx70v) · [**Original CloudpilotEmu README**](doc/README-upstream.md) · [**Upstream repository**](https://github.com/cloudpilot-emu/cloudpilot-emu) · [**Original uARM TG50 code**](https://github.com/uARM-Palm/uARM/blob/master/deviceSonyTG50.c)

[한국어](#cloudpilotemu--sony-clié-peg-tg50-포팅)

## Status (2026-10-23)

Tested with a TG50 NOR ROM whose SHA-256 matches the uARM reference (`c6d5985c…860b94f`).

| Item | Status | Notes |
| --- | --- | --- |
| ROM detection | **Works** | Header company `sony`, HAL `vrna` → TG50, 17 MiB RAM (`src/common/rom_info5.cpp`) |
| Boot, Setup, pen calibration, Launcher | **Works** | Boot fixes: [technical notes](docs/sony-tg50-port.md) |
| Touch (AD7873) | **Works** | Calibrated by Palm OS itself |
| QWERTY keyboard (Sony controller matrix) | **Works** | Typing in Memo Pad; fast typing (40 ms per key) is correct |
| Hard buttons, jog dial keys | **Works** | |
| Power key, sleep/resume | **Works** | Sleep turns the screen off; the next press resumes. RTC alarm wake not checked |
| Session files | **Works** (native) | Also while asleep. Browser save/restore not checked |
| Browser | **Works** (user test on a phone): ROM import, boot, touch, hard buttons | PC keyboard and the on-screen keyboard are new and not yet tested by hand |
| Native window | Works | Still draws the E2 Graffiti silkscreen, which does nothing on TG50 |
| Audio (AK4534), Memory Stick | Register models only | No sound, no media |
| CI | **Passing** | Native build, controller compile check, browser build and deployment |

## Browser

1. Open **https://muns5965.github.io/cloudpilot-emu/**. The ROM file stays in your browser.
2. **Sessions → +**, choose the TG50 NOR ROM. The device shows as **Sony PEG-TG50**, 17 MB RAM.
3. Start the session. Setup 1–2: tap anywhere. Calibration: tap the center of each target. Setup 3: **Next**, Setup 4: **Done**.

**On-screen keyboard** (below the hard buttons, layout after the TG50 manual): tap **Shift**, **Blue**, **Red** or **Ctrl** to latch it (yellow) for the next key, e.g. Blue then W types `2`. The TG50 ROM only applies these keys while they are held, so the on-screen keyboard holds them together with the next key. The bottom row has Home, Graffiti, Voice, the jog dial (▲ ● ▼) and Back.

**PC keyboard** (same as native):

| PC key | TG50 |
| --- | --- |
| Letters, Space, Enter, Backspace, Shift, Ctrl, Tab, Caps Lock, `,` `.` `-` | Same keys (`-` = `@_` key) |
| F7 / F9 | Blue / Red (hold together with the key) |
| F1–F4 | Hard buttons 1–4 |
| Arrows | Arrow keys |
| PageUp / PageDown / F11 / End | Jog up / down / select, Back |
| Home | Home |
| F5 / `` ` `` | Voice / Graffiti |
| Esc | Power (sleep/wake) |
| Ctrl+Shift | Game mode on/off (keys drive the hard buttons) |

PC digit keys are not mapped; use Blue + top row. While the emulator runs, F5 does not reload the page.

## Native build

Linux with SDL2 (`build-essential libsdl2-dev libsdl2-image-dev`):

```sh
make -C src/common bin -j4
make -C src/uarm bin -j4
./src/uarm/cp-uarm /path/to/TG50-ROM.bin
```

The ROM is identified automatically; `--tg50` forces TG50. Type `save-session <file>` in the terminal to save, and start `cp-uarm` with the session file to resume. Keys are the same as in the browser table above.

## Next

1. Hand test of the on-screen keyboard and PC keyboard in the browser; browser session save/restore.
2. RTC alarm wake; optionally PC digit keys → Blue + top row.
3. Native window without the E2 silkscreen.
4. Audio and Memory Stick.

NX70V work continues on [`feature/sony-nx70v`](https://github.com/muns5965/cloudpilot-emu/tree/feature/sony-nx70v) (native only so far).

## Notes

- Branch: `feature/sony-tg50`; `master` keeps the upstream code at the fork point, with a short landing README for this fork. This is an independent experimental fork, **not an official CloudpilotEmu release**. Upstream documentation and licensing: [doc/README-upstream.md](doc/README-upstream.md).
- Never commit or publish ROM files or session files.
- Browser deployment: `.github/workflows/fork-pages.yml` builds WASM + PWA on each push to `feature/sony-tg50` (or manually). One-time setup: Settings → Pages → Source = GitHub Actions; Settings → Environments → github-pages must allow the branch.

---

# CloudpilotEmu — Sony CLIÉ PEG-TG50 포팅

> **실험 단계입니다.** [CloudpilotEmu](https://github.com/cloudpilot-emu/cloudpilot-emu)를 포크해 **Sony CLIÉ PEG-TG50(Palm OS 5)** 을 실행합니다. 펜 보정을 포함한 Setup, CLIÉ 런처, 앱, QWERTY 키보드, 절전·복귀, 세션 파일이 동작합니다. 브라우저 **[muns5965.github.io/cloudpilot-emu](https://muns5965.github.io/cloudpilot-emu/)** 와 Linux 네이티브 빌드에서 실행됩니다. ROM은 포함하지 않으므로 TG50 ROM을 직접 준비해야 합니다.

[**기술 문서·수정 이력(영문)**](docs/sony-tg50-port.md) · [**NX70V 브랜치**](https://github.com/muns5965/cloudpilot-emu/tree/feature/sony-nx70v) · [**CloudpilotEmu 기존 README(영문)**](doc/README-upstream.md) · [**원본 저장소**](https://github.com/cloudpilot-emu/cloudpilot-emu) · [**원본 uARM TG50 코드**](https://github.com/uARM-Palm/uARM/blob/master/deviceSonyTG50.c)

## 현황(2026-10-23)

uARM 기준 해시(`c6d5985c…860b94f`)와 같은 TG50 NOR ROM으로 확인했습니다.

| 항목 | 상태 | 비고 |
| --- | --- | --- |
| ROM 인식 | **동작** | 헤더 회사 `sony`, HAL `vrna` → TG50, RAM 17MiB(`src/common/rom_info5.cpp`) |
| 부팅, Setup, 펜 보정, 런처 | **동작** | 부팅 문제 해결 과정: [기술 문서](docs/sony-tg50-port.md) |
| 터치(AD7873) | **동작** | Palm OS 자체 보정 사용 |
| QWERTY 키보드(Sony 마이컴 매트릭스) | **동작** | 메모장 입력 확인, 빠른 입력(키당 40ms)도 정확 |
| 하드 버튼, 조그 다이얼 키 | **동작** | |
| 전원 키, 절전·복귀 | **동작** | 절전하면 화면이 꺼지고 다시 누르면 복귀. 알람(RTC)으로 깨우기는 미확인 |
| 세션 파일 | **동작**(네이티브) | 절전 중 저장도 가능. 브라우저 저장·복원은 미확인 |
| 브라우저 | **동작**(휴대폰에서 사용자 시험): ROM 불러오기, 부팅, 터치, 하드 버튼 | PC 키보드와 화면 키보드는 새로 넣었고 아직 직접 시험 전 |
| 네이티브 창 | 동작 | E2 그래피티 실크스크린이 아직 그려지며 TG50에서는 기능 없음 |
| 오디오(AK4534), Memory Stick | 레지스터 모델만 | 소리·카드 없음 |
| CI | **통과** | 네이티브 빌드, 마이컴 컴파일 검사, 브라우저 빌드·배포 |

## 브라우저 실행

1. **https://muns5965.github.io/cloudpilot-emu/** 를 엽니다. ROM 파일은 브라우저 안에서만 쓰입니다.
2. **Sessions → +** 에서 TG50 NOR ROM을 고릅니다. 기종 **Sony PEG-TG50**, RAM 17MB로 표시됩니다.
3. 세션을 실행합니다. Setup 1~2는 아무 곳이나 누르고, 펜 보정은 표적 중심을 누르고, Setup 3은 **Next**, Setup 4는 **Done**을 누릅니다.

**화면 키보드**(하드 버튼 아래, TG50 매뉴얼 배열): **Shift**, **Blue**, **Red**, **Ctrl**을 탭하면 노란색으로 켜지고 다음 키 한 번에 적용됩니다. 예: Blue 다음 W → `2`. TG50 ROM은 이 키들을 누른 채로 있을 때만 적용하므로, 화면 키보드가 다음 키와 함께 눌린 상태로 보냅니다. 맨 아래 줄은 Home, Graffiti, Voice, 조그 다이얼(▲ ● ▼), Back입니다.

**PC 키보드**(네이티브와 같음):

| PC 키 | TG50 |
| --- | --- |
| 글자, Space, Enter, Backspace, Shift, Ctrl, Tab, Caps Lock, `,` `.` `-` | 같은 키(`-` = `@_` 키) |
| F7 / F9 | Blue / Red(다음 키와 함께 누른 채로) |
| F1~F4 | 하드 버튼 1~4 |
| 방향키 | 방향 키 |
| PageUp / PageDown / F11 / End | 조그 위 / 아래 / 선택, Back |
| Home | 홈 |
| F5 / `` ` `` | 음성 / 그래피티 |
| Esc | 전원(절전·복귀) |
| Ctrl+Shift | 게임 모드 켜기·끄기(키로 하드 버튼을 누름) |

PC 숫자 키는 연결되지 않았으니 Blue + 윗줄을 씁니다. 에뮬레이터 실행 중에는 F5로 페이지가 새로고침되지 않습니다.

## 네이티브 빌드

Linux, SDL2 필요(`build-essential libsdl2-dev libsdl2-image-dev`):

```sh
make -C src/common bin -j4
make -C src/uarm bin -j4
./src/uarm/cp-uarm /path/to/TG50-ROM.bin
```

ROM은 자동 인식되며 `--tg50`으로 강제할 수도 있습니다. 터미널에 `save-session <파일>`을 입력하면 저장되고, 세션 파일로 `cp-uarm`을 실행하면 이어서 동작합니다. 키는 위 브라우저 표와 같습니다.

## 다음 단계

1. 브라우저에서 화면 키보드·PC 키보드 직접 시험, 브라우저 세션 저장·복원 확인.
2. 알람(RTC)으로 깨우기, 필요하면 PC 숫자 키 → Blue + 윗줄.
3. 네이티브 창에서 E2 실크스크린 제거.
4. 오디오와 Memory Stick.

NX70V 작업은 [`feature/sony-nx70v`](https://github.com/muns5965/cloudpilot-emu/tree/feature/sony-nx70v)에서 진행합니다(아직 네이티브 전용).

## 참고

- 개발 브랜치는 `feature/sony-tg50`, `master`는 포크 시점의 원본 코드 그대로이며, README만 이 포크 안내로 바꿨습니다. **공식 CloudpilotEmu 배포판이 아닌 독립 실험용 포크**입니다. 원본 문서와 라이선스: [doc/README-upstream.md](doc/README-upstream.md).
- ROM 파일과 세션 파일은 절대 커밋·공개하지 않습니다.
- 브라우저 배포: `.github/workflows/fork-pages.yml`이 `feature/sony-tg50` push 때마다(또는 수동 실행) WASM과 PWA를 빌드해 올립니다. 처음 한 번 설정: Settings → Pages → Source = GitHub Actions, Settings → Environments → github-pages에서 이 브랜치 허용.
