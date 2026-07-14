#Requires -Version 5.1
$ErrorActionPreference = 'Stop'

$repoRoot = git rev-parse --show-toplevel
if (-not $repoRoot) {
    throw 'Not inside a git repository.'
}

Set-Location $repoRoot
$hooksPath = Join-Path $repoRoot 'githooks'

git config core.hooksPath githooks
Write-Host "core.hooksPath -> githooks"

& (Join-Path $repoRoot 'Tools\git\sync-work-scope.ps1')
Write-Host 'Hooks installed. Edit .git-work-allowlist to change your work scope.'
