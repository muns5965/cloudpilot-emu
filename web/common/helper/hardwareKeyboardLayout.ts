import { DeviceId } from '@common/model/DeviceId';
import { KeyId } from '@native-uarm/index';

/**
 * On-screen keyboard for devices with a hardware keyboard (Sony TG50), drawn below the
 * hard buttons. Positions are in device pixels relative to the top left of the keyboard area.
 * Keys are plain rectangles with text labels; no device artwork is used.
 */
export interface OnScreenKey {
    key: KeyId;
    label: string;
    // Secondary label (character with Blue held)
    blueLabel?: string;
    // Shift, Blue, Red and Ctrl latch until the next key (the TG50 ROM does not latch them)
    modifier?: boolean;
    color?: string;
    x: number;
    y: number;
    width: number;
    height: number;
}

interface KeySpec {
    key: KeyId;
    label: string;
    units?: number;
    blueLabel?: string;
    modifier?: boolean;
    color?: string;
}

const UNITS_PER_ROW = 10;
const ROW_HEIGHT = 30;
const SMALL_ROW_HEIGHT = 24;

const COLOR_BLUE = '#1565c0';
const COLOR_RED = '#c62828';

function letter(c: string, blueLabel?: string): KeySpec {
    return { key: KeyId.keyIdKbdA + (c.charCodeAt(0) - 'A'.charCodeAt(0)), label: c, blueLabel };
}

// Rows follow the TG50 keyboard as drawn in the "Read This First" manual (p. 94). Rows narrower
// than ten units are centered. The bottom row holds the device buttons that are not on the
// keyboard (Home, Graffiti, voice, jog dial and Back).
const ROWS: Array<{ height: number; keys: Array<KeySpec> }> = [
    {
        height: ROW_HEIGHT,
        keys: [
            letter('Q', '1'),
            letter('W', '2'),
            letter('E', '3'),
            letter('R', '4'),
            letter('T', '5'),
            letter('Y', '6'),
            letter('U', '7'),
            letter('I', '8'),
            letter('O', '9'),
            letter('P', '0'),
        ],
    },
    {
        height: ROW_HEIGHT,
        keys: 'ASDFGHJKL'.split('').map((c) => letter(c)),
    },
    {
        height: ROW_HEIGHT,
        keys: [
            { key: KeyId.keyIdKbdTab, label: 'Tab', units: 1.5 },
            ...'ZXCVBNM'.split('').map((c) => letter(c)),
            { key: KeyId.keyIdKbdBackspace, label: 'BS', units: 1.5 },
        ],
    },
    {
        height: ROW_HEIGHT,
        keys: [
            { key: KeyId.keyIdKbdCtrl, label: 'Ctrl', units: 1.5, modifier: true },
            { key: KeyId.keyIdKbdBlue, label: 'Blue', units: 1.5, modifier: true, color: COLOR_BLUE },
            { key: KeyId.keyIdKbdUnderscore, label: '@_' },
            { key: KeyId.keyIdKbdComma, label: ',' },
            { key: KeyId.keyIdKbdPeriod, label: '.' },
            { key: KeyId.keyIdUp, label: '▲' },
            { key: KeyId.keyIdKbdEnter, label: '↵', units: 2.5 },
        ],
    },
    {
        height: ROW_HEIGHT,
        keys: [
            { key: KeyId.keyIdKbdShift, label: 'Shift', units: 1.5, modifier: true },
            { key: KeyId.keyIdKbdRed, label: 'Red', units: 1.5, modifier: true, color: COLOR_RED },
            { key: KeyId.keyIdKbdSpace, label: 'Space', units: 2.5 },
            { key: KeyId.keyIdLeft, label: '◀' },
            { key: KeyId.keyIdDown, label: '▼' },
            { key: KeyId.keyIdRight, label: '▶' },
            { key: KeyId.keyIdKbdCaps, label: 'Caps' },
        ],
    },
    {
        height: SMALL_ROW_HEIGHT,
        keys: [
            { key: KeyId.keyIdHome, label: 'Home', units: 10 / 7 },
            { key: KeyId.keyIdGraffiti, label: 'Graf', units: 10 / 7 },
            { key: KeyId.keyIdVoice, label: 'Voice', units: 10 / 7 },
            { key: KeyId.keyIdPageUp, label: 'Jog ▲', units: 10 / 7 },
            { key: KeyId.keyIdSelect, label: 'Jog ●', units: 10 / 7 },
            { key: KeyId.keyIdPageDown, label: 'Jog ▼', units: 10 / 7 },
            { key: KeyId.keyIdJogBack, label: 'Back', units: 10 / 7 },
        ],
    },
];

function buildLayout(width: number): Array<OnScreenKey> {
    const unit = width / UNITS_PER_ROW;
    const keys: Array<OnScreenKey> = [];

    let y = 0;
    for (const row of ROWS) {
        const rowUnits = row.keys.reduce((acc, spec) => acc + (spec.units ?? 1), 0);
        let x = ((UNITS_PER_ROW - rowUnits) * unit) / 2;

        for (const spec of row.keys) {
            const keyWidth = (spec.units ?? 1) * unit;

            keys.push({
                key: spec.key,
                label: spec.label,
                blueLabel: spec.blueLabel,
                modifier: spec.modifier,
                color: spec.color,
                x,
                y,
                width: keyWidth,
                height: row.height,
            });

            x += keyWidth;
        }

        y += row.height;
    }

    return keys;
}

const KEYBOARD_HEIGHT = ROWS.reduce((acc, row) => acc + row.height, 0);

export function onScreenKeyboardHeight(deviceId: DeviceId): number {
    return deviceId === DeviceId.pegTG50 ? KEYBOARD_HEIGHT : 0;
}

export function onScreenKeyboardLayout(deviceId: DeviceId, width: number): Array<OnScreenKey> {
    return deviceId === DeviceId.pegTG50 ? buildLayout(width) : [];
}

export function onScreenKeyAt(keys: Array<OnScreenKey>, x: number, y: number): OnScreenKey | undefined {
    return keys.find((k) => x >= k.x && x < k.x + k.width && y >= k.y && y < k.y + k.height);
}
