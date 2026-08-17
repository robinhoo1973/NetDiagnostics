# =============================================================================
# VerifyPaletteSync.cmake — Build-time check that AppColors.h and Palette.js
# define consistent color values.
#
# 5WHY: AppColors.h and Palette.js are MIRROR FILES — both contain ~40
# identical hex color values but live in different toolchains (C++ vs QML).
# Without a build-time check, a developer changing one file but not the
# other produces a silent visual inconsistency: C++ reports render one
# color, QML UI renders another.  No compiler/linter catches this.
#
# 5WHY (2026-08-17 review): the pair table below was a hand-maintained
# third copy of the role list (after Palette.js itself and ROLES in
# scripts/generate-appcolors.py) — 78 _palette_check calls that each
# re-scanned both files (~35k regex attempts per configure).  The table
# could also silently miss a newly added role.  The check now DERIVES the
# pairs from Palette.js keys in a single pass (O(n)), so a new role is
# verified automatically and the pair list exists in exactly one place.
#
# Deliberately excluded tokens:
#   · textMuted — known pre-existing deviation (report-local #64748B vs
#     palette #8494A8; see AppColors.h header KNOWN DEVIATION note)
#   · scrim / non-color tokens — 8-digit overlay / no C++ macro counterpart
#
# Triggered at cmake configure time via include() in CMakeLists.txt.
# =============================================================================

set(_APP_COLORS   "${CMAKE_SOURCE_DIR}/src/Common/Utils/AppColors.h")
set(_PALETTE_JS   "${CMAKE_SOURCE_DIR}/src/Common/View/theme/Palette.js")

# ── Pre-load both files once (avoid re-reads inside the loop) ──
file(STRINGS "${_APP_COLORS}" _PALETTE_CPP_LINES)
file(STRINGS "${_PALETTE_JS}" _PALETTE_JS_LINES)

message(STATUS "Verifying AppColors.h ↔ Palette.js sync...")

# ── Build macro→value map from AppColors.h in one pass ──
# NOTE: CMake's regex engine does not support {n} quantifiers — the hex
# class is spelled out 6 times (same convention as the previous check code).
set(_CPP_MAP "")
foreach(_l ${_PALETTE_CPP_LINES})
    if(_l MATCHES "^#define (APPC_[A-Z0-9_]+) +\"(#[0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f])\"")
        list(APPEND _CPP_MAP "${CMAKE_MATCH_1}=${CMAKE_MATCH_2}")
    endif()
endforeach()

# ── Single pass over Palette.js blocks: every color token must exist in ──
# ── AppColors.h under the same macro stem with the same value.          ──
set(_in_block FALSE)
set(_cur_theme "")
set(_checked_dark 0)
set(_checked_light 0)
foreach(_l ${_PALETTE_JS_LINES})
    # Entering a theme block?
    if(_l MATCHES "^var Dark = \\{")
        set(_in_block TRUE)
        set(_cur_theme "DARK")
    elseif(_l MATCHES "^var Light = \\{")
        set(_in_block TRUE)
        set(_cur_theme "LIGHT")
    # Leaving any block?  Both Dark and Light blocks end with "};"
    elseif(_in_block AND _l MATCHES "^\\};")
        set(_in_block FALSE)
        set(_cur_theme "")
    # Inside a block — does this line define a color token?
    # (Palette.js lines are indented, so allow leading whitespace;
    #  no {n} quantifiers — CMake regex limitation, spelled out class)
    elseif(_in_block AND _l MATCHES "^ *([A-Za-z0-9_]+): *\"(#[0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f])\"")
        set(_key "${CMAKE_MATCH_1}")
        set(_val "${CMAKE_MATCH_2}")
        # Known exclusions (see header comment)
        if(_key STREQUAL "textMuted" OR _key STREQUAL "scrim")
            continue()
        endif()
        # camelCase → UPPER_SNAKE macro stem (matches ROLES stems in the generator)
        string(REGEX REPLACE "([a-z0-9])([A-Z])" "\\1_\\2" _stem "${_key}")
        string(TOUPPER "${_stem}" _stem)
        # Look the pair up in the AppColors.h map
        set(_found FALSE)
        foreach(_entry ${_CPP_MAP})
            if(_entry MATCHES "^APPC_${_stem}_${_cur_theme}=(.+)$")
                string(TOLOWER "${CMAKE_MATCH_1}" _cpp_lower)
                string(TOLOWER "${_val}" _val_lower)
                if(_cpp_lower STREQUAL _val_lower)
                    set(_found TRUE)
                endif()
                break()
            endif()
        endforeach()
        if(NOT _found)
            message(FATAL_ERROR
                "Palette sync FAIL: ${_key} (${_cur_theme}) ${_val}\n"
                "  AppColors.h: no matching APPC_${_stem}_${_cur_theme} macro with this value\n"
                "  → Regenerate: python scripts/generate-appcolors.py\n"
                "  → If this is a NEW role, it must also be added to ROLES in scripts/generate-appcolors.py.")
        endif()
        if(_cur_theme STREQUAL "DARK")
            math(EXPR _checked_dark "${_checked_dark} + 1")
        else()
            math(EXPR _checked_light "${_checked_light} + 1")
        endif()
    endif()
endforeach()

# ── Completeness assertion ────────────────────────────────────────────────
# 5WHY (review 2026-08-17): 旧 78 对手写表天然 fail-closed；单遍派生若块
# 检测锚点失效（如 Palette.js 被重排版为 "var Dark = ("）会静默漏检全部
# 配对并打印 "0 verified"。用 AppColors.h 自身的宏数作期望值（每主题减
# 1 = textMuted，文档化偏差），验证数不符即 FATAL。
set(_expected_dark 0)
set(_expected_light 0)
foreach(_entry ${_CPP_MAP})
    if(_entry MATCHES "^APPC_[A-Z0-9_]+_DARK=")
        math(EXPR _expected_dark "${_expected_dark} + 1")
    elseif(_entry MATCHES "^APPC_[A-Z0-9_]+_LIGHT=")
        math(EXPR _expected_light "${_expected_light} + 1")
    endif()
endforeach()
math(EXPR _expected_dark "${_expected_dark} - 1")   # textMuted exclusion
math(EXPR _expected_light "${_expected_light} - 1")
if(NOT _checked_dark EQUAL _expected_dark OR NOT _checked_light EQUAL _expected_light)
    message(FATAL_ERROR
        "Palette sync FAIL: verified ${_checked_dark}/${_checked_light} (dark/light) "
        "tokens but AppColors.h defines ${_expected_dark}/${_expected_light} macros — "
        "Palette.js block parsing is incomplete or a role changed format "
        "(e.g. 'var Dark = {' spacing).")
endif()

message(STATUS "Palette sync: ${_checked_dark} dark + ${_checked_light} light color tokens verified.")
