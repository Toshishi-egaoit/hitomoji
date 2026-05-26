#pragma once

#include <windows.h>
#include <cstdint>
#include <cstddef>

constexpr size_t CHM_CANDIDATE_MAX = 40;

struct ChmCandidatePage {
    UINT page = 0;
    UINT totalCount = 0;
    UINT candidateCount = 0;
    uint32_t candidates[CHM_CANDIDATE_MAX]{};
};
