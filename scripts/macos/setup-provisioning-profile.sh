#!/bin/bash
# ── macOS App Store Provisioning Profile Setup ───────────────────────
# Generates a base64-encoded provisioning profile and stores it as a
# GitHub secret for the apple.yml TestFlight workflow.
#
# Prerequisites:
#   1. Apple Developer account with App Store Connect (Mac) cert
#   2. App ID registered with Mac capabilities enabled
#   3. gh CLI authenticated (gh auth login)
#
# Usage:
#   ./scripts/macos/setup-provisioning-profile.sh /path/to/profile.provisionprofile
#
# To manually create the profile:
#   1. Go to https://developer.apple.com/account/resources/profiles/list
#   2. Click "+" → "App Store Connect (Mac)"
#   3. Select your App ID (com.netdiagnostic.app)
#   4. Select your "3rd Party Mac Developer Installer" certificate
#   5. Download the .provisionprofile file
#   6. Run this script with the downloaded file
#   7. Or manually: base64 -w0 profile.provisionprofile | gh secret set MACOS_PROVISIONING_PROFILE_BASE64
# ============================================================================

set -euo pipefail

PROFILE="${1:-}"
if [ -z "$PROFILE" ] || [ ! -f "$PROFILE" ]; then
    echo "Usage: $0 /path/to/profile.provisionprofile"
    echo ""
    echo "First, create the profile at:"
    echo "  https://developer.apple.com/account/resources/profiles/list"
    echo ""
    echo "Select: App Store Connect (Mac)"
    echo "App ID: com.netdiagnostic.app"
    echo "Certificate: 3rd Party Mac Developer Installer"
    exit 1
fi

echo "=== Provisioning Profile Info ==="
security cms -D -i "$PROFILE" 2>/dev/null | /usr/libexec/PlistBuddy -c "Print" /dev/stdin | grep -E "Name|UUID|AppIDName|TeamName" || true
echo ""

echo "=== Setting GitHub secret ==="
base64 -w0 "$PROFILE" | gh secret set MACOS_PROVISIONING_PROFILE_BASE64
echo "[OK] MACOS_PROVISIONING_PROFILE_BASE64 set"
echo ""
echo "Also ensure these macOS secrets are set:"
echo "  MACOS_INSTALLER_CERT_BASE64    (already configured)"
echo "  MACOS_INSTALLER_CERT_PASSWORD  (already configured)"
