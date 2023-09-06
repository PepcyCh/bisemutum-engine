#pragma once

#include "utils.hpp"

PEP_CPREP_NAMESPACE_BEGIN

enum class TokenType {
    eEof,
    eIdentifier,
    eString,
    eChar,
    eNumber,
    eSharp,
    eDoubleSharp,
    eDot,
    eTripleDots,
    eLeftBracketRound,
    eRightBracketRound,
    eLeftBracketSquare,
    eRightBracketSquare,
    eLeftBracketCurly,
    eRightBracketCurly,
    eAdd,
    eSub,
    eMul,
    eDiv,
    eMod,
    eInc,
    eDec,
    eBAnd,
    eBOr,
    eBXor,
    eBNot,
    eBShl,
    eBShr,
    eLAnd,
    eLOr,
    eLNot,
    eAddEq,
    eSubEq,
    eMulEq,
    eDivEq,
    eModEq,
    eBAndEq,
    eBOrEq,
    eBXorEq,
    eBShlEq,
    eBShrEq,
    eLess,
    eLessEq,
    eGreater,
    eGreaterEq,
    eEq,
    eNotEq,
    eSpaceship,
    eAssign,
    eArrow,
    eQuestion,
    eColon,
    eSemicolon,
    eComma,
    eScope,
    eUnknown,
};

struct Token final {
    TokenType type;
    std::string_view value;
};

enum class SpaceKeepType : uint32_t {
    eSpace = 1,
    eNewLine = 2,
    eBackSlash = 4,
    eAll = 7,
    eNothing = 0,
};
inline SpaceKeepType operator&(SpaceKeepType a, SpaceKeepType b) {
    return static_cast<SpaceKeepType>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
inline SpaceKeepType operator|(SpaceKeepType a, SpaceKeepType b) {
    return static_cast<SpaceKeepType>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

Token get_next_token(
    InputState &input, std::string &output, bool space_cross_line = true, SpaceKeepType keep = SpaceKeepType::eAll
);

PEP_CPREP_NAMESPACE_END
