import { Injectable } from '@angular/core';
import { Vz90Status } from '@native-common/index';

import { NativeSupportService } from './native-support.service';

// Description written next to the bundle by tools/vz90-dal/gen.py at build time (part of the offline precache).
export interface Vz90DalManifest {
    source: string;
    sourceCommit: string;
    sourceDirty: boolean;
    buildId: string;
    sha256: string;
    size: number;
    toolchain: string;
    options: string;
    // boot, amdd, amdi as placed in a rebuilt image: lets a saved session be compared without the bundle
    resources?: { type: string; length: number; sha256: string }[];
}

export interface Vz90Dal {
    bundle: Uint8Array;
    buildId: string;
    sha256: string;
    manifest?: Vz90DalManifest;
}

const BASE = 'vz90-dal/';
// the bundle is fetched only when a VZ90 ROM needs it and then kept here, so that later VZ90 ROMs also work offline
const BUNDLE_CACHE = 'cloudpilot-vz90-dal';
// where the image builder puts the DAL resources (vz90_pv_image.cpp): boot at its link address, amdd and amdi where
// the card header points
const BOOT_OFFSET = 0x230;
const AMDD_POINTER = 0x78;
const AMDI_POINTER = 0xac;

// The development option ?vz90dal=select is read when this module is loaded, before the router replaces the URL, and
// kept for the browser tab.
const MANUAL_SELECTION = ((): boolean => {
    try {
        if (new URLSearchParams(window.location.search).get('vz90dal') === 'select') {
            sessionStorage.setItem('vz90dal', 'select');
        }

        return sessionStorage.getItem('vz90dal') === 'select';
    } catch {
        return false;
    }
})();

function hex(buffer: ArrayBuffer): string {
    return Array.from(new Uint8Array(buffer), (b) => b.toString(16).padStart(2, '0')).join('');
}

function rd32(data: Uint8Array, offset: number): number {
    return (data[offset] | (data[offset + 1] << 8) | (data[offset + 2] << 16) | (data[offset + 3] << 24)) >>> 0;
}

// The Sony VZ90 DAL built into this build (pv-vz90-dal, GPL-3.0-or-later, pinned submodule external/pv-vz90-dal).
// The bundle is a separate static file, fetched only when a VZ90 ROM is rebuilt, and must match the SHA-256 recorded
// at build time. Restoring a session never needs it.
@Injectable({ providedIn: 'root' })
export class Vz90DalService {
    constructor(private nativeSupportService: NativeSupportService) {}

    // rejects with a message for the user if the built-in DAL is missing or does not match its recorded hash
    builtin(): Promise<Vz90Dal> {
        if (!this.builtinPromise) {
            this.builtinPromise = this.loadBuiltin();
            this.builtinPromise.catch(() => (this.builtinPromise = undefined));
        }

        return this.builtinPromise;
    }

    // development override: a DAL bundle file chosen by hand (enabled with ?vz90dal=select)
    isManualSelectionEnabled(): boolean {
        return MANUAL_SELECTION;
    }

    async describe(bundle: Uint8Array): Promise<Vz90Dal> {
        const info = await this.nativeSupportService.describeVz90Dal(bundle);
        if (info.status !== Vz90Status.ok || !info.buildId || !info.sha256) throw new Error(info.message);

        return { bundle, buildId: info.buildId, sha256: info.sha256 };
    }

    // For a saved VZ90 session: true if its image carries the built-in DAL, false if another DAL, undefined if this
    // cannot be told (not a VZ90 image, no DAL description in this build, no Web Crypto). Uses only the build's DAL
    // description; the bundle is not fetched.
    async sessionHasBuiltinDal(rom: Uint8Array): Promise<boolean | undefined> {
        if (!(await this.nativeSupportService.hasVz90Marker(rom))) return undefined;

        const manifest = await this.manifest().catch(() => undefined);
        if (manifest?.resources?.length !== 3 || !globalThis.crypto?.subtle) return undefined;

        const offsets = [BOOT_OFFSET, rd32(rom, AMDD_POINTER), rd32(rom, AMDI_POINTER)];
        for (let i = 0; i < 3; i++) {
            const { length, sha256 } = manifest.resources[i];
            if (offsets[i] + length > rom.length) return false;

            const digest = await crypto.subtle.digest('SHA-256', rom.subarray(offsets[i], offsets[i] + length));
            if (hex(digest) !== sha256) return false;
        }

        return true;
    }

    private manifest(): Promise<Vz90DalManifest> {
        if (!this.manifestPromise) {
            this.manifestPromise = fetch(`${BASE}vz90-dal.json`, { cache: 'no-cache' }).then((response) => {
                if (!response.ok) throw new Error(`HTTP ${response.status}`);

                return response.json() as Promise<Vz90DalManifest>;
            });
            this.manifestPromise.catch(() => (this.manifestPromise = undefined));
        }

        return this.manifestPromise;
    }

    private async loadBuiltin(): Promise<Vz90Dal> {
        let manifest: Vz90DalManifest;
        try {
            manifest = await this.manifest();
        } catch (e) {
            console.warn(e);
            throw new Error('this build does not include the VZ90 DAL, or it could not be loaded');
        }

        // network first; the copy kept from an earlier VZ90 ROM is used when offline (it is checked like a fresh one)
        let bundle = await this.fetchBundle().catch((e: unknown) => {
            console.warn('VZ90 DAL: fetch failed, trying the kept copy', e);
            return undefined;
        });
        let fromCache = false;
        if (!bundle) {
            bundle = await this.cachedBundle();
            fromCache = true;
        }
        if (!bundle) throw new Error('the VZ90 DAL could not be loaded (it is downloaded once, while online)');

        const dal = await this.describe(bundle).catch((e: Error) => {
            throw new Error(`the built-in VZ90 DAL is invalid (${e.message})`);
        });
        if (dal.sha256 !== manifest.sha256) {
            throw new Error('the built-in VZ90 DAL does not match the SHA-256 recorded at build time');
        }

        if (!fromCache) void this.keepBundle(bundle);

        return { ...dal, manifest };
    }

    private async fetchBundle(): Promise<Uint8Array> {
        const response = await fetch(`${BASE}vz90-dal.bundle`, { cache: 'no-cache' });
        if (!response.ok) throw new Error(`HTTP ${response.status}`);

        return new Uint8Array(await response.arrayBuffer());
    }

    private async cachedBundle(): Promise<Uint8Array | undefined> {
        try {
            const response = await (await caches.open(BUNDLE_CACHE)).match(`${BASE}vz90-dal.bundle`);

            return response ? new Uint8Array(await response.arrayBuffer()) : undefined;
        } catch (e) {
            console.warn('VZ90 DAL: no kept copy', e);
            return undefined;
        }
    }

    private async keepBundle(bundle: Uint8Array): Promise<void> {
        try {
            await (
                await caches.open(BUNDLE_CACHE)
            ).put(
                `${BASE}vz90-dal.bundle`,
                new Response(bundle.slice(), { headers: { 'content-type': 'application/octet-stream' } }),
            );
        } catch (e) {
            console.warn('VZ90 DAL: could not keep a copy for offline use', e);
        }
    }

    private builtinPromise: Promise<Vz90Dal> | undefined;
    private manifestPromise: Promise<Vz90DalManifest> | undefined;
}
