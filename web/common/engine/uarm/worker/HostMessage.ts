import { EngineSettings } from '@common/engine/EngineSettings';
import { PalmButton } from '@native/cloudpilot_web';

import { UarmSnapshot } from './Snapshot';
import { RpcRequest } from './rpc';

export const enum HostMessageType {
    rpcCall = 'rpcCall',
    updateSettings = 'updateSettings',
    buttonUp = 'buttonUp',
    buttonDown = 'buttonDown',
    keyUp = 'keyUp',
    keyDown = 'keyDown',
    penUp = 'penUp',
    penDown = 'penDown',
    returnFrame = 'returnFrame',
    returnSnapshot = 'returnSnapshot',
    setBackgrounded = 'setBackgrounded',
    netlibResponse = 'netlibResponse',
    reset = 'reset',
}

export type HostMessageRpcCall = RpcRequest & {
    type: HostMessageType.rpcCall;
};

export interface HostMessageUpdateSettings {
    type: HostMessageType.updateSettings;
    settings: EngineSettings;
}

export interface HostMessageButtonUp {
    type: HostMessageType.buttonUp;
    button: PalmButton;
}

export interface HostMessageButtonDown {
    type: HostMessageType.buttonDown;
    button: PalmButton;
}

export interface HostMessageKeyUp {
    type: HostMessageType.keyUp;
    key: number;
}

export interface HostMessageKeyDown {
    type: HostMessageType.keyDown;
    key: number;
}

export interface HostMessagePenUp {
    type: HostMessageType.penUp;
}

export interface HostMessagePenDown {
    type: HostMessageType.penDown;
    x: number;
    y: number;
}

export interface HostMessageReturnFrame {
    type: HostMessageType.returnFrame;
    frame: ArrayBuffer;
}

export interface HostMessageReturnSnapshot {
    type: HostMessageType.returnSnapshot;
    snapshot: UarmSnapshot;
    success: boolean;
}

export interface HostMessageSetBackgrounded {
    type: HostMessageType.setBackgrounded;
    backgrounded: boolean;
}

export interface HostMessageNetlibResponse {
    type: HostMessageType.netlibResponse;
    data: Uint8Array;
}

export type HostMessage =
    | HostMessageRpcCall
    | HostMessageUpdateSettings
    | HostMessageButtonUp
    | HostMessageButtonDown
    | HostMessageKeyUp
    | HostMessageKeyDown
    | HostMessagePenUp
    | HostMessagePenDown
    | HostMessageReturnFrame
    | HostMessageReturnSnapshot
    | HostMessageSetBackgrounded
    | HostMessageNetlibResponse;
