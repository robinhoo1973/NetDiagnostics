#!/bin/bash
# ─────────────────────────────────────────────────────────────────────
# asc-app.sh — App Store Connect API helpers
# ─────────────────────────────────────────────────────────────────────
# Usage:
#   source scripts/ios/asc-app.sh
#   asc_jwt       API_KEY_ID ISSUER_ID PRIVATE_KEY_PATH   → prints JWT
#   asc_app_find  JWT BUNDLE_ID                          → prints app ID or ""
#   asc_app_create JWT APP_NAME BUNDLE_ID SKU LOCALE      → prints app ID
#   asc_app_ensure JWT APP_NAME BUNDLE_ID SKU LOCALE      → idempotent ensure
# ─────────────────────────────────────────────────────────────────────

set -euo pipefail

ASC_API="https://api.appstoreconnect.apple.com/v1"

# ─────────────────────────────────────────────────────────────────────
# Generate JWT for App Store Connect API (valid 20 min)
# ─────────────────────────────────────────────────────────────────────
asc_jwt() {
    local key_id="$1"
    local issuer_id="$2"
    local key_path="$3"

    local header
    header=$(printf '{"alg":"ES256","kid":"%s","typ":"JWT"}' "$key_id" | base64 | tr -d '=' | tr '/+' '_-')
    local now
    now=$(date +%s)
    local exp=$((now + 1200))
    local payload
    payload=$(printf '{"iss":"%s","iat":%d,"exp":%d,"aud":"appstoreconnect-v1"}' \
        "$issuer_id" "$now" "$exp" | base64 | tr -d '=' | tr '/+' '_-')
    local signature
    signature=$(printf '%s.%s' "$header" "$payload" | openssl dgst -sha256 -sign "$key_path" | base64 | tr -d '=' | tr '/+' '_-')
    echo "${header}.${payload}.${signature}"
}

# ─────────────────────────────────────────────────────────────────────
# Find an app by bundle ID → prints app id or empty string
# ─────────────────────────────────────────────────────────────────────
asc_app_find() {
    local jwt="$1"
    local bundle_id="$2"

    local resp
    resp=$(curl -s -X GET "${ASC_API}/apps?filter[bundleId]=${bundle_id}" \
        -H "Authorization: Bearer ${jwt}" \
        -H "Accept: application/json")

    # If curl failed entirely
    if [ $? -ne 0 ]; then
        echo "ERROR: curl request failed" >&2
        return 1
    fi

    local app_id
    app_id=$(echo "$resp" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('data',[{}])[0].get('id',''))" 2>/dev/null || echo "")

    echo "$app_id"
}

# ─────────────────────────────────────────────────────────────────────
# Create an app → prints app id
# ─────────────────────────────────────────────────────────────────────
asc_app_create() {
    local jwt="$1"
    local app_name="$2"
    local bundle_id="$3"
    local sku="$4"
    local locale="${5:-en-US}"

    local payload
    payload=$(cat <<ENDJSON
{
  "data": {
    "type": "apps",
    "attributes": {
      "name": "${app_name}",
      "bundleId": "${bundle_id}",
      "primaryLocale": "${locale}",
      "sku": "${sku}"
    }
  }
}
ENDJSON
)

    local resp
    resp=$(curl -s -w "\n%{http_code}" -X POST "${ASC_API}/apps" \
        -H "Authorization: Bearer ${jwt}" \
        -H "Content-Type: application/json" \
        -H "Accept: application/json" \
        -d "$payload")

    local http_code
    http_code=$(echo "$resp" | tail -1)
    local body
    body=$(echo "$resp" | sed '$d')

    if [ "$http_code" != "201" ] && [ "$http_code" != "200" ]; then
        echo "ERROR: failed to create app (HTTP ${http_code}): ${body}" >&2
        return 1
    fi

    local app_id
    app_id=$(echo "$body" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d['data']['id'])")
    echo "$app_id"
}

# ─────────────────────────────────────────────────────────────────────
# Ensure app exists — find or create — prints app id
# ─────────────────────────────────────────────────────────────────────
asc_app_ensure() {
    local jwt="$1"
    local app_name="$2"
    local bundle_id="$3"
    local sku="$4"
    local locale="${5:-en-US}"

    local app_id
    app_id=$(asc_app_find "$jwt" "$bundle_id")

    if [ -n "$app_id" ]; then
        echo "App already exists: id=${app_id}" >&2
        echo "$app_id"
        return 0
    fi

    echo "App not found, creating: name=${app_name} bundle=${bundle_id}..." >&2
    app_id=$(asc_app_create "$jwt" "$app_name" "$bundle_id" "$sku" "$locale")
    echo "Created app: id=${app_id}" >&2
    echo "$app_id"
}

# ─────────────────────────────────────────────────────────────────────
# Bundle ID management
# ─────────────────────────────────────────────────────────────────────
asc_bundle_find() {
    local jwt="$1"
    local identifier="$2"

    local resp
    resp=$(curl -s -X GET "${ASC_API}/bundleIds?filter[identifier]=${identifier}" \
        -H "Authorization: Bearer ${jwt}" \
        -H "Accept: application/json")

    local bid
    bid=$(echo "$resp" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('data',[{}])[0].get('id',''))" 2>/dev/null || echo "")
    echo "$bid"
}

asc_bundle_create() {
    local jwt="$1"
    local name="$2"
    local identifier="$3"
    local platform="${4:-IOS}"

    local payload
    payload=$(cat <<ENDJSON
{
  "data": {
    "type": "bundleIds",
    "attributes": {
      "name": "${name}",
      "identifier": "${identifier}",
      "platform": "${platform}"
    }
  }
}
ENDJSON
)

    local resp
    resp=$(curl -s -w "\n%{http_code}" -X POST "${ASC_API}/bundleIds" \
        -H "Authorization: Bearer ${jwt}" \
        -H "Content-Type: application/json" \
        -H "Accept: application/json" \
        -d "$payload")

    local http_code
    http_code=$(echo "$resp" | tail -1)
    local body
    body=$(echo "$resp" | sed '$d')

    if [ "$http_code" != "201" ] && [ "$http_code" != "200" ]; then
        echo "ERROR: failed to create bundle ID (HTTP ${http_code}): ${body}" >&2
        return 1
    fi

    local bid
    bid=$(echo "$body" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d['data']['id'])")
    echo "$bid"
}

asc_bundle_ensure() {
    local jwt="$1"
    local name="$2"
    local identifier="$3"

    local bid
    bid=$(asc_bundle_find "$jwt" "$identifier")

    if [ -n "$bid" ]; then
        echo "Bundle ID already registered: id=${bid}" >&2
        echo "$bid"
        return 0
    fi

    echo "Bundle ID not found, registering: name=${name} id=${identifier}..." >&2
    bid=$(asc_bundle_create "$jwt" "$name" "$identifier")
    echo "Registered bundle ID: id=${bid}" >&2
    echo "$bid"
}

# ─────────────────────────────────────────────────────────────────────
# Bundle ID capability management
# ─────────────────────────────────────────────────────────────────────
asc_bundle_capability_list() {
    local jwt="$1"
    local bundle_id="$2"

    local resp
    resp=$(curl -s -X GET "${ASC_API}/bundleIds/${bundle_id}/bundleIdCapabilities" \
        -H "Authorization: Bearer ${jwt}" \
        -H "Accept: application/json")

    local caps
    caps=$(echo "$resp" | python3 -c "
import sys, json
data = json.load(sys.stdin).get('data', [])
caps = [c.get('attributes', {}).get('capabilityType', '') for c in data]
print('\n'.join(filter(None, caps)))
" 2>/dev/null || echo "")
    echo "$caps"
}

# Parse Apple ASC API error response and print human-readable message
_asc_parse_error() {
    local body="$1"
    echo "$body" | python3 -c "
import sys, json
try:
    errs = json.load(sys.stdin).get('errors', [])
except:
    print('(unable to parse error response)')
    sys.exit(0)
for e in errs:
    status = e.get('status', '???')
    title  = e.get('title', '')
    detail = e.get('detail', '')
    code   = e.get('code', '')
    print(f'  [ASC] HTTP {status}  code={code}')
    print(f'  [ASC] {title}')
    if detail:
        print(f'  [ASC] {detail}')
" 2>/dev/null
}

asc_bundle_capability_enable() {
    local jwt="$1"
    local bundle_id="$2"
    local capability="${3:-ACCESS_WIFI_INFORMATION}"

    # Check if capability already enabled
    local existing
    existing=$(asc_bundle_capability_list "$jwt" "$bundle_id") || true
    if echo "$existing" | grep -q "$capability"; then
        echo "Capability '${capability}' already enabled on bundle ID ${bundle_id}" >&2
        return 0
    fi

    echo "Enabling capability '${capability}' on bundle ID ${bundle_id}..." >&2

    local payload
    payload=$(cat <<ENDJSON
{
  "data": {
    "type": "bundleIdCapabilities",
    "attributes": {
      "capabilityType": "${capability}"
    },
    "relationships": {
      "bundleId": {
        "data": { "type": "bundleIds", "id": "${bundle_id}" }
      }
    }
  }
}
ENDJSON
)

    local resp
    resp=$(curl -s -w "\n%{http_code}" -X POST "${ASC_API}/bundleIdCapabilities" \
        -H "Authorization: Bearer ${jwt}" \
        -H "Content-Type: application/json" \
        -H "Accept: application/json" \
        -d "$payload")

    local http_code
    http_code=$(echo "$resp" | tail -1)
    local body
    body=$(echo "$resp" | sed '$d')

    if [ "$http_code" != "201" ] && [ "$http_code" != "200" ]; then
        if [ "$http_code" = "409" ]; then
            echo "Capability '${capability}' already exists (HTTP 409)" >&2
            return 0
        fi
        echo "ERROR: failed to enable capability '${capability}' (HTTP ${http_code})" >&2
        _asc_parse_error "$body" >&2

        # 403 / 401 = API key lacks Certificates, Identifiers & Profiles access
        if [ "$http_code" = "403" ] || [ "$http_code" = "401" ]; then
            echo "" >&2
            echo "HOW TO FIX:" >&2
            echo "  Your ASC API Key does not have permission to manage Bundle ID Capabilities." >&2
            echo "  1. Go to https://appstoreconnect.apple.com/access/api" >&2
            echo "  2. Revoke the current key and create a new one" >&2
            echo "  3. Ensure 'Access to Certificates, Identifiers & Profiles' is CHECKED" >&2
            echo "  4. Update APPSTORE_CONNECT_API_KEY, APPSTORE_CONNECT_KEY_ID, APPSTORE_CONNECT_ISSUER_ID secrets" >&2
            echo "" >&2
            echo "  OR — manually enable the capability in Apple Developer Portal:" >&2
            echo "  https://developer.apple.com/account/resources/identifiers/list" >&2
            echo "  Find '${BUNDLE_ID:-com.netdiagnostic.app}' → Capabilities → check 'Access WiFi Information'" >&2
        fi
        return 1
    fi

    echo "Enabled capability '${capability}' on bundle ID ${bundle_id}" >&2
    return 0
}

# ─────────────────────────────────────────────────────────────────────
# Certificate lookup
# ─────────────────────────────────────────────────────────────────────
asc_cert_find_distribution() {
    local jwt="$1"

    # List all certificates (no type filter), pick first distribution-related one
    local resp
    resp=$(curl -s -X GET "${ASC_API}/certificates?sort=-displayName&limit=10" \
        -H "Authorization: Bearer ${jwt}" \
        -H "Accept: application/json")

    local cid
    cid=$(echo "$resp" | python3 -c "
import sys, json
raw = sys.stdin.read()
# Check for API errors first
try:
    resp_data = json.loads(raw)
except:
    print('DEBUG_PARSE_ERROR', file=sys.stderr)
    print('')
    sys.exit(0)
if 'errors' in resp_data:
    for e in resp_data.get('errors', []):
        print(f'DEBUG_API_ERROR: status={e.get(\"status\")} title={e.get(\"title\",\"\")} detail={e.get(\"detail\",\"\")}', file=sys.stderr)
    print('')
    sys.exit(0)
data = resp_data.get('data', [])
print(f'DEBUG_CERT_COUNT: {len(data)}', file=sys.stderr)
for cert in data:
    cert_type = cert.get('attributes', {}).get('certificateType', '???')
    cert_name = cert.get('attributes', {}).get('displayName', '???')
    print(f'DEBUG_CERT: type={cert_type} name={cert_name}', file=sys.stderr)
    if 'DISTRIBUTION' in cert_type.upper() or 'DEVELOPER_ID_APPLICATION' in cert_type.upper():
        print(cert['id'])
        break
else:
    print('')
")
    echo "$cid"
}

# ─────────────────────────────────────────────────────────────────────
# Provisioning profile management
# ─────────────────────────────────────────────────────────────────────
asc_profile_find() {
    local jwt="$1"
    local profile_name="$2"

    local resp
    resp=$(curl -s -X GET "${ASC_API}/profiles?filter[name]=${profile_name}&filter[profileType]=IOS_APP_STORE&limit=1" \
        -H "Authorization: Bearer ${jwt}" \
        -H "Accept: application/json")

    local pid
    pid=$(echo "$resp" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('data',[{}])[0].get('id',''))" 2>/dev/null || echo "")
    echo "$pid"
}

asc_profile_create() {
    local jwt="$1"
    local profile_name="$2"
    local bundle_id="$3"
    local cert_id="$4"

    local payload
    payload=$(cat <<ENDJSON
{
  "data": {
    "type": "profiles",
    "attributes": {
      "name": "${profile_name}",
      "profileType": "IOS_APP_STORE"
    },
    "relationships": {
      "bundleId": {
        "data": { "type": "bundleIds", "id": "${bundle_id}" }
      },
      "certificates": {
        "data": [ { "type": "certificates", "id": "${cert_id}" } ]
      }
    }
  }
}
ENDJSON
)

    local resp
    resp=$(curl -s -w "\n%{http_code}" -X POST "${ASC_API}/profiles" \
        -H "Authorization: Bearer ${jwt}" \
        -H "Content-Type: application/json" \
        -H "Accept: application/json" \
        -d "$payload")

    local http_code
    http_code=$(echo "$resp" | tail -1)
    local body
    body=$(echo "$resp" | sed '$d')

    if [ "$http_code" != "201" ] && [ "$http_code" != "200" ]; then
        echo "ERROR: failed to create provisioning profile (HTTP ${http_code}): ${body}" >&2
        return 1
    fi

    local pid
    pid=$(echo "$body" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d['data']['id'])")
    echo "$pid"
}

asc_profile_ensure() {
    local jwt="$1"
    local profile_name="$2"
    local bundle_id="$3"
    local cert_id="$4"

    local pid
    pid=$(asc_profile_find "$jwt" "$profile_name")

    if [ -n "$pid" ]; then
        echo "Provisioning profile exists: id=${pid}" >&2
        echo "$pid"
        return 0
    fi

    echo "Provisioning profile not found, creating: ${profile_name}..." >&2
    pid=$(asc_profile_create "$jwt" "$profile_name" "$bundle_id" "$cert_id")

    # Provisioning profile creation returns the profile object
    # We need to download the actual .mobileprovision file content
    echo "Created provisioning profile: id=${pid}" >&2
    echo "$pid"
}

# ─────────────────────────────────────────────────────────────────────
# Download provisioning profile data (base64-encoded content)
# ─────────────────────────────────────────────────────────────────────
asc_profile_download() {
    local jwt="$1"
    local profile_id="$2"

    local resp
    resp=$(curl -s -X GET "${ASC_API}/profiles/${profile_id}" \
        -H "Authorization: Bearer ${jwt}" \
        -H "Accept: application/json" \
        -H "fields[profiles]=profileContent")

    local content
    content=$(echo "$resp" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d['data']['attributes'].get('profileContent',''))" 2>/dev/null || echo "")

    if [ -z "$content" ]; then
        echo "ERROR: failed to download provisioning profile content" >&2
        return 1
    fi

    echo "$content"
}

# ─────────────────────────────────────────────────────────────────────
# When executed directly (not sourced), run the requested function
# ─────────────────────────────────────────────────────────────────────
if [ "${BASH_SOURCE[0]}" = "$0" ]; then
    CMD="${1:-}"
    shift 2>/dev/null || true
    case "$CMD" in
        jwt)              asc_jwt "$@" ;;
        app-find)         asc_app_find "$@" ;;
        app-create)       asc_app_create "$@" ;;
        app-ensure)       asc_app_ensure "$@" ;;
        bundle-find)      asc_bundle_find "$@" ;;
        bundle-create)    asc_bundle_create "$@" ;;
        bundle-ensure)    asc_bundle_ensure "$@" ;;
        cap-list)         asc_bundle_capability_list "$@" ;;
        cap-enable)       asc_bundle_capability_enable "$@" ;;
        cert-dist)        asc_cert_find_distribution "$@" ;;
        profile-find)     asc_profile_find "$@" ;;
        profile-create)   asc_profile_create "$@" ;;
        profile-ensure)   asc_profile_ensure "$@" ;;
        profile-download) asc_profile_download "$@" ;;
        *)
            echo "Usage: $0 {jwt|app-find|app-create|app-ensure|bundle-find|bundle-create|bundle-ensure|cap-list|cap-enable|cert-dist|profile-find|profile-create|profile-ensure|profile-download} [args...]" >&2
            exit 1
            ;;
    esac
fi
