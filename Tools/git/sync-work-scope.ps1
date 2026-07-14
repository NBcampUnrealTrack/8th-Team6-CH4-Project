#Requires -Version 5.1
<#
.SYNOPSIS
  .git-work-allowlist 밖의 변경은 status에서 숨기고(unstage + skip-worktree) 정리합니다.
#>
param(
    [switch]$Quiet,
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'

function Write-Info([string]$Message) {
    if (-not $Quiet) {
        Write-Host $Message
    }
}

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

function Invoke-GitIndexUpdate([string]$Flag, [string[]]$Files) {
    if ($Files.Count -eq 0) {
        return
    }

    $chunkSize = 150
    for ($i = 0; $i -lt $Files.Count; $i += $chunkSize) {
        $end = [Math]::Min($i + $chunkSize - 1, $Files.Count - 1)
        $chunk = $Files[$i..$end]
        & git update-index $Flag -- @chunk
        if ($LASTEXITCODE -ne 0) {
            throw "git update-index $Flag failed (chunk starting at $i)"
        }
    }
}

function Update-LocalExclude([string]$RepoRoot, [string[]]$Patterns) {
    $excludeFile = Join-Path $RepoRoot '.git\info\exclude'
    $localPatternsFile = Join-Path $RepoRoot 'Tools\git\local-exclude.txt'
    if (-not (Test-Path $localPatternsFile)) {
        return
    }

    $beginMarker = '# BEGIN git-work-scope (auto)'
    $endMarker = '# END git-work-scope (auto)'
    $localPatterns = Get-Content $localPatternsFile | ForEach-Object { $_.Trim() } |
        Where-Object { $_ -and -not $_.StartsWith('#') }

    $existing = if (Test-Path $excludeFile) { Get-Content $excludeFile -Raw } else { '' }
    $head = $existing
    if ($head -match "(?s)(.*)$beginMarker.*?$endMarker(.*)") {
        $head = ($Matches[1] + $Matches[2]).TrimEnd()
    }

    $block = @($beginMarker) + $localPatterns + @($endMarker)
    $newContent = (($head, ($block -join [Environment]::NewLine)) | Where-Object { $_ }) -join ([Environment]::NewLine + [Environment]::NewLine)
    if (-not $DryRun) {
        Set-Content -Path $excludeFile -Value $newContent -Encoding UTF8
    }
}

$repoRoot = git rev-parse --show-toplevel
if (-not $repoRoot) {
    throw 'Not inside a git repository.'
}
Set-Location $repoRoot

$patterns = @()
Get-Content '.git-work-allowlist' | ForEach-Object {
    $line = $_.Trim().TrimEnd("`r")
    if ($line -and -not $line.StartsWith('#')) {
        $patterns += (Normalize-Path $line)
    }
}
if ($patterns.Count -eq 0) {
    throw 'Allowlist is empty. Add at least one path to .git-work-allowlist.'
}

$unstaged = 0
$skipSet = 0
$skipCleared = 0

# 1) allowlist 밖 staged 파일 unstage
foreach ($file in @(git diff --cached --name-only)) {
    if (-not (Test-AllowlistMatch $file $patterns)) {
        if ($DryRun) {
            Write-Info "[dry-run] unstage: $file"
        }
        else {
            git reset -q HEAD -- $file | Out-Null
        }
        $unstaged++
    }
}

# 2) skip-worktree 일괄 동기화
$skipWorktreeFiles = New-Object 'System.Collections.Generic.HashSet[string]'
foreach ($line in @(git ls-files -v)) {
    if ($line -match '^[Ss]\s+(.+)$') {
        [void]$skipWorktreeFiles.Add($Matches[1])
    }
}

$toSkip = New-Object 'System.Collections.Generic.List[string]'
$toUnskip = New-Object 'System.Collections.Generic.List[string]'

foreach ($file in @(git ls-files)) {
    $inScope = Test-AllowlistMatch $file $patterns
    $hasSkip = $skipWorktreeFiles.Contains($file)

    if ($inScope) {
        if ($hasSkip) {
            $toUnskip.Add($file)
        }
    }
    elseif (-not $hasSkip) {
        $toSkip.Add($file)
    }
}

if (-not $DryRun) {
    Invoke-GitIndexUpdate '--no-skip-worktree' @($toUnskip)
    Invoke-GitIndexUpdate '--skip-worktree' @($toSkip)
}
else {
    foreach ($file in $toUnskip) { Write-Info "[dry-run] no-skip-worktree: $file" }
    foreach ($file in $toSkip) { Write-Info "[dry-run] skip-worktree: $file" }
}

$skipCleared = $toUnskip.Count
$skipSet = $toSkip.Count

# 3) untracked 노이즈 — local-exclude.txt 를 .git/info/exclude 에 반영
Update-LocalExclude $repoRoot $patterns

Write-Info "Work scope sync: unstaged=$unstaged, skip-worktree=$skipSet, restored-visible=$skipCleared"
