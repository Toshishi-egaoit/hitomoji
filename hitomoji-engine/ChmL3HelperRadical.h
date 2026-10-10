#pragma once
#include <cstdint>
#include <string_view>

// Fixed Kangxi radical catalog. One exact hiragana reading per radical;
// aliases, normalization, and attribute-query parsing are deliberately separate.
class ChmL3HelperRadical {
public:
    struct Entry {
        uint32_t number;
        uint32_t codePoint; // Unified ideograph, not a CJK/Kangxi radical symbol.
        std::wstring_view reading;
    };
    static constexpr uint32_t Count = 214;

    // Returns nullptr for unknown numbers/readings. Returned entries live forever.
    static const Entry* FindByNumber(uint32_t number);
    static const Entry* FindByReading(std::wstring_view reading);
};

