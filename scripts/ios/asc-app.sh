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
sys.stdout.write(f'DEBUG_RAW_RESP: {raw[:500]}\n')
data = json.loads(raw).get('data', [])
sys.stdout.write(f'DEBUG_CERT_COUNT: {len(data)}\n')
for cert in data:
    cert_type = cert.get('attributes', {}).get('certificateType', '???')
    cert_name = cert.get('attributes', {}).get('displayName', '???')
    sys.stdout.write(f'DEBUG_CERT: type={cert_type} name={cert_name}\n')
    if 'DISTRIBUTION' in cert_type.upper() or 'DEVELOPER_ID_APPLICATION' in cert_type.upper():
        print(cert['id'])
        break
else:
    if len(data) == 0:
        sys.stdout.write('DEBUG: API returned zero certificates\n')
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
        cert-dist)        asc_cert_find_distribution "$@" ;;
        profile-find)     asc_profile_find "$@" ;;
        profile-create)   asc_profile_create "$@" ;;
        profile-ensure)   asc_profile_ensure "$@" ;;
        profile-download) asc_profile_download "$@" ;;
        *)
            echo "Usage: $0 {jwt|app-find|app-create|app-ensure|bundle-find|bundle-create|bundle-ensure|cert-dist|profile-find|profile-create|profile-ensure|profile-download} [args...]" >&2
            exit 1
            ;;
    esac
fi
