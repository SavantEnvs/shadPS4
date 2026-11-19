// libFuzzer harness: npbind.dat parser — NPBindFile::Load(std::span<const u8>).
#include <cstdint>
#include <cstring> // npbind.h uses memset without including it
#include <span>
#include <vector>
#include "fuzz_env.h"
#include "core/file_format/npbind.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::vector<u8> buf(data, data + size); // exact-size heap copy so ASan sees overreads
    NPBindFile f;
    if (!f.Load(std::span<const u8>(buf.data(), buf.size()))) {
        return 0;
    }
    (void)f.GetNpCommIds();
    for (u64 i = 0; i < f.BodyCount(); i++) {
        (void)f.GetBody(i);
    }
    return 0;
}
