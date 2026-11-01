#include "vz90_builder.h"

#include <cstdint>

bool Vz90Builder::IsCandidate(const void* rom, long size) const {
    return rom && size > 0 && Vz90IsCandidate(static_cast<const uint8_t*>(rom), static_cast<size_t>(size));
}

bool Vz90Builder::HasMarker(const void* image, long size) const {
    return image && size > 0 && Vz90HasMarker(static_cast<const uint8_t*>(image), static_cast<size_t>(size));
}

long Vz90Builder::Build(const void* rom, long romSize, const void* dal, long dalSize) {
    ReleaseImage();

    if (romSize < 0 || dalSize < 0) {
        message = "invalid buffer size";
        return static_cast<long>(Vz90Status::errBadRom);
    }

    const Vz90Status status =
        Vz90BuildPvImage(static_cast<const uint8_t*>(rom), static_cast<size_t>(romSize),
                         {static_cast<const uint8_t*>(dal), static_cast<size_t>(dalSize)}, Vz90Options(), image,
                         message);

    return static_cast<long>(status);
}

long Vz90Builder::DescribeDal(const void* dal, long dalSize) {
    dalBuildId.clear();
    dalSha256.clear();
    if (!dal || dalSize <= 0) {
        message = "DAL bundle: no data";
        return static_cast<long>(Vz90Status::errDalMismatch);
    }
    return static_cast<long>(Vz90DescribeBundle({static_cast<const uint8_t*>(dal), static_cast<size_t>(dalSize)},
                                                dalBuildId, dalSha256, message));
}

const char* Vz90Builder::GetDalBuildId() const { return dalBuildId.c_str(); }

const char* Vz90Builder::GetDalSha256() const { return dalSha256.c_str(); }

long Vz90Builder::ImageHasDal(const void* image, long size, const void* dal, long dalSize) const {
    if (!image || size <= 0 || !dal || dalSize <= 0) return -1;
    return Vz90ImageHasDal(static_cast<const uint8_t*>(image), static_cast<size_t>(size),
                           {static_cast<const uint8_t*>(dal), static_cast<size_t>(dalSize)});
}

void* Vz90Builder::GetImage() { return image.data.get(); }

long Vz90Builder::GetImageSize() const { return static_cast<long>(image.size); }

void Vz90Builder::ReleaseImage() {
    image.data.reset();
    image.size = 0;
}

const char* Vz90Builder::GetMessage() const { return message.c_str(); }
