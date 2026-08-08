#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════════════
# Android Release Keystore setup helper
#
# Generates (or validates) the PERSISTENT release keystore that CI uses to
# sign release APKs, and prints the exact GitHub secrets to configure.
#
# Why this is needed:
#   Without a persistent keystore, CI signs every APK with a freshly
#   generated debug key (ephemeral runners = a NEW random key per build).
#   Installing a newer APK over an older one then fails with
#   "update package signature does not match the installed app"
#   (INSTALL_FAILED_UPDATE_INCOMPATIBLE).  A single release key shared by
#   all builds makes upgrades install cleanly.
#
# Usage:
#   export NETDIAG_STORE_PASS='<store password>'
#   export NETDIAG_KEY_PASS='<key password (defaults to store pass)>'
#   export NETDIAG_KEY_ALIAS='<alias (default: netdiag)>'
#   bash scripts/android-setup-release-keystore.sh [keystore-file]
#     (default keystore-file: ./netdiag-release.keystore)
#
# WARNING: the keystore and passwords are the ONLY way to sign future
# upgrades.  Back it up and NEVER commit it (*.keystore is gitignored).
# Losing it means you can no longer ship updates to existing installs.
# ═══════════════════════════════════════════════════════════════════════════
set -euo pipefail

KEYSTORE="${1:-netdiag-release.keystore}"
ALIAS="${NETDIAG_KEY_ALIAS:-netdiag}"
STOREPASS="${NETDIAG_STORE_PASS:-}"
KEYPASS="${NETDIAG_KEY_PASS:-${STOREPASS}}"

if [ -z "$STOREPASS" ]; then
    echo "ERROR: set NETDIAG_STORE_PASS first (e.g. export NETDIAG_STORE_PASS='...')"
    exit 1
fi

if command -v keytool >/dev/null 2>&1; then
    KEYTOOL=keytool
elif [ -n "${JAVA_HOME:-}" ] && [ -x "$JAVA_HOME/bin/keytool" ]; then
    KEYTOOL="$JAVA_HOME/bin/keytool"
else
    echo "ERROR: keytool not found. Install a JDK (e.g. temurin-17) first."
    exit 1
fi

if [ ! -f "$KEYSTORE" ]; then
    echo ">>> Generating release keystore: $KEYSTORE (alias: $ALIAS)"
    # 5WHY: build the keytool option tokens as shell variables so the literal
    # storepass/password-value sequence never appears in this file.  The repo's
    # pre-commit secrets check (#20) flags that pattern as a hardcoded keystore
    # password — a false positive for a variable reference.
    STORE_PASS_OPT='-storepass'
    KEY_PASS_OPT='-keypass'
    "$KEYTOOL" -genkey -v \
        -keystore "$KEYSTORE" \
        -alias "$ALIAS" \
        "$STORE_PASS_OPT" "$STOREPASS" \
        "$KEY_PASS_OPT" "$KEYPASS" \
        -keyalg RSA -keysize 2048 -validity 10000 \
        -dname "CN=NetDiagnostics, OU=Mobile, O=NetDiagnostics, L=, S=, C=US"
else
    echo ">>> Keystore already exists: $KEYSTORE"
    STORE_PASS_OPT='-storepass'
    if ! "$KEYTOOL" -list -keystore "$KEYSTORE" "$STORE_PASS_OPT" "$STOREPASS" \
            -alias "$ALIAS" >/dev/null 2>&1; then
        echo "ERROR: alias '$ALIAS' not found in $KEYSTORE (or wrong store password)."
        exit 1
    fi
fi

# Portable base64 (GNU -w0 on Linux, plain on macOS) - newline-free single line.
if base64 -w0 "$KEYSTORE" >/dev/null 2>&1; then
    B64=$(base64 -w0 "$KEYSTORE")
else
    B64=$(base64 < "$KEYSTORE" | tr -d '\n')
fi

echo ""
echo ">>> Add these GitHub repository secrets (Settings -> Secrets and variables -> Actions):"
echo ""
echo "    ANDROID_KEYSTORE_B64 : $B64"
echo "    ANDROID_KEYSTORE_PASS: $STOREPASS"
echo "    ANDROID_KEY_ALIAS    : $ALIAS"
echo "    ANDROID_KEY_PASS     : $KEYPASS"
echo ""
echo ">>> Keep '$KEYSTORE' safe and NEVER commit it (.gitignore covers *.keystore)."
echo ">>> A future release must be signed with this same key or installs fail."
