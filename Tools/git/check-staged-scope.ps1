#Requires -Version 5.1
param([switch]$Quiet)

$ErrorActionPreference = 'Stop'

function Normalize-Path([string]$Path) {
    return ($Path -replace '\\', '/').TrimStart('./')
}

function Test-AllowlistMatch([string]$RelativePath, [string[]]$Patterns) {
    $normalized = Normalize-Path $RelativePath
    foreach ($pattern in $Patterns) {
        if ($pattern.Contains('*') -or $pattern.Contains('?')) {
            if ($normalized -like $pattern) {
                return $true
            }
            continue
        }

        if ($normalized -eq $pattern) {
            return $true
        }

        if ($pattern.EndsWith('/')) {
            if ($normalized.StartsWith($pattern)) {
                return $true
            }
        }
        elseif ($normalized.StartsWith("$pattern/")) {
            return $true
        }
    }
    return $false
}

$repoRoot = git rev-parse --show-toplevel
Set-Location $repoRoot

$patterns = @()
Get-Content '.git-work-allowlist' | ForEach-Object {
    $line = $_.Trim().TrimEnd("`r")
    if ($line -and -not $line.StartsWith('#')) {
        $patterns += (Normalize-Path $line)
    }
}

$blocked = @()
foreach ($file in @(git diff --cached --name-only)) {
    if (-not (Test-AllowlistMatch $file $patterns)) {
        $blocked += $file
    }
}

if ($blocked.Count -gt 0) {
    if (-not $Quiet) {
        Write-Error @"
Staged files outside .git-work-allowlist:
$($blocked -join [Environment]::NewLine)
Edit .git-work-allowlist or unstage them.
"@
    }
    exit 1
}

exit 0
