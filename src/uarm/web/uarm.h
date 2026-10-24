#ifndef __UARM_H__
#define __UARM_H__

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>
#include <deque>
#include <optional>
#include <string>

#include "MainLoop.h"
#include "device.h"
#include "device_configuration.h"
#include "device_type5.h"

class SoC;
struct AudioQueue;
class DbBackup;

class Uarm {
   public:
    explicit Uarm() = default;

    Uarm& SetRamSize(unsigned int size);
    Uarm& SetDisplayMode(unsigned int displayMode);
    Uarm& SetDeviceType(int deviceType);
    Uarm& SetNand(unsigned int size, void* data);
    Uarm& SetMemory(unsigned int size, void* data);
    Uarm& SetSavestate(unsigned int size, void* data);
    Uarm& SetSd(unsigned int size, void* data, const char* id);
    Uarm& SetDefaultMips(unsigned int defaultMips);

    bool Launch(unsigned int romSize, void* romData);

    double Cycle(uint64_t now);

    void* GetFrame();
    uint32_t GetFirstDirtyLine();
    uint32_t GetLastDirtyLine();
    uint32_t GetFrameSize();
    void ResetFrame();

    uint32_t GetTimesliceSizeUsec();

    void PenDown(int x, int y);
    void PenUp();

    uint32_t CurrentIps();
    uint64_t CurrentIpsMax();
    void SetMaxLoad(unsigned int maxLoad);
    void SetCyclesPerSecondLimit(unsigned int cyclesPerSecondLimit);

    uint64_t GetTimestampUsec();

    void KeyDown(int key);
    void KeyUp(int key);

    uint32_t PendingSamples();
    uint32_t* PopQueuedSamples();
    uint32_t GetSampleQueueSize();
    void SetPcmOutputEnabled(bool enabled);
    void SetPcmSuspended(bool suspended);

    uint32_t GetRomDataSize();
    void* GetRomData();

    uint32_t GetNandDataSize();
    void* GetNandData();
    void* GetNandDirtyPages();
    bool IsNandDirty();
    void SetNandDirty(bool isDirty);

    uint32_t GetSdCardDataSize();
    void* GetSdCardData();
    void* GetSdCardDirtyPages();
    bool IsSdCardDirty();
    void SetSdCardDirty(bool isDirty);

    uint32_t GetMemoryDataSize();
    void* GetMemoryData();
    void* GetMemoryDirtyPages();

    uint32_t GetDeviceType();

    bool SdCardInsert(void* data, int length, const char* id);
    void SdCardEject();
    bool SdCardInitialized();
    const char* GetSdCardId();
    void Reset();

    void Save();
    uint32_t GetSavestateSize();
    void* GetSavestateData();
    bool IsSdInserted();

    uint32_t GetRamSize();

    void JamKey(int key, uint32_t durationMsec);

    uint32_t InstallDatabase(uint32_t len, void* data);

    DbBackup* NewDbBackup(int type);

    bool IsUiInitialized();
    bool IsOsVersionSet();
    uint32_t GetOsVersion();

    bool IsLcdEnabled();

    bool LaunchAppByName(const char* name);
    bool LaunchAppByDbHeader(void* header, int len);

    // NetLib redirection (68k NetLib calls answered by the host, see uarm/netlib_redirect.h).
    // Events for the host: 0 none, 1 session open, 2 session close, 3 request (data below).
    void SetNetlibEnabled(bool enabled);
    int NetlibTakeEvent();
    uint32_t NetlibGetRequestSize();
    void* NetlibGetRequestData();
    void NetlibDeliver(void* data, int size);

   private:
    size_t ramSize{0};

    std::optional<DisplayMode> displayMode;
    // Device type of the session; overrides the ROM header (an NX70V image carries a TG50 boot ROM)
    std::optional<DeviceType5> requestedDeviceType;

    size_t nandSize{0};
    uint8_t* nandData{nullptr};

    size_t memorySize{0};
    uint8_t* memoryData{nullptr};

    size_t savestateSize{0};
    uint8_t* savestateData{nullptr};

    size_t sdSize{0};
    uint8_t* sdData{nullptr};
    std::string sdId;

    uint32_t firstDirtyLine{0};
    uint32_t lastDirtyLine{0};

    uint32_t defaultMips{0};
    DeviceType5 deviceType{DeviceType5::deviceTypeInvalid};

    SoC* soc{nullptr};
    AudioQueue* audioQueue{nullptr};

    std::unique_ptr<MainLoop> mainLoop;

    std::deque<std::pair<int, std::vector<uint8_t>>> netlibEvents;
    std::vector<uint8_t> netlibRequest;

   private:
    Uarm(const Uarm&) = delete;
    Uarm(Uarm&&) = delete;
    Uarm& operator=(const Uarm&) = delete;
    Uarm& operator=(Uarm&&) = delete;
};

#endif  // __UARM_H__
