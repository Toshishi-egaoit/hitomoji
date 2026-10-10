#pragma once

#include <windows.h>
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

constexpr size_t CHM_CANDIDATE_MAX = 40;

// Zero denotes unavailable JIS information. JIS X 0208 uses plane 1.
struct ChmJisInfo {
    uint32_t plane = 0;
    uint32_t ku = 0;
    uint32_t ten = 0;
    uint32_t level = 0; // 1 through 4; 0 denotes unavailable.
};

enum class ChmKanjiCategory {
    Unknown = -1,
    Education1,
    Education2,
    Education3,
    Education4,
    Education5,
    Education6,
    Joyo,
    Jinmeiyo,
    Other,
};

struct ChmRadicalInfo {
    uint32_t number = 0; // Dictionary radical number; 0 denotes unavailable.
    uint32_t codePoint = 0; // 0 denotes unavailable.
    std::wstring name;
};

// Display-only information; variant selection numbers belong to the engine.
struct ChmVariantInfo {
    uint32_t codePoint = 0;
};

struct ChmKanjiInfo {
    size_t cellIndex = 0; // Corresponding cell in ChmCandidatePage::candidates.
    uint32_t codePoint = 0;
    ChmJisInfo jis;
    ChmKanjiCategory category = ChmKanjiCategory::Unknown;
    ChmRadicalInfo radical;
    uint32_t strokeCount = 0; // 0 denotes unavailable.
    std::vector<ChmVariantInfo> variants;
};

// Owns all display data so a copied page can be passed to the window thread.
// The window formats code points as U+xxxx and renders variant glyphs.
struct ChmCandidatePage {
    UINT page = 0;
    UINT totalPages = 0;
    UINT totalCount = 0;
    UINT candidateCount = 0;
    uint32_t candidates[CHM_CANDIDATE_MAX]{};
    // Entries for occupied cells, associated by cellIndex rather than vector order.
    std::vector<ChmKanjiInfo> kanjiInfo;
};
