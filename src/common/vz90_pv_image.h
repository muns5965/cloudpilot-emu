#ifndef _VZ90_PV_IMAGE_H_
#define _VZ90_PV_IMAGE_H_

// Sony CLIE VZ90: build a PV ROM image in memory from the original VZ90 ROM image and an independent DAL bundle
// (pv-vz90-dal, tools/mkbundle.py). The original image is only read. Module data is copied unchanged; the containers
// (card header, storage header, heap, chunks, database headers, directory) are rebuilt at the PV base, every pointer
// re-targeted by the identity of what it points to, and the DAL's three resources replaced by the bundle's.
// The image carries the ROM token vz9p = "VZ90" so that a saved session is recognised as VZ90 again on restore.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// numeric values are part of the web binding (Vz90Builder::Build) and must not change
enum class Vz90Status { ok = 0, errBadRom = 1, errUnsupportedVariant = 2, errDalMismatch = 3, errNoMemory = 4 };

struct Vz90DalBundle {
    const uint8_t* data{nullptr};  // owned by the caller, read only
    size_t size{0};
};

struct Vz90Image {
    std::unique_ptr<uint8_t[]> data;  // owned by the caller after a successful build
    size_t size{0};
};

struct Vz90Options {
    // ROM serial number token: by default the original image's value is kept (all 0xff on the stock jpJP image).
    // Set to override (e.g. the fixed test value used to reproduce earlier test runs).
    bool overrideSnum{false};
    std::vector<uint8_t> snum;
    // keyboard token knvp: the original image has none; only added when set (test reproduction)
    std::vector<uint8_t> knvp;
};

// sony / bone card header: an original VZ90 ROM image to be rebuilt (variant and structure are checked by the build)
bool Vz90IsCandidate(const uint8_t* rom, size_t romSize);

// PvPv image carrying the token vz9p = "VZ90": run with the VZ90 shared page
bool Vz90HasMarker(const uint8_t* pvImage, size_t size);

// check a DAL bundle (format, bounds, hashes); fills buildId (40 hex digits) on success
Vz90Status Vz90CheckBundle(const Vz90DalBundle& dal, std::string& buildId, std::string& message);

// as Vz90CheckBundle, and also the SHA-256 of the whole bundle file (64 hex digits), to identify the DAL in use
Vz90Status Vz90DescribeBundle(const Vz90DalBundle& dal, std::string& buildId, std::string& sha256Hex,
                              std::string& message);

// whether a rebuilt PV image (e.g. from a saved session) carries the DAL of this bundle: compares the bundle's
// boot, amdd and amdi resources with the image at the places the build put them. 1 = same DAL, 0 = another DAL,
// -1 = not a VZ90 PV image or the bundle is invalid
int Vz90ImageHasDal(const uint8_t* pvImage, size_t size, const Vz90DalBundle& dal);

Vz90Status Vz90BuildPvImage(const uint8_t* rom, size_t romSize, const Vz90DalBundle& dal,
                            const Vz90Options& options, Vz90Image& out, std::string& message);

const char* Vz90StatusName(Vz90Status status);

#endif  // _VZ90_PV_IMAGE_H_
