# DO NOT run via MCP — import-assets triggers editor debugger breaks.
# Manual: Content Browser -> Content/UI/HUD/Textures/Common -> Reimport
Write-Host @"
HUD panel textures (PNG on disk):
  Content/UI/HUD/Textures/Common/T_HUD_Panel_BG.png
  Content/UI/HUD/Textures/Common/T_HUD_Panel_BG_Wide.png
  Content/UI/HUD/Textures/Common/T_HUD_Panel_BG_Bar.png

Reimport in editor, then run ApplyHUDLayout.ps1
"@
