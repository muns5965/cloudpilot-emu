export { DisplayMode } from '../../uarm/web/binding/binding';
import { DisplayMode } from '../../uarm/web/binding/binding';

export const enum GunzipState {
    more = 0,
    done = 1,
    error = -1,
}

export const enum GzipState {
    initial = 0,
    more = 1,
    done = 2,
    error = -1,
}

export const enum ZipfileWalkerState {
    error = -1,
    open = 0,
    done = 1,
}

export const enum DeviceType5 {
    deviceTypeE2 = 0,
    deviceTypeFrankenE2 = 1,
    deviceTypePV = 2,
    deviceTypeSonyTG50 = 3,
    deviceTypeSonyNX70V = 4,
    deviceTypeInvalid = -1,
}

export interface GunzipContext<VoidPtr> {
    GetState(): GunzipState;
    Continue(): GunzipState;

    GetUncompressedData(): VoidPtr;
    ReleaseUncompressedData(): VoidPtr;
    GetUncompressedSize(): number;

    GetError(): string;
}

export interface GzipContext<VoidPtr> {
    SetFilename(filename: string): GzipContext<VoidPtr>;
    SetMtime(mtime: number): GzipContext<VoidPtr>;

    Continue(): number;
    GetState(): number;

    GetGzipData(): VoidPtr;
    GetGzipSize(): number;

    GetError(): string;
}

export interface CreateZipContext<VoidPtr> {
    Initialize(compressionLevel: number): boolean;

    AddEntry(name: string): boolean;

    WriteData(data: VoidPtr, size: number): boolean;

    GetZipData(): VoidPtr;
    GetZipDataSize(): number;

    GetLastError(): number;
}

export interface ZipfileWalker<VoidPtr> {
    GetState(): ZipfileWalkerState;
    Next(): ZipfileWalkerState;

    GetTotalEntries(): number;

    GetCurrentEntrySize(): number;
    GetCurrentEntryName(): string;
    GetCurrentEntryContent(): VoidPtr;
}

export interface SessionFile5<VoidPtr> {
    IsSessionFile(size: number, data: VoidPtr): boolean;

    GetDeviceType(): DeviceType5;
    GetDisplayMode(): number;
    SetDeviceType(deviceTyoe: DeviceType5): SessionFile5<VoidPtr>;
    SetDisplayMode(displayMode: DisplayMode): SessionFile5<VoidPtr>;

    GetMetadata(): VoidPtr;
    GetMetadataSize(): number;
    SetMetadata(size: number, data: VoidPtr): SessionFile5<VoidPtr>;

    GetNor(): VoidPtr;
    GetNorSize(): number;
    SetNor(size: number, data: VoidPtr): SessionFile5<VoidPtr>;

    GetNand(): VoidPtr;
    GetNandSize(): number;
    SetNand(size: number, data: VoidPtr): SessionFile5<VoidPtr>;

    GetMemory(): VoidPtr;
    GetMemorySize(): number;
    SetMemory(size: number, data: VoidPtr): SessionFile5<VoidPtr>;

    GetSavestate(): VoidPtr;
    GetSavestateSize(): number;
    SetSavestate(size: number, data: VoidPtr): SessionFile5<VoidPtr>;

    GetRamSize(): number;
    SetRamSize(size: number): SessionFile5<VoidPtr>;

    Serialize(): boolean;
    GetSerializedSession(): VoidPtr;
    GetSerializedSessionSize(): number;

    GetVersion(): number;

    Deserialize(size: number, data: VoidPtr): boolean;
}

export interface RomInfo5<VoidPtr> {
    IsValid(): boolean;

    GetCardName(): string;
    GetManufacturer(): string;
    GetRomName(): string;
    NeedsNand(): boolean;

    GetCompanyId(): number;
    GetHalId(): number;

    GetDeviceType(): DeviceType5;

    GetRecommendedRamSize(): number;
}

// values of Vz90Status in vz90_pv_image.h
export const enum Vz90Status {
    ok = 0,
    errBadRom = 1,
    errUnsupportedVariant = 2,
    errDalMismatch = 3,
    errNoMemory = 4,
}

export interface Vz90Builder<VoidPtr> {
    IsCandidate(rom: VoidPtr, size: number): boolean;
    HasMarker(image: VoidPtr, size: number): boolean;

    Build(rom: VoidPtr, romSize: number, dal: VoidPtr, dalSize: number): Vz90Status;

    DescribeDal(dal: VoidPtr, dalSize: number): Vz90Status;
    GetDalBuildId(): string;
    GetDalSha256(): string;

    GetImage(): VoidPtr;
    GetImageSize(): number;
    ReleaseImage(): void;

    GetMessage(): string;
}

export interface ModuleWithGunzipContext<VoidPtr> {
    GunzipContext: {
        new (data: VoidPtr, size: number, slizeSize: number): GunzipContext<VoidPtr>;
    };
}

export interface ModuleWithGzipContext<VoidPtr> {
    GzipContext: {
        new (data: VoidPtr, size: number, slizeSize: number): GzipContext<VoidPtr>;
    };
}

export interface ModuleWithCreateZipContext<VoidPtr> {
    CreateZipContext: {
        new (): CreateZipContext<VoidPtr>;
    };
}

export interface ModuleWithZipfileWalker<VoidPtr> {
    ZipfileWalker: {
        new (bufferSize: number, buffer: VoidPtr): ZipfileWalker<VoidPtr>;
    };
}

export interface ModuleWithSessionFile5<VoidPtr> {
    SessionFile5: {
        new (): SessionFile5<VoidPtr>;
    };
}

export interface ModuleWithRomInfo5<VoidPtr> {
    RomInfo5: {
        new (buffer: VoidPtr, size: number): RomInfo5<VoidPtr>;
    };
}

export interface ModuleWithVz90Builder<VoidPtr> {
    Vz90Builder: {
        new (): Vz90Builder<VoidPtr>;
    };
}
