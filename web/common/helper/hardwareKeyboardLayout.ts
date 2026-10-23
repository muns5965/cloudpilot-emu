import { DeviceId } from '@common/model/DeviceId';
import { KeyId } from '@native-uarm/index';

/**
 * On-screen keyboard for devices with a hardware keyboard (Sony TG50), drawn below the
 * hard buttons. Positions are in device pixels relative to the top left of the keyboard area.
 * Keys are drawn as plain rounded rectangles with text labels; no device artwork is used.
 */
export interface OnScreenKey {
    key: KeyId;
    label: string;
    // Character typed with Blue held (as the TG50 ROM produces it)
    blueLabel?: string;
    // Shift, Blue, Red and Ctrl latch until the next key (the TG50 ROM does not latch them)
    modifier?: boolean;
    // Draw a colored bar instead of the label (Blue and Red keys)
    bar?: string;
    x: number;
    y: number;
    width: number;
    height: number;
}

interface KeySpec {
    key: KeyId;
    label: string;
    // Position and size in the units of the measurement (see GEOMETRY below)
    x: number;
    w?: number;
    blueLabel?: string;
    modifier?: boolean;
    bar?: string;
}

// Key positions measured on the keyboard drawing in the TG50 "Read This First" manual (p. 94):
// key pitch 79.5, key size 56, row pitch 80.75 (units of the measurement). Only the geometry is
// used. Blue characters are what the TG50 ROM types with Blue held (checked in the emulator).
const GEOMETRY = { width: 772, keyWidth: 56, keyHeight: 56, rowPitch: 80.75 };

const COLOR_BLUE = '#1565c0';
const COLOR_RED = '#c62828';

const KEY_X = [0, 80, 159, 239, 318, 398, 477, 557, 637, 716];
const KEY_X_OFFSET = [40, 119, 199, 278, 358, 438, 517, 597, 676];

function letter(c: string, x: number, blueLabel?: string): KeySpec {
    return { key: KeyId.keyIdKbdA + (c.charCodeAt(0) - 'A'.charCodeAt(0)), label: c, x, blueLabel };
}

const ROWS: Array<Array<KeySpec>> = [
    'QWERTYUIOP'.split('').map((c, i) => letter(c, KEY_X[i], '1234567890'[i])),
    'ASDFGHJKL'.split('').map((c, i) => letter(c, KEY_X_OFFSET[i], '#<>()\\$€£'[i])),
    [
        { key: KeyId.keyIdKbdTab, label: 'Tab', x: 0, w: 96 },
        ...'ZXCVBNM'.split('').map((c, i) => letter(c, KEY_X_OFFSET[i + 1], ' °%+/*='[i].trim() || undefined)),
        { key: KeyId.keyIdKbdBackspace, label: 'BS', x: 676, w: 96, blueLabel: 'Del' },
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
];

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
const EXTRA_ROW_LEFT_GAP = 40;

function scaleForWidth(width: number): number {
    return (width - 2 * MARGIN) / GEOMETRY.width;
}

function mainHeight(width: number): number {
    const s = scaleForWidth(width);

    return 2 * MARGIN + s * ((ROWS.length - 1) * GEOMETRY.rowPitch + GEOMETRY.keyHeight);
}

function buildLayout(width: number): Array<OnScreenKey> {
    const s = scaleForWidth(width);
    const keys: Array<OnScreenKey> = [];

    ROWS.forEach((row, rowIndex) => {
        for (const spec of row) {
            keys.push({
                key: spec.key,
                label: spec.label,
                blueLabel: spec.blueLabel,
                modifier: spec.modifier,
                bar: spec.bar,
                x: MARGIN + s * spec.x,
                y: MARGIN + s * rowIndex * GEOMETRY.rowPitch,
                width: s * (spec.w ?? GEOMETRY.keyWidth),
                height: s * GEOMETRY.keyHeight,
            });
        }
    });

    const extraTop = mainHeight(width);
    const extraWidth = (width - EXTRA_ROW_LEFT_GAP - MARGIN) / EXTRA_KEYS.length;

    EXTRA_KEYS.forEach((spec, i) =>
        keys.push({
            key: spec.key,
            label: spec.label,
            x: EXTRA_ROW_LEFT_GAP + i * extraWidth,
            y: extraTop,
            width: extraWidth,
            height: EXTRA_ROW_HEIGHT - MARGIN,
        }),
    );

    return keys;
}

export function onScreenKeyboardHeight(deviceId: DeviceId): number {
    return deviceId === DeviceId.pegTG50 ? Math.ceil(mainHeight(DEVICE_WIDTH) + EXTRA_ROW_HEIGHT) : 0;
}

export function onScreenKeyboardLayout(deviceId: DeviceId, width: number): Array<OnScreenKey> {
    return deviceId === DeviceId.pegTG50 ? buildLayout(width) : [];
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
