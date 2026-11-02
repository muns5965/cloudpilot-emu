#include "vz90_pv_image.h"

#include <cstring>
#include <new>
#include <unordered_map>

#include "md5.h"

using namespace std;

namespace {
    constexpr uint32_t PV_BASE = 0x10000000;
    constexpr uint32_t PV_ROM_SIZE = 0x01500000;
    constexpr uint32_t SIG_CARD = 0xfeedbeef;
    constexpr uint32_t SIG_STORE = 0xfeedface;
    constexpr uint32_t OWNER_HEADER = 0xf1000000;
    constexpr uint32_t OWNER_DATA = 0xf2000000;
    constexpr uint32_t DAL_LINK = 0x10000230;
    constexpr size_t TOKEN_AREA = 0xc0, TOKEN_AREA_END = 0x100;

    // original images this loader has been verified with (MD5 of the whole file)
    const char* const SUPPORTED[] = {
        "c034b590bd59547664fff254bc995840",  // CLIE PEG-VZ90, Japanese (fullrom.jpJP)
        "c8ddf3aff8aba8da030af1a22c69a713",  // CLIE PEG-VZ90, English "mini", modified (RomFlash.VZ fullrom.mini.enUS)
        // not listed: RomFlash.VZ fullrom.full.enUS (e148cc70...): checked less, and the CLIE Organizer To Do "Free handle"
        // error (also seen with the Japanese ROM, see README) was observed there first and more often
    };

    uint16_t rd16(const uint8_t* p) { return p[0] | (p[1] << 8); }
    uint32_t rd32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (uint32_t(p[3]) << 24); }
    void wr16(uint8_t* p, uint16_t v) { p[0] = v; p[1] = v >> 8; }
    void wr32(uint8_t* p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }

    // four-character codes are stored little-endian in these images: "boot" reads as 'toob'
    uint32_t fourcc(const char* s) { return (uint32_t(s[0]) << 24) | (s[1] << 16) | (s[2] << 8) | s[3]; }

    // ---- SHA-256 (FIPS 180-4) ----
    struct Sha256 {
        uint32_t h[8]{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                      0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
        uint8_t buf[64];
        size_t fill{0};
        uint64_t total{0};

        static uint32_t ror(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

        void block(const uint8_t* p) {
            static const uint32_t k[64] = {
                0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
                0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
                0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
                0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
                0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
                0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
                0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
                0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
                0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
                0xc67178f2};
            uint32_t w[64];
            for (int i = 0; i < 16; i++)
                w[i] = (uint32_t(p[4 * i]) << 24) | (p[4 * i + 1] << 16) | (p[4 * i + 2] << 8) | p[4 * i + 3];
            for (int i = 16; i < 64; i++) {
                uint32_t s0 = ror(w[i - 15], 7) ^ ror(w[i - 15], 18) ^ (w[i - 15] >> 3);
                uint32_t s1 = ror(w[i - 2], 17) ^ ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
                w[i] = w[i - 16] + s0 + w[i - 7] + s1;
            }
            uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
            for (int i = 0; i < 64; i++) {
                uint32_t t1 = hh + (ror(e, 6) ^ ror(e, 11) ^ ror(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
                uint32_t t2 = (ror(a, 2) ^ ror(a, 13) ^ ror(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
                hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
            }
            h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
        }

        void update(const uint8_t* p, size_t n) {
            total += n;
            while (n) {
                size_t m = min(n, sizeof(buf) - fill);
                memcpy(buf + fill, p, m);
                fill += m; p += m; n -= m;
                if (fill == 64) { block(buf); fill = 0; }
            }
        }

        void final(uint8_t out[32]) {
            uint64_t bits = total * 8;
            uint8_t pad = 0x80, zero = 0;
            update(&pad, 1);
            while (fill != 56) update(&zero, 1);
            uint8_t len[8];
            for (int i = 0; i < 8; i++) len[i] = bits >> (56 - 8 * i);
            update(len, 8);
            for (int i = 0; i < 8; i++) wr32(out + 4 * i, __builtin_bswap32(h[i]));
        }
    };

    void sha256(const uint8_t* p, size_t n, uint8_t out[32]) {
        Sha256 s;
        s.update(p, n);
        s.final(out);
    }

    // ---- original image reader ----
    struct Item {
        uint32_t src{0};       // original data address
        uint8_t meta[8]{};     // resource: type + id + attr (8) / record: attr + uniqueID (4)
        const uint8_t* data{nullptr};
        size_t length{0};
        uint32_t placed{0};    // new data address
    };

    struct Db {
        uint32_t addr{0};
        uint8_t header[0x50];
        bool resource{false};
        vector<Item> items;
        uint32_t info[2]{0, 0};  // appInfo, sortInfo (original addresses)
        const uint8_t* infoData[2]{nullptr, nullptr};
        size_t infoLength[2]{0, 0};
        uint32_t placed{0};
    };

    struct Original {
        const uint8_t* b{nullptr};
        size_t size{0};
        size_t card0{0}, store{0};
        uint32_t base{0};
        uint8_t dirTail[2]{};
        vector<Db> dbs;
        string error;

        // [addr, addr + n) lies inside the image; no address arithmetic on unchecked values (64-bit end)
        bool in(uint32_t addr, size_t n) const {
            return addr >= base && size_t(addr - base) <= size && n <= size - (addr - base);
        }

        // data of the chunk that starts at addr (the 8-byte chunk header precedes it)
        bool blob(uint32_t addr, const uint8_t*& data, size_t& length) {
            if (addr < base || addr - base < 8 || !in(addr - 8, 8)) return false;
            uint32_t h = rd32(b + (addr - base) - 8);
            uint32_t total = h & 0xffffff, adj = h >> 24;
            if (total < 8 + adj) return false;
            length = total - 8 - adj;
            if (!in(addr, length)) return false;
            data = b + (addr - base);
            return true;
        }

        bool fail(const string& why) {
            error = why;
            return false;
        }

        bool read(const uint8_t* rom, size_t romSize) {
            b = rom;
            size = romSize;
            vector<size_t> cards;
            for (size_t o = 0; o + 0x200 <= size; o += 0x8000)
                if (rd32(b + o + 8) == SIG_CARD) cards.push_back(o);
            if (cards.empty()) return fail("no card header");
            card0 = cards[0];
            base = rd32(b + card0 + 0x58) & 0xfff00000;

            size_t best = 0;
            uint16_t bestCount = 0;
            for (size_t c : cards) {
                size_t st = c + 0x100;
                if (rd32(b + st) != SIG_STORE) continue;
                uint32_t dir = rd32(b + st + 0x3c);
                if (!in(dir, 8)) continue;
                uint16_t n = rd16(b + (dir - base) + 4);
                if (n > bestCount) {
                    best = st;
                    bestCount = n;
                }
            }
            if (bestCount == 0) return fail("no storage with databases");
            store = best;
            uint32_t dir = rd32(b + store + 0x3c);
            if (!in(dir, 8 + 4 * size_t(bestCount))) return fail("database directory out of range");
            memcpy(dirTail, b + (dir - base) + 6, 2);

            dbs.resize(bestCount);
            for (size_t i = 0; i < bestCount; i++)
                if (!readDb(rd32(b + (dir - base) + 8 + 4 * i), dbs[i])) return false;
            return true;
        }

        bool readDb(uint32_t p, Db& db) {
            if (!in(p, 0x50)) return fail("database header out of range");
            const uint8_t* a = b + (p - base);
            db.addr = p;
            memcpy(db.header, a, 0x50);
            db.resource = rd16(a + 0x20) & 1;
            // one record list only: a database whose list continues in another chunk is not supported
            if (rd32(a + 0x48) != 0) return fail("database with a chained record list");
            uint16_t k = rd16(a + 0x4c);
            size_t entry = db.resource ? 12 : 8;
            if (!in(p, 0x50 + entry * size_t(k))) return fail("database entries out of range");
            db.items.resize(k);
            for (size_t i = 0; i < k; i++) {
                const uint8_t* e = a + 0x50 + entry * i;
                Item& it = db.items[i];
                if (db.resource) {
                    memcpy(it.meta, e, 8);
                    it.src = rd32(e + 8);
                } else {
                    memcpy(it.meta, e + 4, 4);
                    it.src = rd32(e);
                }
                if (!blob(it.src, it.data, it.length)) return fail("database item chunk out of range");
            }
            for (int f = 0; f < 2; f++) {
                db.info[f] = rd32(a + 0x34 + 4 * f);
                if (db.info[f] && !blob(db.info[f], db.infoData[f], db.infoLength[f]))
                    return fail("appInfo/sortInfo chunk out of range");
            }
            return true;
        }

        // original ROM token value (token list of card 0), entries padded to 2 bytes as on the images measured
        bool token(uint32_t name, vector<uint8_t>& value) const {
            uint64_t p = rd32(b + card0 + 0x64);
            for (int guard = 0; guard < 64 && p <= UINT32_MAX && in(uint32_t(p), 8); guard++) {
                const uint8_t* e = b + (p - base);
                uint32_t n = rd32(e), len = rd32(e + 4);
                if (n == 0xffffffff || len > 0x1000 || !in(uint32_t(p), 8 + size_t(len))) break;
                if (n == name) {
                    value.assign(e + 8, e + 8 + len);
                    return true;
                }
                p += 8 + ((uint64_t(len) + 1) & ~uint64_t(1));
            }
            return false;
        }
    };

    bool isResource(const Db& db, const char* type) { return db.resource && rd32(db.header + 0x3c) == fourcc(type); }
    bool hasResource(const Original& r, const char* type, uint16_t id) {
        for (auto& db : r.dbs)
            if (db.resource)
                for (auto& it : db.items)
                    if (rd32(it.meta) == fourcc(type) && rd16(it.meta + 4) == id) return true;
        return false;
    }

    // ---- DAL bundle (format 1, see pv-vz90-dal tools/mkbundle.py) ----
    struct Bundle {
        const uint8_t* res[3]{};
        size_t len[3]{};
        string buildId;
    };

    Vz90Status checkBundle(const Vz90DalBundle& dal, Bundle& out, string& message) {
        const uint8_t* p = dal.data;
        auto bad = [&](const char* why) {
            message = string("DAL bundle: ") + why;
            return Vz90Status::errDalMismatch;
        };
        if (!p || dal.size < 0x100 || memcmp(p, "VZ90DAL\0", 8) != 0) return bad("not a bundle");
        if (rd32(p + 0x08) != 1) return bad("unsupported format version");
        if (rd32(p + 0x0c) != 0x100 || rd32(p + 0x14) != 3) return bad("unexpected header");
        if (rd32(p + 0x10) != DAL_LINK) return bad("boot is not linked at 0x10000230");
        for (size_t i = 0xe0; i < 0x100; i++)
            if (p[i]) return bad("reserved bytes are not zero");

        static const char* const types[3] = {"boot", "amdd", "amdi"};
        size_t end = 0x100;
        for (int i = 0; i < 3; i++) {
            const uint8_t* e = p + 0x30 + 16 * i;
            if (memcmp(e, types[i], 4) != 0 || rd16(e + 4) != 0x4a38 || rd16(e + 6) != 0)
                return bad("unexpected resource table");
            uint32_t off = rd32(e + 8), len = rd32(e + 12);
            // in order, back to back on 4-byte boundaries, inside the file, no overlap
            if (off < end || off - end > 3 || off % 4 || len == 0 || len > dal.size || off > dal.size - len)
                return bad("resource out of range or overlapping");
            for (size_t z = end; z < off; z++)  // padding before a resource is zero (format 1)
                if (p[z]) return bad("padding bytes are not zero");
            uint8_t h[32];
            sha256(p + off, len, h);
            if (memcmp(h, p + 0x60 + 32 * i, 32) != 0) return bad("resource hash mismatch");
            out.res[i] = p + off;
            out.len[i] = len;
            end = off + len;
        }
        if (end != dal.size) return bad("trailing data");

        Sha256 s;
        static const uint8_t zero[32] = {};
        s.update(p, 0xc0);
        s.update(zero, 32);
        s.update(p + 0xe0, dal.size - 0xe0);
        uint8_t h[32];
        s.final(h);
        if (memcmp(h, p + 0xc0, 32) != 0) return bad("bundle hash mismatch");

        static constexpr char hex[] = "0123456789abcdef";
        out.buildId.clear();
        for (int i = 0; i < 20; i++) {
            out.buildId += hex[p[0x18 + i] >> 4];
            out.buildId += hex[p[0x18 + i] & 15];
        }
        if (rd32(p + 0x2c) & 1) out.buildId += "-dirty";
        return Vz90Status::ok;
    }

    vector<uint8_t> token(const char* name, const vector<uint8_t>& value) {
        vector<uint8_t> t(8);
        wr32(t.data(), fourcc(name));
        wr32(t.data() + 4, value.size());
        t.insert(t.end(), value.begin(), value.end());
        if (value.size() & 1) t.push_back(0);
        return t;
    }
}  // namespace

const char* Vz90StatusName(Vz90Status status) {
    switch (status) {
        case Vz90Status::ok:
            return "ok";
        case Vz90Status::errBadRom:
            return "invalid VZ90 ROM";
        case Vz90Status::errUnsupportedVariant:
            return "unsupported VZ90 ROM variant";
        case Vz90Status::errDalMismatch:
            return "DAL bundle mismatch";
        case Vz90Status::errNoMemory:
            return "out of memory";
    }
    return "?";
}

// Decided by the identity of the first card header only (as RomInfo5 identifies a ROM): a combined image whose first
// card is something else is not a candidate, even if a later card were sony/bone.
bool Vz90IsCandidate(const uint8_t* rom, size_t romSize) {
    if (!rom) return false;
    for (size_t o = 0; o + 0x100 <= romSize; o += 0x8000)
        if (rd32(rom + o + 8) == SIG_CARD)
            return rd32(rom + o + 0x80) == fourcc("sony") && rd32(rom + o + 0x84) == fourcc("bone");
    return false;
}

// Run-environment check only: it does not validate the image as a whole.
bool Vz90HasMarker(const uint8_t* pv, size_t size) {
    if (!pv || size < TOKEN_AREA_END || rd32(pv + 8) != SIG_CARD || rd32(pv + 0x84) != fourcc("PvPv")) return false;
    if (rd32(pv + 0x64) != PV_BASE + TOKEN_AREA) return false;
    // walk the token area with both entry alignments in use (2 bytes as built, 4 bytes as the DAL walks): the
    // marker is placed so that both agree, and only an exact match in the card header's token area counts
    for (uint32_t align : {2u, 4u}) {
        size_t p = TOKEN_AREA;
        while (p + 8 <= TOKEN_AREA_END) {
            uint32_t n = rd32(pv + p), len = rd32(pv + p + 4);
            if (n == 0xffffffff || len > TOKEN_AREA_END - p - 8) break;
            if (n == fourcc("vz9p") && len == 4 && memcmp(pv + p + 8, "VZ90", 4) == 0) return true;
            p += 8 + ((len + align - 1) & ~(align - 1));
        }
    }
    return false;
}

Vz90Status Vz90CheckBundle(const Vz90DalBundle& dal, string& buildId, string& message) {
    if (!dal.data) {
        message = "DAL bundle: no data";
        return Vz90Status::errDalMismatch;
    }
    Bundle b;
    Vz90Status s = checkBundle(dal, b, message);
    if (s == Vz90Status::ok) buildId = b.buildId;
    return s;
}

Vz90Status Vz90DescribeBundle(const Vz90DalBundle& dal, string& buildId, string& sha256Hex, string& message) {
    Vz90Status s = Vz90CheckBundle(dal, buildId, message);
    if (s != Vz90Status::ok) return s;
    uint8_t h[32];
    sha256(dal.data, dal.size, h);
    static constexpr char hex[] = "0123456789abcdef";
    sha256Hex.clear();
    for (uint8_t b : h) {
        sha256Hex += hex[b >> 4];
        sha256Hex += hex[b & 15];
    }
    return s;
}

int Vz90ImageHasDal(const uint8_t* pv, size_t size, const Vz90DalBundle& dal) {
    if (!Vz90HasMarker(pv, size) || !dal.data) return -1;
    Bundle b;
    string message;
    if (checkBundle(dal, b, message) != Vz90Status::ok) return -1;
    // boot at its link address; amdd and amdi where the card header points (see Vz90BuildPvImage)
    const size_t at[3] = {DAL_LINK - PV_BASE, rd32(pv + 0x78), rd32(pv + 0xac)};
    for (int i = 0; i < 3; i++)
        if (at[i] > size || b.len[i] > size - at[i] || memcmp(pv + at[i], b.res[i], b.len[i]) != 0) return 0;
    return 1;
}

Vz90Status Vz90BuildPvImage(const uint8_t* rom, size_t romSize, const Vz90DalBundle& dal,
                            const Vz90Options& options, Vz90Image& out, string& message) {
    out.data.reset();
    out.size = 0;

    // 1. candidate and supported variant
    if (!rom || !Vz90IsCandidate(rom, romSize)) {
        message = "not an original VZ90 ROM image (sony/bone)";
        return Vz90Status::errBadRom;
    }
    string hash = md5(const_cast<uint8_t*>(rom), romSize);
    bool supported = false;
    for (const char* h : SUPPORTED) supported |= hash == h;
#ifdef VZ90_TEST_ANY_VARIANT  // test builds only: lets modified images reach the structure checks
    supported = true;
#endif
    if (!supported) {
        message = "MD5 " + hash + " is not on the list of supported images";
        return Vz90Status::errUnsupportedVariant;
    }

    // 2. structure
    Original r;
    if (!r.read(rom, romSize)) {
        message = "VZ90 ROM structure: " + r.error;
        return Vz90Status::errBadRom;
    }
    Db* dalDb = nullptr;
    for (auto& db : r.dbs)
        if (isResource(db, "rsrc") && rd32(db.header + 0x40) == fourcc("pdal")) dalDb = &db;
    if (!dalDb) {
        message = "VZ90 ROM structure: no DAL database";
        return Vz90Status::errBadRom;
    }
    for (uint16_t id : {10001, 10002, 10003})
        if (!hasResource(r, "boot", id)) {
            message = "VZ90 ROM structure: Boot " + to_string(id) + " missing";
            return Vz90Status::errBadRom;
        }

    // 3. DAL bundle
    Bundle bundle;
    Vz90Status s = checkBundle(dal, bundle, message);
    if (s != Vz90Status::ok) return s;
    // the DAL database holds exactly boot, amdd and amdi, each once, all with ID 19000
    Item* dalItem[3] = {nullptr, nullptr, nullptr};
    if (dalDb->items.size() != 3) {
        message = "VZ90 ROM structure: DAL database does not hold exactly 3 resources";
        return Vz90Status::errBadRom;
    }
    for (auto& it : dalDb->items) {
        uint32_t type = rd32(it.meta);
        int k = type == fourcc("boot") ? 0 : type == fourcc("amdd") ? 1 : type == fourcc("amdi") ? 2 : -1;
        if (k < 0 || dalItem[k] || rd16(it.meta + 4) != 0x4a38) {
            message = "VZ90 ROM structure: unexpected, repeated or misnumbered DAL resource";
            return Vz90Status::errBadRom;
        }
        dalItem[k] = &it;
        it.data = bundle.res[k];
        it.length = bundle.len[k];
    }
    Item* bootItem = dalItem[0];

    // ROM tokens: snum (original value unless overridden), the VZ90 marker right after it, knvp last if requested.
    // Every entry before the marker has a length that is a multiple of 4, so both entry alignments read the list alike.
    vector<uint8_t> tokens;
    vector<uint8_t> snum;
    bool haveSnum = options.overrideSnum ? (snum = options.snum, true) : r.token(fourcc("snum"), snum);
    if (haveSnum) {
        if (snum.size() % 4) {
            message = "snum token length must be a multiple of 4";
            return Vz90Status::errBadRom;
        }
        auto t = token("snum", snum);
        tokens.insert(tokens.end(), t.begin(), t.end());
    }
    {
        auto t = token("vz9p", {'V', 'Z', '9', '0'});
        tokens.insert(tokens.end(), t.begin(), t.end());
    }
    if (!options.knvp.empty()) {
        auto t = token("knvp", options.knvp);
        tokens.insert(tokens.end(), t.begin(), t.end());
    }
    if (tokens.size() + 4 > TOKEN_AREA_END - TOKEN_AREA) {
        message = "ROM tokens do not fit";
        return Vz90Status::errBadRom;
    }

    // 4. rebuild
    unique_ptr<uint8_t[]> img(new (nothrow) uint8_t[PV_ROM_SIZE]);
    if (!img) {
        message = "out of memory";
        return Vz90Status::errNoMemory;
    }
    uint8_t* m = img.get();
    memset(m, 0xff, PV_ROM_SIZE);
    unordered_map<uint32_t, uint32_t> where;  // original address -> new address of the same object
    size_t pos = 0x228;                        // first chunk header right after the heap header
    // place one chunk; 0 when it does not fit (room is always kept for the final free chunk header and the heap end
    // marker). Every caller stops the build at the first 0, so no address of a chunk that was not placed is written.
    constexpr size_t RESERVE = 8 + 8 + 4 + 3;  // this chunk's header, the free chunk's header, end marker, alignment
    auto put = [&](const uint8_t* data, size_t len, uint32_t owner) -> uint32_t {
        if (pos > PV_ROM_SIZE - RESERVE || len > PV_ROM_SIZE - RESERVE - pos) return 0;
        size_t total = (8 + len + 3) & ~size_t(3);
        size_t adj = total - 8 - len;
        wr32(m + pos, (uint32_t(adj) << 24) | uint32_t(total));
        wr32(m + pos + 4, owner);
        if (len) memcpy(m + pos + 8, data, len);
        memset(m + pos + 8 + len, 0, adj);
        uint32_t addr = PV_BASE + pos + 8;
        pos += total;
        return addr;
    };

    // the DAL's boot code is the first chunk: the reset branch and its link address point there
    auto full = [&]() {
        message = "rebuilt image does not fit the PV ROM size";
        return Vz90Status::errBadRom;
    };
    bootItem->placed = put(bootItem->data, bootItem->length, OWNER_DATA);
    if (!bootItem->placed) return full();
    if (bootItem->placed != DAL_LINK) {
        message = "internal: boot not at its link address";
        return Vz90Status::errBadRom;
    }
    // every other data chunk, database by database
    for (auto& db : r.dbs) {
        for (auto& it : db.items) {
            if (!it.placed && !(it.placed = put(it.data, it.length, OWNER_DATA))) return full();
            where[it.src] = it.placed;
        }
        for (int f = 0; f < 2; f++)
            if (db.info[f]) {
                uint32_t a = put(db.infoData[f], db.infoLength[f], OWNER_DATA);
                if (!a) return full();
                where[db.info[f]] = a;
            }
    }
    // database headers, PV form (no 2-byte filler after the entry list)
    for (auto& db : r.dbs) {
        vector<uint8_t> h(db.header, db.header + 0x50);
        size_t nul = 31;
        for (size_t i = 0; i < 32; i++)
            if (h[i] == 0) {
                nul = i;
                break;
            }
        memset(h.data() + nul, 0, 32 - nul);
        if (&db == dalDb) {  // this DAL: attributes 1, version 1, dates as on PV
            wr16(&h[0x20], 1);
            wr16(&h[0x22], 1);
            wr32(&h[0x24], 0xf0000000);
            wr32(&h[0x28], 0xf0000000);
            wr32(&h[0x2c], 0xf0000000);
        }
        for (int f = 0; f < 2; f++) {
            uint32_t v = rd32(&h[0x34 + 4 * f]);
            if (!v) continue;
            auto i = where.find(v);
            if (i == where.end()) {
                message = "VZ90 ROM structure: appInfo/sortInfo not found";
                return Vz90Status::errBadRom;
            }
            wr32(&h[0x34 + 4 * f], i->second);
        }
        wr32(&h[0x48], 0);
        wr16(&h[0x4c], db.items.size());
        for (auto& it : db.items) {
            uint8_t e[12];
            if (db.resource) {
                memcpy(e, it.meta, 8);
                wr32(e + 8, it.placed);
                h.insert(h.end(), e, e + 12);
            } else {
                wr32(e, it.placed);
                memcpy(e + 4, it.meta, 4);
                h.insert(h.end(), e, e + 8);
            }
        }
        db.placed = put(h.data(), h.size(), OWNER_HEADER);
        if (!db.placed) return full();
        where[db.addr] = db.placed;
    }
    // directory
    vector<uint8_t> d(8);
    wr32(d.data(), 0);
    wr16(d.data() + 4, r.dbs.size());
    memcpy(d.data() + 6, r.dirTail, 2);
    for (auto& db : r.dbs) {
        uint8_t a[4];
        wr32(a, db.placed);
        d.insert(d.end(), a, a + 4);
    }
    uint32_t dirNew = put(d.data(), d.size(), OWNER_HEADER);
    if (!dirNew) return full();
    // the rest of the heap: one free chunk, then the 4-byte zero that ends a heap (as in the original image)
    uint32_t rest = PV_ROM_SIZE - 4 - pos;
    wr32(m + pos, (0x80u << 24) | rest);
    wr32(m + pos + 4, 0);
    memset(m + pos + 8, 0, rest - 8 + 4);
    // heap list and heap header
    wr16(m + 0x208, 1);
    wr16(m + 0x20a, 0);
    wr32(m + 0x20c, PV_BASE + 0x214);
    memset(m + 0x210, 0, 4);
    memcpy(m + 0x214, rom + r.store + 0x114, 0x14);
    wr32(m + 0x218, PV_ROM_SIZE - 0x214);
    wr32(m + 0x21c, 0);
    // storage header: copy, then re-target every field that pointed to a known object of the original image
    uint8_t* st = m + 0x100;
    memcpy(st, rom + r.store, 0x100);
    for (size_t f = 0; f < 0x100; f += 4) {
        uint32_t v = rd32(st + f);
        if (f == 0x30)
            wr32(st + f, PV_BASE + 0x208);
        else if (f == 0x3c)
            wr32(st + f, dirNew);
        else if (auto i = where.find(v); i != where.end())
            wr32(st + f, i->second);
    }
    memcpy(m + 0x200, rom + r.store + 0x100, 8);
    // card header: original card 0 header with the PV identity and addresses
    uint8_t* ch = m;
    memcpy(ch, rom + r.card0, 0x100);
    wr32(ch, 0xea000000 | ((0x230 - 8) / 4));  // reset: branch to the DAL boot code
    wr32(ch + 0x58, PV_BASE + 0x200);
    wr32(ch + 0x64, PV_BASE + TOKEN_AREA);
    wr32(ch + 0x68, PV_BASE);
    wr32(ch + 0x6c, PV_ROM_SIZE);
    wr32(ch + 0x78, dalItem[1]->placed - PV_BASE);  // amdd and amdi were placed (checked above)
    wr32(ch + 0x7c, 0);
    wr32(ch + 0xac, dalItem[2]->placed - PV_BASE);
    wr32(ch + 0x84, fourcc("PvPv"));
    memset(ch + TOKEN_AREA, 0xff, TOKEN_AREA_END - TOKEN_AREA);
    memcpy(ch + TOKEN_AREA, tokens.data(), tokens.size());

    out.data = std::move(img);
    out.size = PV_ROM_SIZE;
    message = "VZ90 image rebuilt with DAL " + bundle.buildId;
    return Vz90Status::ok;
}
