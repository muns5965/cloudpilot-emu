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

void* Vz90Builder::GetImage() { return image.data.get(); }

long Vz90Builder::GetImageSize() const { return static_cast<long>(image.size); }

void Vz90Builder::ReleaseImage() {
    image.data.reset();
    image.size = 0;
}

const char* Vz90Builder::GetMessage() const { return message.c_str(); }
