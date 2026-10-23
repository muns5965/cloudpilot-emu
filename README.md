# CloudpilotEmu fork — Sony CLIÉ (Palm OS 5)

> **Experimental, unofficial fork** of [CloudpilotEmu](https://github.com/cloudpilot-emu/cloudpilot-emu) that adds Sony CLIÉ handhelds to its Palm OS 5 (uARM) engine. This is **not an official CloudpilotEmu release**. No ROM is included; bring your own.

[한국어](#cloudpilotemu-포크--sony-clié-palm-os-5)

## Try it in the browser

**[muns5965.github.io/cloudpilot-emu](https://muns5965.github.io/cloudpilot-emu/)** — Sony PEG-TG50 and PEG-NX70V. Open **Sessions → +** and choose your ROM; the device is detected from it. For the NX70V, the settings dialog also asks for a TG50 ROM, which supplies the missing boot ROM. ROM files stay in your browser.

## Branches

| Branch | Device | Runs in | Status |
| --- | --- | --- | --- |
| [`feature/sony-tg50`](https://github.com/muns5965/cloudpilot-emu/tree/feature/sony-tg50) | PEG-TG50 (Palm OS 5.0, 320×320, QWERTY keyboard) | Browser and native Linux | Setup, Launcher, apps, keyboard (PC and on-screen), sleep/resume, sessions (native). No sound, no Memory Stick media |
| [`feature/sony-nx70v`](https://github.com/muns5965/cloudpilot-emu/tree/feature/sony-nx70v) | PEG-NX70V (Palm OS 5.0, 320×480) | Browser and native Linux | Setup, Launcher, apps, keyboard (PC and on-screen), sleep/resume, sessions (native). Boots in the browser (phone test). The known dump lacks the small boot ROM, so a TG50 ROM supplies it at run time (`--boot-rom`) |
| `master` | — | — | Upstream code at the fork point; only this README differs |

Each branch README has the details, keys and build steps.

## Notes

- ROM files and session files are never committed or published.
- The upstream README (emulator overview, supported devices, build, license) is kept in [doc/README-upstream.md](doc/README-upstream.md). Upstream: [cloudpilot-emu/cloudpilot-emu](https://github.com/cloudpilot-emu/cloudpilot-emu). Sony hardware models are ported from [uARM](https://github.com/uARM-Palm/uARM).

---

# CloudpilotEmu 포크 — Sony CLIÉ (Palm OS 5)

> [CloudpilotEmu](https://github.com/cloudpilot-emu/cloudpilot-emu)의 Palm OS 5(uARM) 엔진에 Sony CLIÉ 기종을 추가하는 **실험용 비공식 포크**입니다. **공식 CloudpilotEmu 배포판이 아닙니다.** ROM은 포함하지 않으므로 직접 준비해야 합니다.

## 브라우저에서 실행

**[muns5965.github.io/cloudpilot-emu](https://muns5965.github.io/cloudpilot-emu/)** — Sony PEG-TG50, PEG-NX70V. **Sessions → +** 에서 ROM을 고르면 기종이 자동으로 인식됩니다. NX70V는 설정 창에서 빠진 부트 ROM을 채울 TG50 ROM을 하나 더 고릅니다. ROM 파일은 브라우저 안에서만 쓰입니다.

## 브랜치

| 브랜치 | 기종 | 실행 환경 | 현황 |
| --- | --- | --- | --- |
| [`feature/sony-tg50`](https://github.com/muns5965/cloudpilot-emu/tree/feature/sony-tg50) | PEG-TG50(Palm OS 5.0, 320×320, QWERTY 키보드) | 브라우저, Linux 네이티브 | Setup, 런처, 앱, 키보드(PC·화면 키보드), 절전·복귀, 세션(네이티브). 소리·Memory Stick 카드 없음 |
| [`feature/sony-nx70v`](https://github.com/muns5965/cloudpilot-emu/tree/feature/sony-nx70v) | PEG-NX70V(Palm OS 5.0, 320×480) | 브라우저, Linux 네이티브 | Setup, 런처, 앱, 키보드(PC·화면 키보드), 절전·복귀, 세션(네이티브). 브라우저 부팅 확인(휴대폰 시험). 알려진 덤프에 작은 부트 ROM이 없어 실행 때 TG50 ROM에서 빌려 씀(`--boot-rom`) |
| `master` | — | — | 포크 시점의 원본 코드 그대로, 이 README만 다름 |

자세한 내용, 키 연결, 빌드 방법은 각 브랜치 README에 있습니다.

## 참고

- ROM 파일과 세션 파일은 절대 커밋·공개하지 않습니다.
- 원본 README(에뮬레이터 소개, 지원 기종, 빌드, 라이선스)는 [doc/README-upstream.md](doc/README-upstream.md)에 보존했습니다. 원본 저장소: [cloudpilot-emu/cloudpilot-emu](https://github.com/cloudpilot-emu/cloudpilot-emu). Sony 하드웨어 모델은 [uARM](https://github.com/uARM-Palm/uARM)에서 이식했습니다.
