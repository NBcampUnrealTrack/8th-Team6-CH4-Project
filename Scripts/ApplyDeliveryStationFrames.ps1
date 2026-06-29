# Import cutout station frames + apply to WBP. Run with editor open.
param([string]$BaseUrl = "http://127.0.0.1:8080/mcp")

$ErrorActionPreference = "Stop"

for ($i = 0; $i -lt 30; $i += 2) {
    try {
        if ((Invoke-WebRequest -Uri $BaseUrl -Method GET -UseBasicParsing -TimeoutSec 2).StatusCode -eq 200) { break }
    } catch { Start-Sleep -Seconds 2 }
}

Write-Host "Generating PNGs..."
python "E:\Unreal\git\SpaCh4\Scripts\ImportDeliveryStationFrames_trim.py"

Write-Host "Importing to Unreal..."
& "$PSScriptRoot\ImportDeliveryStationFrames.ps1"

Write-Host "Updating WBP..."
& "$PSScriptRoot\FixDeliveryProgressFrame.ps1"

Write-Host "Done."
