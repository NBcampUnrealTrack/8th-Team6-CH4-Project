import unreal

paths = [
    "/Game/Blueprints/UI/WBP_TeammateEntry",
    "/Game/Blueprints/UI/WBP_GameHUD",
    "/Game/Blueprints/UI/BP_GameHUD",
    "/Game/Blueprints/BP_MatchGameMode",
]

for path in paths:
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
        print(f"deleted {path}")

for folder in ("/Game/Blueprints/UI", "/Game/Blueprints"):
    if not unreal.EditorAssetLibrary.does_directory_exist(folder):
        unreal.EditorAssetLibrary.make_directory(folder)

print("cleanup done")
