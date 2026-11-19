// libFuzzer harness: RIF licence parser — RIF::Open(ridx_buffer, rifa_buffer) and its lookups.
// Input layout: u32 little-endian length N of the RIDX index, then N bytes of RIDX, then the
// RIFA container (the rest). Inputs shorter than the 4-byte prefix, or whose RIDX/RIFA part
// is shorter than its fixed header, are rejected (see below).
#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>
#include "fuzz_env.h"
#include "core/file_format/rif.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 4) {
        return -1;
    }
    uint32_t n;
    std::memcpy(&n, data, 4);
    data += 4;
    size -= 4;
    if (n > size) {
        n = static_cast<uint32_t>(size);
    }
    std::vector<u8> ridx(data, data + n);
    std::vector<u8> rifa(data + n, data + size);
    // RIF::Open(vector, vector) memcpy's both fixed-size headers without a length check, so
    // every buffer shorter than a header overreads. Mayhem already recorded that defect
    // (defect 3657824, savantenvs/shadps4 rif run 1: the header memcpy at
    // src/core/file_format/rif.cpp:55). Without this guard it fires on nearly every input and
    // the target gets 0 edges, hiding the entry-table and offset handling behind it.
    if (ridx.size() < sizeof(RIDXHeader) || rifa.size() < sizeof(RIFAHeader)) {
        return -1;
    }
    RIF rif;
    if (!rif.Open(ridx, rifa)) {
        return 0;
    }
    (void)rif.GetServiceId();
    (void)rif.GetSkuFlag();
    std::array<u8, 16> key{};
    for (const auto& [label, d] : rif.GetAllEntitlements()) {
        (void)rif.GetEntitlementKey(label, key);
    }
    for (const auto& kv : rif.GetLicenseOnlyEntitlements()) {
        (void)kv;
    }
    return 0;
}
