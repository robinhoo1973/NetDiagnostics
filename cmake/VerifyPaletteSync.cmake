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
# Triggered at cmake configure time via include() in CMakeLists.txt.
# =============================================================================

set(_APP_COLORS   "${CMAKE_SOURCE_DIR}/src/Common/Utils/AppColors.h")
set(_PALETTE_JS   "${CMAKE_SOURCE_DIR}/src/Common/View/theme/Palette.js")

# ── Helper: extract the hex value from an AppColors.h #define ──────────
# 5WHY: The original regex required at least 2 spaces between the macro
# name and value (the "  *" token).  Using " +" (one or more spaces)
# tolerates single-space formatting without breaking existing aligned
# definitions that use many spaces for column alignment.
function(_palette_check pair_name macro js_key)
    # Extract C++ value: #define MACRO "value"
    file(STRINGS "${_APP_COLORS}" _cpp_line REGEX "^#define ${macro} +\"#")
    if(NOT _cpp_line MATCHES "${macro} +\"(#[0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f])\"")
        message(FATAL_ERROR "Palette sync FAIL: macro ${macro} not matched in AppColors.h")
    endif()
    set(_cpp_val "${CMAKE_MATCH_1}")

    # ── Extract JS value from the correct theme block ──────────────────
    # 5WHY: The original code searched the ENTIRE Palette.js file for
    # matching key names.  Since both Dark and Light object blocks use
    # the same key names (e.g. "surface" exists in both Dark and Light),
    # a simple full-file grep could produce a FALSE POSITIVE: if a
    # developer accidentally swapped a dark/light value pair, the check
    # would still find the hex value in the wrong theme's block and
    # report OK.
    #
    # Fix: read all lines sequentially and track which block we are in
    # (Dark or Light).  Only match keys inside the correct block.

    # Determine which JS object block to search
    if(pair_name MATCHES "dark")
        set(_block_pattern "^var Dark = \\{")
    else()
        set(_block_pattern "^var Light = \\{")
    endif()

    file(STRINGS "${_PALETTE_JS}" _all_lines)
    set(_in_block FALSE)
    set(_js_ok FALSE)
    set(_js_val_found "")
    foreach(_l ${_all_lines})
        # Entering the correct block?
        if(_l MATCHES "${_block_pattern}")
            set(_in_block TRUE)
        # Leaving any block?  Both Dark and Light blocks end with "};"
        elseif(_in_block AND _l MATCHES "^\\};")
            set(_in_block FALSE)
        # Inside the correct block — does this line define our key?
        elseif(_in_block AND _l MATCHES "${js_key}: *\"(#[0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f])\"")
            set(_js_val_found "${CMAKE_MATCH_1}")
            string(TOLOWER "${CMAKE_MATCH_1}" _js_try)
            string(TOLOWER "${_cpp_val}" _cpp_lower)
            if(_js_try STREQUAL _cpp_lower)
                set(_js_ok TRUE)
                break()
            endif()
        endif()
    endforeach()
    if(NOT _js_ok)
        if(_js_val_found)
            message(FATAL_ERROR
                "Palette sync FAIL: ${pair_name}\n"
                "  AppColors.h  ${macro}: ${_cpp_val}\n"
                "  Palette.js   ${js_key}: ${_js_val_found} (value mismatch)\n"
                "  → Update BOTH files to keep them in sync.")
        else()
            message(FATAL_ERROR
                "Palette sync FAIL: ${pair_name}\n"
                "  AppColors.h  ${macro}: ${_cpp_val}\n"
                "  Palette.js   ${js_key}: not found in correct theme block\n"
                "  → Update BOTH files to keep them in sync.")
        endif()
    endif()
    message(STATUS "  [OK] ${pair_name} → ${_cpp_val}")
endfunction()

message(STATUS "Verifying AppColors.h ↔ Palette.js sync...")

# Core theme surfaces (dark)
_palette_check("surface dark"       APPC_SURFACE_DARK          "surface")
_palette_check("card dark"          APPC_CARD_DARK             "card")
_palette_check("input dark"         APPC_INPUT_DARK            "input")

# Core theme surfaces (light)
_palette_check("surface light"      APPC_SURFACE_LIGHT         "surface")
_palette_check("card light"         APPC_CARD_LIGHT            "card")
_palette_check("input light"        APPC_INPUT_LIGHT           "input")

# Brand colors
_palette_check("primary dark"       APPC_PRIMARY_DARK          "primary")
_palette_check("primary light"      APPC_PRIMARY_LIGHT         "primary")
_palette_check("secondary dark"     APPC_SECONDARY_DARK        "secondary")
_palette_check("secondary light"    APPC_SECONDARY_LIGHT       "secondary")

# Text colors
_palette_check("textPrimary dark"   APPC_TEXT_PRIMARY_DARK     "textPrimary")
_palette_check("textPrimary light"  APPC_TEXT_PRIMARY_LIGHT    "textPrimary")
_palette_check("textSecondary dark" APPC_TEXT_SECONDARY_DARK   "textSecondary")
_palette_check("textSecondary light" APPC_TEXT_SECONDARY_LIGHT "textSecondary")

# Status colors (most critical — WCAG contrast specs)
_palette_check("passGreen dark"     APPC_PASS_GREEN_DARK       "passGreen")
_palette_check("passGreen light"    APPC_PASS_GREEN_LIGHT      "passGreen")
_palette_check("warnYellow dark"    APPC_WARN_YELLOW_DARK      "warnYellow")
_palette_check("warnYellow light"   APPC_WARN_YELLOW_LIGHT     "warnYellow")
_palette_check("failRed dark"       APPC_FAIL_RED_DARK         "failRed")
_palette_check("failRed light"      APPC_FAIL_RED_LIGHT        "failRed")
_palette_check("infoBlue dark"      APPC_INFO_BLUE_DARK        "infoBlue")
_palette_check("infoBlue light"     APPC_INFO_BLUE_LIGHT       "infoBlue")

message(STATUS "Palette sync: all checks passed.")
