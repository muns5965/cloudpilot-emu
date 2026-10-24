import { Injectable } from '@angular/core';
import { NetlibEvent, NetlibEventType } from '@common/bridge/Uarm';
import { Mutex } from 'async-mutex';

import { NetworkBackend } from './network-backend/network-backend';
import { NetworkBackendFactory } from './network-backend/network-backend-factory.service';

export type DeliverResponseT = (data: Uint8Array) => void;

// Network integration for Palm OS 5 (uARM): the core redirects 68k NetLib calls and sends the
// encoded requests here. They run on the same backend as the OS 4 integration (proxy or native);
// the responses go back to the core. An empty response makes the call fail.
@Injectable({ providedIn: 'root' })
export class UarmNetworkService {
    constructor(private backendFactory: NetworkBackendFactory) {}

    handleEvent(event: NetlibEvent, deliver: DeliverResponseT): void {
        switch (event.type) {
            case NetlibEventType.open:
                this.openSession(deliver);
                break;

            case NetlibEventType.close:
                this.reset();
                break;

            case NetlibEventType.request:
                void this.handleRequest(event.data, deliver);
                break;

            default:
                event satisfies never;
        }
    }

    reset(): void {
        const backend = this.backend;
        if (!backend) return;

        backend.rpcResultEvent.removeHandler(this.onRpcResult);
        backend.unexpectedCloseEvent.removeHandler(this.onUnexpectedClose);

        this.backend = undefined;
        this.connected = undefined;

        void this.mutex.runExclusive(() => backend.disconnect());
    }

    private openSession(deliver: DeliverResponseT): void {
        this.reset();

        this.deliver = deliver;
        this.backend = this.backendFactory.createBackend();
        if (!this.backend) return;

        this.backend.rpcResultEvent.addHandler(this.onRpcResult);
        this.backend.unexpectedCloseEvent.addHandler(this.onUnexpectedClose);

        const backend = this.backend;
        this.connected = this.mutex.runExclusive(async () => {
            try {
                return await backend.connect();
            } catch (e) {
                console.error('failed to connect to backend', e);

                return false;
            }
        });
    }

    private async handleRequest(data: Uint8Array, deliver: DeliverResponseT): Promise<void> {
        const backend = this.backend;
        const connected = this.connected;

        if (!backend || !connected || !(await connected) || backend !== this.backend) return deliver(new Uint8Array(0));

        const success = await this.mutex.runExclusive(async () => {
            try {
                return await backend.handleRpc(data);
            } catch (e) {
                console.error('failed to process rpc request', e);

                return false;
            }
        });

        if (!success) deliver(new Uint8Array(0));
    }

    private onRpcResult = (data: Uint8Array) => this.deliver?.(data);

    private onUnexpectedClose = () => {
        this.deliver?.(new Uint8Array(0));
        this.reset();
    };

    private backend: NetworkBackend | undefined;
    private connected: Promise<boolean> | undefined;
    private deliver: DeliverResponseT | undefined;

    private mutex = new Mutex();
}
