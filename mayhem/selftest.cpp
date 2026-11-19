// Known-answer functional tests for the shadPS4 file-format parsers the fuzzers target.
// Built by mayhem/build.sh with normal flags and run by mayhem/test.sh. Prints one
// "PASS <name>" / "FAIL <name>" line per check.
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <span>
#include <string>
#include <vector>
#include "harnesses/fuzz_env.h"
#include "core/file_format/npbind.h"
#include "core/file_format/playgo_chunk.h"
#include "core/file_format/psf.h"
#include "core/file_format/rif.h"

static int failures = 0;

static void put_be(std::vector<u8>& v, u64 x, int n) {
    for (int i = n - 1; i >= 0; i--) {
        v.push_back(static_cast<u8>(x >> (8 * i)));
    }
}
static void put_le(std::vector<u8>& v, size_t at, u64 x, int n) {
    for (int i = 0; i < n; i++) {
        v[at + i] = static_cast<u8>(x >> (8 * i));
    }
}
static void write_file(const char* path, const std::vector<u8>& d) {
    if (FILE* f = std::fopen(path, "wb")) {
        std::fwrite(d.data(), 1, d.size(), f);
        std::fclose(f);
    }
}

// npbind.dat with two bodies (4 TLV entries + 0x98 padding each) and a trailing digest.
static std::vector<u8> make_npbind() {
    std::vector<u8> v;
    const u64 body_size = 4 * 4 + 12 + 12 + 176 + 16 + 0x98;
    put_be(v, NPBIND_MAGIC, 4);
    put_be(v, 1, 4);                                        // version
    put_be(v, sizeof(NpBindHeader) + 2 * body_size + 20, 8); // file_size
    put_be(v, body_size, 8);                                // entry_size
    put_be(v, 2, 8);                                        // num_entries
    v.resize(sizeof(NpBindHeader), 0);
    for (int b = 0; b < 2; b++) {
        const char* id = b == 0 ? "NPWR00001_00" : "NPWR00002_00";
        put_be(v, 0x10, 2);
        put_be(v, 12, 2);
        v.insert(v.end(), id, id + 12);
        put_be(v, 0x11, 2);
        put_be(v, 12, 2);
        v.insert(v.end(), id, id + 12);
        put_be(v, 0x12, 2);
        put_be(v, 176, 2);
        v.resize(v.size() + 176, static_cast<u8>(0xA0 + b));
        put_be(v, 0x13, 2);
        put_be(v, 16, 2);
        v.resize(v.size() + 16, static_cast<u8>(0xB0 + b));
        v.resize(v.size() + 0x98, 0);
    }
    for (int i = 0; i < 20; i++) {
        v.push_back(static_cast<u8>(0xD0 + i)); // digest
    }
    return v;
}

// playgo-chunk.dat: 2 chunks, 2 mchunks; chunk0 = {mchunk0}, chunk1 = {mchunk0, mchunk1}.
static std::vector<u8> make_playgo() {
    const size_t hdr = sizeof(PlaygoHeader); // 0x100
    const size_t attrs = hdr, attrs_len = 2 * sizeof(playgo_chunk_attr_entry_t);
    const size_t mch = attrs + attrs_len, mch_len = 3 * sizeof(u16);
    const size_t lbl = 0x180, lbl_len = 16;
    const size_t mattr = 0x190, mattr_len = 2 * sizeof(playgo_mchunk_attr_entry_t);
    std::vector<u8> v(mattr + mattr_len, 0);
    put_le(v, 0, PLAYGO_MAGIC, 4);
    put_le(v, 4, 1, 2);    // version_major
    put_le(v, 8, 1, 2);    // image_count
    put_le(v, 10, 2, 2);   // chunk_count
    put_le(v, 12, 2, 2);   // mchunk_count
    put_le(v, 14, 1, 2);   // scenario_count
    put_le(v, 16, v.size(), 4); // file_size
    const char* cid = "UP0000-CUSA00001_00-FUZZFUZZFUZZFUZZ";
    std::memcpy(&v[offsetof(PlaygoHeader, content_id)], cid, std::strlen(cid));
    const auto set_chunk = [&](size_t field, size_t off, size_t len) {
        put_le(v, field, off, 4);
        put_le(v, field + 4, len, 4);
    };
    set_chunk(offsetof(PlaygoHeader, chunk_attrs), attrs, attrs_len);
    set_chunk(offsetof(PlaygoHeader, chunk_mchunks), mch, mch_len);
    set_chunk(offsetof(PlaygoHeader, chunk_labels), lbl, lbl_len);
    set_chunk(offsetof(PlaygoHeader, mchunk_attrs), mattr, mattr_len);
    for (int i = 0; i < 2; i++) {
        const size_t e = attrs + i * sizeof(playgo_chunk_attr_entry_t);
        v[e + offsetof(playgo_chunk_attr_entry_t, req_locus)] = static_cast<u8>(2 + i);
        put_le(v, e + offsetof(playgo_chunk_attr_entry_t, mchunk_count), i + 1, 2);
        put_le(v, e + offsetof(playgo_chunk_attr_entry_t, language_mask), 0x10ull << i, 8);
        put_le(v, e + offsetof(playgo_chunk_attr_entry_t, mchunks_offset), i == 0 ? 0 : 2, 4);
        put_le(v, e + offsetof(playgo_chunk_attr_entry_t, label_offset), i == 0 ? 0 : 7, 4);
    }
    put_le(v, mch + 0, 0, 2);
    put_le(v, mch + 2, 0, 2);
    put_le(v, mch + 4, 1, 2);
    std::memcpy(&v[lbl], "chunk0\0chunk1\0", 14);
    for (int i = 0; i < 2; i++) {
        const size_t e = mattr + i * sizeof(playgo_mchunk_attr_entry_t);
        put_le(v, e, 0x10000 * (i + 1), 6);     // loc.offset
        put_le(v, e + 8, 0x1000 * (i + 1), 6);  // size.size
    }
    return v;
}
static void check(const char* name, bool ok) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) {
        failures++;
    }
}

int main(int argc, char** argv) {
    // PSF: build, encode, re-parse; every value must survive the round trip.
    PSF psf;
    psf.AddString("TITLE", "Fuzz Title");
    psf.AddString("TITLE_ID", "CUSA00001");
    psf.AddInteger("APP_TYPE", 1);
    psf.AddInteger("PARENTAL_LEVEL", -7);
    psf.AddBinary("PUBTOOLINFO", std::vector<u8>{1, 2, 3, 4, 5});
    std::vector<u8> enc;
    psf.Encode(enc);
    const std::vector<u8> npbind_file = make_npbind();
    const std::vector<u8> playgo_file = make_playgo();
    // selftest [<out.sfo> [<out npbind.dat> [<out playgo-chunk.dat>]]]: also write the valid
    // files these checks parse (used to make the per-target seeds under mayhem/<target>/testsuite).
    if (argc > 1) {
        write_file(argv[1], enc);
    }
    if (argc > 2) {
        write_file(argv[2], npbind_file);
    }
    if (argc > 3) {
        write_file(argv[3], playgo_file);
    }
    check("psf_encode_magic", enc.size() > 0x14 && enc[0] == 0 && enc[1] == 'P' && enc[2] == 'S' &&
                                  enc[3] == 'F');
    PSF back;
    check("psf_reopen", back.Open(enc));
    check("psf_entry_count", back.GetEntries().size() == 5);
    auto title = back.GetString("TITLE");
    check("psf_string", title && *title == "Fuzz Title");
    auto tid = back.GetString("TITLE_ID");
    check("psf_string2", tid && *tid == "CUSA00001");
    auto at = back.GetInteger("APP_TYPE");
    check("psf_integer", at && *at == 1);
    auto pl = back.GetInteger("PARENTAL_LEVEL");
    check("psf_integer_negative", pl && *pl == -7);
    auto bin = back.GetBinary("PUBTOOLINFO");
    check("psf_binary", bin && bin->size() == 5 && (*bin)[0] == 1 && (*bin)[4] == 5);
    check("psf_missing_key", !back.GetString("NOPE").has_value() &&
                                 !back.GetInteger("NOPE").has_value());
    std::vector<u8> enc2;
    back.Encode(enc2);
    check("psf_reencode_identical", enc2 == enc);
    std::vector<u8> bad(enc);
    bad[1] = 'X';
    PSF badp;
    check("psf_bad_magic_rejected", !badp.Open(bad));

    // npbind / playgo / RIF: well-sized buffers with a wrong magic are rejected.
    std::vector<u8> zeros(4096, 0);
    NPBindFile npb;
    check("npbind_bad_magic_rejected", !npb.Load(std::span<const u8>(zeros)) && !npb.IsValid());
    PlaygoFile pg;
    check("playgo_bad_magic_rejected", !pg.Open(std::span<const u8>(zeros)));
    check("playgo_too_short_rejected", !pg.Open(std::span<const u8>(zeros.data(), 4)));
    RIF rif;
    check("rif_bad_magic_rejected", !rif.Open(zeros, zeros));

    // Valid npbind.dat: both bodies parsed, ids and digest read back.
    NPBindFile npv;
    check("npbind_valid_load", npv.Load(std::span<const u8>(npbind_file)) && npv.IsValid() &&
                                   npv.BodyCount() == 2);
    auto ids = npv.GetNpCommIds();
    check("npbind_npcommids", ids.size() == 2 && ids[0] == "NPWR00001_00" &&
                                  ids[1] == "NPWR00002_00" && npv.GetBody(1).unk1.data.size() == 176 &&
                                  npv.GetBody(1).unk1.data[0] == 0xA1);
    check("npbind_digest", npv.Digest()[0] == 0xD0 && npv.Digest()[19] == 0xE3);

    // Valid playgo-chunk.dat: labels, locus, language mask and summed mchunk sizes.
    PlaygoFile pgv;
    check("playgo_valid_open", pgv.Open(std::span<const u8>(playgo_file)) && pgv.chunks.size() == 2);
    check("playgo_chunks", pgv.chunks.size() == 2 && pgv.chunks[0].label_name == "chunk0" &&
                               pgv.chunks[1].label_name == "chunk1" &&
                               pgv.chunks[0].total_size == 0x1000 &&
                               pgv.chunks[1].total_size == 0x3000 && pgv.chunks[1].req_locus == 3 &&
                               pgv.chunks[1].language_mask == 0x20);

    std::printf("selftest: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
