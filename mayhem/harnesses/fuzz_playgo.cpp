// libFuzzer harness: playgo-chunk.dat parser — PlaygoFile::Open(std::span<const u8>).
#include <cstdint>
#include <span>
#include <vector>
#include "fuzz_env.h"
#include "core/file_format/playgo_chunk.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::vector<u8> buf(data, data + size); // exact-size heap copy so ASan sees overreads
    PlaygoFile f;
    (void)f.Open(std::span<const u8>(buf.data(), buf.size()));
    return 0;
}
