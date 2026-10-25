import { KeyId } from '@native-uarm/index';

/**
 * Maps a host key (KeyboardEvent.code) to a device key for devices with a hardware
 * keyboard (Sony TG50). Mirrors the native SDL mapping in src/uarm/native/SdlEventHandler.cpp.
 */
export function hardwareKeysFromCode(code: string): Array<KeyId> | undefined {
    // Digits are Blue + Q..P on the device; send that chord.
    const digit = /^(?:Digit|Numpad)([0-9])$/.exec(code);
    if (digit) {
        const letter = 'PQWERTYUIO'.charCodeAt(Number(digit[1])) - 'A'.charCodeAt(0);

        return [KeyId.keyIdKbdBlue, KeyId.keyIdKbdA + letter];
    }

    const key = hardwareKeyFromCode(code);

    return key === undefined ? undefined : [key];
}

export function hardwareKeyFromCode(code: string): KeyId | undefined {
    const letter = /^Key([A-Z])$/.exec(code);
    if (letter) return KeyId.keyIdKbdA + (letter[1].charCodeAt(0) - 'A'.charCodeAt(0));

    switch (code) {
        case 'Space':
            return KeyId.keyIdKbdSpace;

        case 'Enter':
        case 'NumpadEnter':
            return KeyId.keyIdKbdEnter;

        case 'Backspace':
            return KeyId.keyIdKbdBackspace;

        case 'ShiftLeft':
        case 'ShiftRight':
            return KeyId.keyIdKbdShift;

        case 'ControlLeft':
        case 'ControlRight':
            return KeyId.keyIdKbdCtrl;

        case 'Tab':
            return KeyId.keyIdKbdTab;

        case 'CapsLock':
            return KeyId.keyIdKbdCaps;

        case 'Comma':
            return KeyId.keyIdKbdComma;

        case 'Period':
            return KeyId.keyIdKbdPeriod;

        case 'Minus':
            return KeyId.keyIdKbdUnderscore;

        case 'F7':
            return KeyId.keyIdKbdBlue;

        case 'F9':
            return KeyId.keyIdKbdRed;

        case 'Home':
            return KeyId.keyIdHome;

        case 'PageUp':
            return KeyId.keyIdPageUp;

        case 'PageDown':
            return KeyId.keyIdPageDown;

        case 'End':
            return KeyId.keyIdJogBack;

        case 'F11':
            return KeyId.keyIdSelect;

        case 'F5':
            return KeyId.keyIdVoice;

        case 'Backquote':
            return KeyId.keyIdGraffiti;

        case 'F1':
            return KeyId.keyIdHard1;

        case 'F2':
            return KeyId.keyIdHard2;

        case 'F3':
            return KeyId.keyIdHard3;

        case 'F4':
            return KeyId.keyIdHard4;

        case 'ArrowUp':
            return KeyId.keyIdUp;

        case 'ArrowDown':
            return KeyId.keyIdDown;

        case 'ArrowLeft':
            return KeyId.keyIdLeft;

        case 'ArrowRight':
            return KeyId.keyIdRight;

        case 'Escape':
            return KeyId.keyIdPower;

        default:
            return undefined;
    }
}
