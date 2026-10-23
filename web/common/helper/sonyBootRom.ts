// PEG-NX70V: the known ROM dump holds device data, not the small boot ROM, at 0x0-0x1FFFF. The
// boot ROM of a TG50 NOR image (first 0x18000 bytes) boots it (same as native --boot-rom). The
// combined image only lives in the browser's own storage.

const PALM_CARD_SIGNATURE = 0xfeedbeef;
const COMPANY_ID_SONY = 0x736f6e79; // 'sony'

export const SONY_BOOT_ROM_SIZE = 0x18000;

function read32(data: Uint8Array, offset: number): number {
    return (data[offset] | (data[offset + 1] << 8) | (data[offset + 2] << 16) | (data[offset + 3] << 24)) >>> 0;
}

export function hasPalmCardHeaderAt(data: Uint8Array, offset: number): boolean {
    return data.length >= offset + 0x88 && read32(data, offset + 8) === PALM_CARD_SIGNATURE;
}

// A ROM that needs a boot ROM: no card header at 0x0, but one at 0x20000.
export function needsSonyBootRom(rom: Uint8Array): boolean {
    return !hasPalmCardHeaderAt(rom, 0) && hasPalmCardHeaderAt(rom, 0x20000);
}

// A Sony NOR image with a boot ROM at 0x0 (e.g. a TG50 ROM).
export function isSonyBootRomSource(data: Uint8Array): boolean {
    return data.length >= SONY_BOOT_ROM_SIZE && hasPalmCardHeaderAt(data, 0) && read32(data, 0x80) === COMPANY_ID_SONY;
}

export function applySonyBootRom(rom: Uint8Array, bootRomSource: Uint8Array): Uint8Array {
    const combined = rom.slice();
    combined.set(bootRomSource.subarray(0, SONY_BOOT_ROM_SIZE), 0);

    return combined;
}
