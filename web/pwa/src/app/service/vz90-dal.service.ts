import { Injectable } from '@angular/core';
import { Vz90Status } from '@native-common/index';

import { NativeSupportService } from './native-support.service';

// Description written next to the bundle by tools/vz90-dal/gen.py at build time.
export interface Vz90DalManifest {
    source: string;
    sourceCommit: string;
    sourceDirty: boolean;
    buildId: string;
    sha256: string;
    size: number;
    toolchain: string;
    options: string;
}

export interface Vz90Dal {
    bundle: Uint8Array;
    buildId: string;
    sha256: string;
    manifest?: Vz90DalManifest;
}

const BASE = 'vz90-dal/';

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

// The Sony VZ90 DAL built into this build (pv-vz90-dal, GPL-3.0-or-later, pinned submodule external/pv-vz90-dal).
// It is a separate static file, fetched only when a VZ90 ROM or session needs it; the fetched bundle must match the
// SHA-256 recorded at build time.
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

    private async loadBuiltin(): Promise<Vz90Dal> {
        let manifest: Vz90DalManifest;
        let bundle: Uint8Array;

        try {
            const manifestResponse = await fetch(`${BASE}vz90-dal.json`, { cache: 'no-cache' });
            if (!manifestResponse.ok) throw new Error(`HTTP ${manifestResponse.status}`);
            manifest = (await manifestResponse.json()) as Vz90DalManifest;

            const bundleResponse = await fetch(`${BASE}vz90-dal.bundle`, { cache: 'no-cache' });
            if (!bundleResponse.ok) throw new Error(`HTTP ${bundleResponse.status}`);
            bundle = new Uint8Array(await bundleResponse.arrayBuffer());
        } catch (e) {
            console.warn(e);
            throw new Error('this build does not include the VZ90 DAL, or it could not be loaded');
        }

        const dal = await this.describe(bundle).catch((e: Error) => {
            throw new Error(`the built-in VZ90 DAL is invalid (${e.message})`);
        });
        if (dal.sha256 !== manifest.sha256) {
            throw new Error('the built-in VZ90 DAL does not match the SHA-256 recorded at build time');
        }

        return { ...dal, manifest };
    }

    private builtinPromise: Promise<Vz90Dal> | undefined;
}
