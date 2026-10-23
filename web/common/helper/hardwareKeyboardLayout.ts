import { DeviceId } from '@common/model/DeviceId';
import { KeyId } from '@native-uarm/index';

/**
 * On-screen keyboard for devices with a hardware keyboard (Sony TG50, NX70V), drawn below the
 * hard buttons. Positions are in device pixels relative to the top left of the keyboard area.
 * Keys are drawn as plain rounded rectangles with text labels; no device artwork is used.
 */
export interface OnScreenKey {
    key: KeyId;
    label: string;
    // Characters typed with Blue / Fn and with Red / Alt held (as the ROM produces them)
    fnLabel?: string;
    altLabel?: string;
    // Shift, Blue/Fn, Red/Alt and Ctrl latch until the next key (the ROMs do not latch them)
    modifier?: boolean;
    // Draw a colored bar instead of the label (TG50 Blue and Red keys)
    bar?: string;
    // Label color
    color?: string;
    x: number;
    y: number;
    width: number;
    height: number;
}

type KeySpec = Omit<OnScreenKey, 'x' | 'y' | 'width' | 'height'> & {
    // Left edge and width in the units of the layout (default width: keyWidth)
    x: number;
    w?: number;
};

interface LayoutSpec {
    // Width of the keyboard and key size in layout units
    width: number;
    keyWidth: number;
    keyHeight: number;
    rowPitch: number;
    rows: Array<Array<KeySpec>>;
}

const COLOR_BLUE = '#1565c0';
const COLOR_RED = '#c62828';
const COLOR_ALT = '#6a1b9a';

function letter(c: string, x: number, fnLabel?: string, altLabel?: string): KeySpec {
    return {
        key: KeyId.keyIdKbdA + (c.charCodeAt(0) - 'A'.charCodeAt(0)),
        label: c,
        x,
        fnLabel: fnLabel || undefined,
        altLabel: altLabel || undefined,
    };
}

// TG50: key positions measured on the keyboard drawing in the TG50 "Read This First" manual
// (p. 94): key pitch 79.5, key size 56, row pitch 80.75. Only the geometry is used. Blue
// characters are what the TG50 ROM types with Blue held (checked in the emulator).
const TG50_X = [0, 80, 159, 239, 318, 398, 477, 557, 637, 716];
const TG50_X_OFFSET = [40, 119, 199, 278, 358, 438, 517, 597, 676];

const TG50: LayoutSpec = {
    width: 772,
    keyWidth: 56,
    keyHeight: 56,
    rowPitch: 80.75,
    rows: [
        'QWERTYUIOP'.split('').map((c, i) => letter(c, TG50_X[i], '1234567890'[i])),
        'ASDFGHJKL'.split('').map((c, i) => letter(c, TG50_X_OFFSET[i], '#<>()\\$€£'[i])),
        [
            { key: KeyId.keyIdKbdTab, label: 'Tab', x: 0, w: 96 },
            ...'ZXCVBNM'.split('').map((c, i) => letter(c, TG50_X_OFFSET[i + 1], ' °%+/*='[i].trim())),
            { key: KeyId.keyIdKbdBackspace, label: 'BS', x: 676, w: 96, fnLabel: 'Del' },
        ],
        [
            { key: KeyId.keyIdKbdCtrl, label: 'Ctrl', x: 40, w: 95, modifier: true },
            { key: KeyId.keyIdKbdBlue, label: 'Blue', x: 159, w: 96, modifier: true, bar: COLOR_BLUE },
            { key: KeyId.keyIdKbdUnderscore, label: '@_', x: 278 },
            { key: KeyId.keyIdKbdComma, label: ',;', x: 358 },
            { key: KeyId.keyIdKbdPeriod, label: '.:', x: 438 },
            { key: KeyId.keyIdUp, label: '▲', x: 517 },
            { key: KeyId.keyIdKbdEnter, label: '↵', x: 597, w: 135 },
        ],
        [
            { key: KeyId.keyIdKbdShift, label: 'Shift', x: 40, w: 95, modifier: true },
            { key: KeyId.keyIdKbdRed, label: 'Red', x: 159, w: 96, modifier: true, bar: COLOR_RED },
            { key: KeyId.keyIdKbdSpace, label: 'Space', x: 278, w: 136 },
            { key: KeyId.keyIdLeft, label: '◀', x: 438 },
            { key: KeyId.keyIdDown, label: '▼', x: 517 },
            { key: KeyId.keyIdRight, label: '▶', x: 597 },
            { key: KeyId.keyIdKbdCaps, label: 'Caps', x: 676 },
        ],
    ],
};

// NX70V: key positions measured on a front photo of a European NX70V (perspective corrected
// per row); unit = key pitch, round keys 0.77, row pitch 1.25. Fn is the TG50 Blue key, Alt the
// Red key; `<,` `>.` `:;` sit on the TG50 `@_` `.` `,` matrix positions. Fn and Alt characters
// are what the NX70V ROM (European) types (checked in the emulator).
const NX_X = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9];
const NX_X_OFFSET = [0.5, 1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5, 8.5];

const NX70V: LayoutSpec = {
    width: 9.77,
    keyWidth: 0.77,
    keyHeight: 0.77,
    rowPitch: 1.25,
    rows: [
        'QWERTYUIOP'.split('').map((c, i) => letter(c, NX_X[i], '1234567890'[i], ' é€ßëêèÇçà'[i].trim())),
        'ASDFGHJKL'.split('').map((c, i) => letter(c, NX_X_OFFSET[i], '~!%"\'()\\_'[i], '  Üüï'[i]?.trim())),
        [
            { key: KeyId.keyIdKbdTab, label: 'Tab', x: NX_X_OFFSET[0] },
            ...'ZXCVBNM'.split('').map((c, i) => letter(c, NX_X_OFFSET[i + 1], '@+*#$-/'[i], 'ÖöÄä£'[i])),
            { key: KeyId.keyIdKbdBackspace, label: 'BS', x: NX_X_OFFSET[8], fnLabel: 'Del' },
        ],
        [
            { key: KeyId.keyIdKbdCtrl, label: 'Ctrl', x: 0.5, modifier: true },
            { key: KeyId.keyIdKbdBlue, label: 'Fn', x: 1.5, modifier: true, color: COLOR_BLUE },
            { key: KeyId.keyIdKbdRed, label: 'Alt', x: 2.5, modifier: true, color: COLOR_ALT },
            { key: KeyId.keyIdKbdUnderscore, label: '<,', x: 3.5 },
            { key: KeyId.keyIdKbdPeriod, label: '>.', x: 4.5, fnLabel: '?' },
            { key: KeyId.keyIdKbdComma, label: ':;', x: 5.5, fnLabel: '&' },
            { key: KeyId.keyIdUp, label: '▲', x: 6.5 },
            { key: KeyId.keyIdKbdEnter, label: '↵', x: 7.5, w: 1.77, fnLabel: '=' },
        ],
        [
            { key: KeyId.keyIdKbdShift, label: 'Shift', x: 0.5, w: 1.77, modifier: true },
            { key: KeyId.keyIdKbdSpace, label: 'Space', x: 3.035, w: 1.73 },
            { key: KeyId.keyIdLeft, label: '◀', x: 5.5 },
            { key: KeyId.keyIdDown, label: '▼', x: 6.5 },
            { key: KeyId.keyIdRight, label: '▶', x: 7.5 },
            { key: KeyId.keyIdKbdCaps, label: 'Caps', x: 8.5, fnLabel: 'Num' },
        ],
    ],
};

// Device buttons that are not on the keyboard, in a row of their own below it. The row starts
// after a gap on the left, where the PWA places its floating menu button.
const EXTRA_KEYS: Array<{ key: KeyId; label: string }> = [
    { key: KeyId.keyIdHome, label: 'Home' },
    { key: KeyId.keyIdGraffiti, label: 'Graf' },
    { key: KeyId.keyIdVoice, label: 'Voice' },
    { key: KeyId.keyIdPageUp, label: 'Jog▲' },
    { key: KeyId.keyIdSelect, label: 'Jog●' },
    { key: KeyId.keyIdPageDown, label: 'Jog▼' },
    { key: KeyId.keyIdJogBack, label: 'Back' },
];

const DEVICE_WIDTH = 320;
const MARGIN = 4;
const EXTRA_ROW_HEIGHT = 22;
// Room for the Fn / Alt labels printed below the last keyboard row
const LABEL_SPACE = 7;
const EXTRA_ROW_LEFT_GAP = 40;

function layoutSpec(deviceId: DeviceId): LayoutSpec | undefined {
    switch (deviceId) {
        case DeviceId.pegTG50:
            return TG50;

        case DeviceId.pegNX70V:
            return NX70V;

        default:
            return undefined;
    }
}

function scaleForWidth(spec: LayoutSpec, width: number): number {
    return (width - 2 * MARGIN) / spec.width;
}

function mainHeight(spec: LayoutSpec, width: number): number {
    return 2 * MARGIN + scaleForWidth(spec, width) * ((spec.rows.length - 1) * spec.rowPitch + spec.keyHeight);
}

function buildLayout(spec: LayoutSpec, width: number): Array<OnScreenKey> {
    const s = scaleForWidth(spec, width);
    const keys: Array<OnScreenKey> = [];

    spec.rows.forEach((row, rowIndex) => {
        for (const { x, w, ...key } of row) {
            keys.push({
                ...key,
                x: MARGIN + s * x,
                y: MARGIN + s * rowIndex * spec.rowPitch,
                width: s * (w ?? spec.keyWidth),
                height: s * spec.keyHeight,
            });
        }
    });

    const extraTop = mainHeight(spec, width) + LABEL_SPACE;
    const extraWidth = (width - EXTRA_ROW_LEFT_GAP - MARGIN) / EXTRA_KEYS.length;

    EXTRA_KEYS.forEach((key, i) =>
        keys.push({
            ...key,
            x: EXTRA_ROW_LEFT_GAP + i * extraWidth,
            y: extraTop,
            width: extraWidth,
            height: EXTRA_ROW_HEIGHT - MARGIN,
        }),
    );

    return keys;
}

export function onScreenKeyboardHeight(deviceId: DeviceId): number {
    const spec = layoutSpec(deviceId);

    return spec ? Math.ceil(mainHeight(spec, DEVICE_WIDTH) + LABEL_SPACE + EXTRA_ROW_HEIGHT) : 0;
}

export function onScreenKeyboardLayout(deviceId: DeviceId, width: number): Array<OnScreenKey> {
    const spec = layoutSpec(deviceId);

    return spec ? buildLayout(spec, width) : [];
}

// Key under the point; a touch between keys snaps to the nearest key within a few pixels.
export function onScreenKeyAt(keys: Array<OnScreenKey>, x: number, y: number): OnScreenKey | undefined {
    const SNAP = 4;
    let best: OnScreenKey | undefined;
    let bestDistance = SNAP;

    for (const k of keys) {
        const dx = Math.max(k.x - x, 0, x - (k.x + k.width));
        const dy = Math.max(k.y - y, 0, y - (k.y + k.height));
        const distance = Math.hypot(dx, dy);

        if (distance === 0) return k;
        if (distance < bestDistance) {
            best = k;
            bestDistance = distance;
        }
    }

    return best;
}
