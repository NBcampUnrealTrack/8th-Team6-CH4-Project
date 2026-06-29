# Removes HUD textures not referenced by current WBP/C++ layout.
$Root = "E:\Unreal\git\SpaCh4\Content\UI\HUD"

$RelativePaths = @(
    # Legacy root duplicates (moved under Textures/)
    "T_HUD_Bar_BG.uasset",
    "T_HUD_Bar_Fill.uasset",
    "T_HUD_CageStack_Empty.uasset",
    "T_HUD_CageStack_Filled.uasset",
    "T_HUD_Divider.uasset",

    # Common — unused panel wide / divider
    "Textures\Common\T_HUD_Divider.png",
    "Textures\Common\T_HUD_Divider.uasset",
    "Textures\Common\T_HUD_Panel_BG_Wide.png",
    "Textures\Common\T_HUD_Panel_BG_Wide.uasset",

    # Cage stack icons (text-only in HUD)
    "Icons\CageStack\T_HUD_CageStack_Empty.png",
    "Icons\CageStack\T_HUD_CageStack_Empty.uasset",
    "Icons\CageStack\T_HUD_CageStack_Filled.png",
    "Icons\CageStack\T_HUD_CageStack_Filled.uasset",

    # Delivery — legacy stack / duplicate slots / old fill bar
    "Textures\Delivery\T_HUD_Delivery_Slot_A.png",
    "Textures\Delivery\T_HUD_Delivery_Slot_A.uasset",
    "Textures\Delivery\T_HUD_Delivery_Slot_B.png",
    "Textures\Delivery\T_HUD_Delivery_Slot_B.uasset",
    "Textures\Delivery\T_HUD_Delivery_Progress_Fill.png",
    "Textures\Delivery\T_HUD_Delivery_Progress_Fill.uasset",
    "Textures\Delivery\T_HUD_Delivery_Stack_Empty.png",
    "Textures\Delivery\T_HUD_Delivery_Stack_Empty.uasset",
    "Textures\Delivery\T_HUD_Delivery_Stack_Filled.png",
    "Textures\Delivery\T_HUD_Delivery_Stack_Filled.uasset",
    "Textures\Delivery\T_HUD_Delivery_Stack_Track.png",
    "Textures\Delivery\T_HUD_Delivery_Stack_Track.uasset",

    # Inventory / Perk — state variants not wired yet
    "Textures\Inventory\T_HUD_Inventory_Slot_Filled.png",
    "Textures\Inventory\T_HUD_Inventory_Slot_Filled.uasset",
    "Textures\Inventory\T_HUD_Inventory_Slot_Hover.png",
    "Textures\Inventory\T_HUD_Inventory_Slot_Hover.uasset",
    "Textures\Perk\T_HUD_Perk_Slot_Equipped.png",
    "Textures\Perk\T_HUD_Perk_Slot_Equipped.uasset",
    "Textures\Perk\T_HUD_Perk_Slot_Cooldown.png",
    "Textures\Perk\T_HUD_Perk_Slot_Cooldown.uasset",

    # Teammate — per-state frames replaced by Portrait_Slot tint
    "Textures\Teammate\T_HUD_Entry_Row_BG.png",
    "Textures\Teammate\T_HUD_Entry_Row_BG.uasset",
    "Textures\Teammate\T_HUD_Portrait_BG.png",
    "Textures\Teammate\T_HUD_Portrait_BG.uasset",
    "Textures\Teammate\T_HUD_Portrait_Frame.png",
    "Textures\Teammate\T_HUD_Portrait_Frame.uasset",
    "Textures\Teammate\T_HUD_Portrait_Frame_Healthy.png",
    "Textures\Teammate\T_HUD_Portrait_Frame_Healthy.uasset",
    "Textures\Teammate\T_HUD_Portrait_Frame_Injured.png",
    "Textures\Teammate\T_HUD_Portrait_Frame_Injured.uasset",
    "Textures\Teammate\T_HUD_Portrait_Frame_Downed.png",
    "Textures\Teammate\T_HUD_Portrait_Frame_Downed.uasset",
    "Textures\Teammate\T_HUD_Portrait_Frame_Caged.png",
    "Textures\Teammate\T_HUD_Portrait_Frame_Caged.uasset",
    "Textures\Teammate\T_HUD_Portrait_Frame_Dead.png",
    "Textures\Teammate\T_HUD_Portrait_Frame_Dead.uasset",
    "Textures\Teammate\T_HUD_Portrait_Frame_Escaped.png",
    "Textures\Teammate\T_HUD_Portrait_Frame_Escaped.uasset",
    "Textures\Teammate\T_HUD_Portrait_Robot_A.png",
    "Textures\Teammate\T_HUD_Portrait_Robot_A.uasset",
    "Textures\Teammate\T_HUD_Portrait_Robot_B.png",
    "Textures\Teammate\T_HUD_Portrait_Robot_B.uasset",
    "Textures\Teammate\T_HUD_Portrait_Robot_C.png",
    "Textures\Teammate\T_HUD_Portrait_Robot_C.uasset"
)

$removed = 0
$missing = 0
foreach ($rel in $RelativePaths) {
    $full = Join-Path $Root $rel
    if (Test-Path $full) {
        Remove-Item -Force $full
        Write-Host "[removed] $rel"
        $removed++
    } else {
        Write-Host "[skip] $rel"
        $missing++
    }
}

# Remove empty CageStack folder if nothing left
$cageDir = Join-Path $Root "Icons\CageStack"
if ((Test-Path $cageDir) -and -not (Get-ChildItem $cageDir -Force | Select-Object -First 1)) {
    Remove-Item -Force $cageDir
    Write-Host "[removed] empty Icons\CageStack"
}

Write-Host "Done. Removed $removed files ($missing already absent)."
