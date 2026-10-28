// One file out of one of the game's zips (stored or DEFLATE), for collidablepresets.xml. The inflater
// and the zip walk are the FH2 Mod Platform's (src/strings.cpp there, MIT licence).

// ---- raw DEFLATE (RFC 1951) ---------------------------------------------------------------------------

struct Inflater {
    const unsigned char* in = nullptr;
    std::size_t len = 0, pos = 0;
    uint32_t bitbuf = 0;
    int bitcnt = 0;
    std::string out;
    bool bad = false;

    int bits(int need) {
        uint32_t v = bitbuf;
        while (bitcnt < need) {
            if (pos >= len) {
                bad = true;
                return 0;
            }
            v |= static_cast<uint32_t>(in[pos++]) << bitcnt;
            bitcnt += 8;
        }
        bitbuf = v >> need;
        bitcnt -= need;
        return static_cast<int>(v & ((1u << need) - 1));
    }

    struct Huffman {
        short count[16]{};
        short symbol[288]{};
    };

    static bool build(Huffman& h, const short* lengths, int n) {
        std::memset(h.count, 0, sizeof(h.count));
        for (int s = 0; s < n; ++s) h.count[lengths[s]]++;
        if (h.count[0] == n) return true;
        int left = 1;
        for (int l = 1; l < 16; ++l) {
            left <<= 1;
            left -= h.count[l];
            if (left < 0) return false;
        }
        short offs[16]{};
        for (int l = 1; l < 15; ++l) offs[l + 1] = offs[l] + h.count[l];
        for (int s = 0; s < n; ++s)
            if (lengths[s]) h.symbol[offs[lengths[s]]++] = static_cast<short>(s);
        return true;
    }

    int decode(const Huffman& h) {
        int code = 0, first = 0, index = 0;
        for (int l = 1; l < 16; ++l) {
            code |= bits(1);
            if (bad) return -1;
            const int count = h.count[l];
            if (code - count < first) return h.symbol[index + (code - first)];
            index += count;
            first += count;
            first <<= 1;
            code <<= 1;
        }
        bad = true;
        return -1;
    }

    bool codes(const Huffman& lit, const Huffman& dist) {
        static const short lbase[29] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                        31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
        static const short lext[29]  = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                        2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
        static const short dbase[30] = {1,    2,    3,    4,    5,    7,     9,     13,    17,    25,
                                        33,   49,   65,   97,   129,  193,   257,   385,   513,   769,
                                        1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
        static const short dext[30]  = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
                                        6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
        for (;;) {
            int sym = decode(lit);
            if (sym < 0) return false;
            if (sym < 256) {
                out.push_back(static_cast<char>(sym));
            } else if (sym == 256) {
                return true;
            } else {
                sym -= 257;
                if (sym >= 29) return false;
                const int n  = lbase[sym] + bits(lext[sym]);
                const int ds = decode(dist);
                if (ds < 0 || ds >= 30) return false;
                const std::size_t d = static_cast<std::size_t>(dbase[ds] + bits(dext[ds]));
                if (bad || d > out.size()) return false;
                for (int i = 0; i < n; ++i) out.push_back(out[out.size() - d]);
            }
        }
    }

    bool fixed() {
        static Huffman lit, dist;
        static bool built = false;
        if (!built) {
            short l[288];
            int s = 0;
            for (; s < 144; ++s) l[s] = 8;
            for (; s < 256; ++s) l[s] = 9;
            for (; s < 280; ++s) l[s] = 7;
            for (; s < 288; ++s) l[s] = 8;
            build(lit, l, 288);
            for (s = 0; s < 30; ++s) l[s] = 5;
            build(dist, l, 30);
            built = true;
        }
        return codes(lit, dist);
    }

    bool dynamic() {
        static const short order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
        const int nlen = bits(5) + 257, ndist = bits(5) + 1, ncode = bits(4) + 4;
        if (bad || nlen > 286 || ndist > 30) return false;
        short lengths[320]{};
        for (int i = 0; i < ncode; ++i) lengths[order[i]] = static_cast<short>(bits(3));
        Huffman lencode, lit, dist;
        if (bad || !build(lencode, lengths, 19)) return false;
        int i = 0;
        while (i < nlen + ndist) {
            const int sym = decode(lencode);
            if (sym < 0) return false;
            if (sym < 16) {
                lengths[i++] = static_cast<short>(sym);
            } else {
                short v = 0;
                int rep;
                if (sym == 16) {
                    if (i == 0) return false;
                    v   = lengths[i - 1];
                    rep = 3 + bits(2);
                } else if (sym == 17) {
                    rep = 3 + bits(3);
                } else {
                    rep = 11 + bits(7);
                }
                if (bad || i + rep > nlen + ndist) return false;
                while (rep--) lengths[i++] = v;
            }
        }
        if (lengths[256] == 0) return false;
        if (!build(lit, lengths, nlen) || !build(dist, lengths + nlen, ndist)) return false;
        return codes(lit, dist);
    }

    bool run() {
        for (;;) {
            const int last = bits(1), type = bits(2);
            if (bad) return false;
            bool ok;
            if (type == 0) {
                bitbuf = 0;
                bitcnt = 0;
                if (pos + 4 > len) return false;
                const unsigned n = in[pos] | (in[pos + 1] << 8);
                pos += 4;
                if (pos + n > len) return false;
                out.append(reinterpret_cast<const char*>(in + pos), n);
                pos += n;
                ok = true;
            } else if (type == 1) {
                ok = fixed();
            } else if (type == 2) {
                ok = dynamic();
            } else {
                return false;
            }
            if (!ok) return false;
            if (last) return true;
        }
    }
};

uint16_t r16(const std::string& d, std::size_t o) {
    return static_cast<uint16_t>(static_cast<unsigned char>(d[o]) | (static_cast<unsigned char>(d[o + 1]) << 8));
}
uint32_t r32(const std::string& d, std::size_t o) {
    return static_cast<uint32_t>(r16(d, o)) | (static_cast<uint32_t>(r16(d, o + 2)) << 16);
}

bool ieq(const char* a, std::size_t na, const char* b) {
    if (na != std::strlen(b)) return false;
    for (std::size_t i = 0; i < na; ++i)
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    return true;
}

// `name` from the zip at `zip`, or "" if the zip or the file isn't there or can't be read.
std::string unzip_file(const std::wstring& zip, const char* name) {
    std::string d;
    HANDLE f = CreateFileW(zip.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return {};
    LARGE_INTEGER size{};
    if (GetFileSizeEx(f, &size) && size.QuadPart > 0 && size.QuadPart < (64 << 20)) {
        d.resize(static_cast<std::size_t>(size.QuadPart));
        DWORD got = 0;
        if (!ReadFile(f, d.data(), static_cast<DWORD>(d.size()), &got, nullptr) || got != d.size()) d.clear();
    }
    CloseHandle(f);
    if (d.size() < 22) return {};
    std::size_t eocd = std::string::npos;
    for (std::size_t i = d.size() - 22 + 1; i-- > 0 && d.size() - i < 22 + 65536;)
        if (r32(d, i) == 0x06054b50) {
            eocd = i;
            break;
        }
    if (eocd == std::string::npos) return {};
    const std::size_t entries = r16(d, eocd + 10);
    std::size_t cd            = r32(d, eocd + 16);
    for (std::size_t e = 0; e < entries && cd + 46 <= d.size() && r32(d, cd) == 0x02014b50; ++e) {
        const uint16_t method = r16(d, cd + 10);
        const uint32_t csize = r32(d, cd + 20), usize = r32(d, cd + 24);
        const uint16_t nlen = r16(d, cd + 28), xlen = r16(d, cd + 30), clen = r16(d, cd + 32);
        const uint32_t local = r32(d, cd + 42);
        if (cd + 46 + nlen > d.size()) return {};
        const char* n = d.data() + cd + 46;
        cd += 46 + nlen + xlen + clen;
        if (!ieq(n, nlen, name)) continue;
        if (local + 30 > d.size() || r32(d, local) != 0x04034b50) return {};
        const std::size_t data = local + 30 + r16(d, local + 26) + r16(d, local + 28);
        if (data + csize > d.size()) return {};
        if (method == 0) return d.substr(data, csize);
        if (method != 8) return {};
        Inflater inf;
        inf.in  = reinterpret_cast<const unsigned char*>(d.data() + data);
        inf.len = csize;
        if (!inf.run() || inf.out.size() != usize) return {};
        return inf.out;
    }
    return {};
}
