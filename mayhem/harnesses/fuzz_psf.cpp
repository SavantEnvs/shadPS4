// libFuzzer harness: param.sfo (PSF) parser — PSF::Open(const std::vector<u8>&), then every
// typed getter and an Encode round-trip.
#include <cstdint>
#include <vector>
#include "fuzz_env.h"
#include "core/file_format/psf.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // PSF::Open(vector) memcpy's the 0x14-byte header without a length check (a real upstream
    // overread: PSF::Open(path) passes any non-empty file, so a 5-byte param.sfo hits
    // psf.cpp:71; fuzz_psf-standalone reproduces it). It is not a library precondition, but
    // without this guard the target cannot fuzz: libFuzzer's first input on an empty corpus is
    // the 1-byte "\n", which overreads and aborts the run before it iterates (the gate's
    // fuzz-smoke fails, and the unguarded rif target got 0 edges in Mayhem the same way).
    if (size < sizeof(PSFHeader)) {
        return -1;
    }
    std::vector<u8> buf(data, data + size); // exact-size heap copy so ASan sees overreads
    PSF psf;
    if (!psf.Open(buf)) {
        return 0;
    }
    for (const auto& e : psf.GetEntries()) {
        // Each getter ASSERTs the entry's format, so call only the matching one (as callers do).
        switch (e.param_fmt) {
        case PSFEntryFmt::Binary: (void)psf.GetBinary(e.key); break;
        case PSFEntryFmt::Text: (void)psf.GetString(e.key); break;
        case PSFEntryFmt::Integer: (void)psf.GetInteger(e.key); break;
        }
    }
    std::vector<u8> out;
    psf.Encode(out);
    return 0;
}
