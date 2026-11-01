#ifndef _VZ90_BUILDER_H_
#define _VZ90_BUILDER_H_

// Web-facing wrapper around vz90_pv_image (WebIDL binding: web/vz90_builder.idl). The caller passes the original
// ROM and the DAL bundle as buffers it owns; Build() keeps the rebuilt image in this object until ReleaseImage()
// or destruction, so the caller copies it out and then releases it.

#include <cstddef>
#include <string>

#include "vz90_pv_image.h"

class Vz90Builder {
   public:
    Vz90Builder() = default;

    // first card header is sony/bone: an original VZ90 ROM to rebuild (structure and variant checked by Build)
    bool IsCandidate(const void* rom, long size) const;

    // PvPv image with the VZ90 marker token
    bool HasMarker(const void* image, long size) const;

    // returns a Vz90Status value (0 = ok); GetMessage() describes the result
    long Build(const void* rom, long romSize, const void* dal, long dalSize);

    // check a DAL bundle without building; on success GetDalBuildId()/GetDalSha256() describe it (Vz90Status value)
    long DescribeDal(const void* dal, long dalSize);
    const char* GetDalBuildId() const;
    const char* GetDalSha256() const;

    void* GetImage();
    long GetImageSize() const;
    void ReleaseImage();

    const char* GetMessage() const;

   private:
    Vz90Image image;
    std::string message;
    std::string dalBuildId;
    std::string dalSha256;

    Vz90Builder(const Vz90Builder&) = delete;
    Vz90Builder& operator=(const Vz90Builder&) = delete;
};

#endif  // _VZ90_BUILDER_H_
