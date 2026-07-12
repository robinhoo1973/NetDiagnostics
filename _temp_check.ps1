$lines = Get-Content -Path 'src/Diagnostics/Controller/TaskFactory.cpp'
for ($i = 0; $i -lt $lines.Count; $i++) {
    $l = $lines[$i]
    if ($l -match '^\s*#\s*(if|ifdef|ifndef|elif|else|endif)') {
        Write-Host ('{0,4}: {1}' -f ($i+1), $l.Trim())
    }
}
