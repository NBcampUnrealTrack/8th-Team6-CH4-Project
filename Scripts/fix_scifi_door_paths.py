"""Flatten SciFi_Door assets that were nested one level too deep."""
import unreal

FIXES = [
    (
        "/Game/Assets/Pops/SciFi_Door/SM_SciFiDoor/SM_SciFiDoor.SM_SciFiDoor",
        "/Game/Assets/Pops/SciFi_Door/SM_SciFiDoor.SM_SciFiDoor",
    ),
    (
        "/Game/Assets/Pops/SciFi_Door/SM_SciFiDoor_Frame/SM_SciFiDoor_Frame.SM_SciFiDoor_Frame",
        "/Game/Assets/Pops/SciFi_Door/SM_SciFiDoor_Frame.SM_SciFiDoor_Frame",
    ),
    (
        "/Game/Assets/Pops/SciFi_Door/SM_SciFiDoor_Door/SM_SciFiDoor_Door.SM_SciFiDoor_Door",
        "/Game/Assets/Pops/SciFi_Door/SM_SciFiDoor_Door.SM_SciFiDoor_Door",
    ),
]

NESTED_DIRS = [
    "/Game/Assets/Pops/SciFi_Door/SM_SciFiDoor",
    "/Game/Assets/Pops/SciFi_Door/SM_SciFiDoor_Frame",
    "/Game/Assets/Pops/SciFi_Door/SM_SciFiDoor_Door",
]


def main():
    moved = []
    failed = []
    for src, dst in FIXES:
        if not unreal.EditorAssetLibrary.does_asset_exist(src):
            continue
        if unreal.EditorAssetLibrary.does_asset_exist(dst):
            unreal.log_warning(f"[flatten] dest exists, skip {dst}")
            continue
        if unreal.EditorAssetLibrary.rename_asset(src, dst):
            moved.append({"from": src, "to": dst})
            unreal.log(f"[flatten] {src} -> {dst}")
        else:
            failed.append({"from": src, "to": dst})

    for folder in NESTED_DIRS:
        if unreal.EditorAssetLibrary.does_directory_exist(folder):
            unreal.EditorAssetLibrary.delete_directory(folder)

    unreal.EditorAssetLibrary.save_directory("/Game/Assets/Pops/SciFi_Door", only_if_is_dirty=False, recursive=True)
    return {"moved": moved, "failed": failed}


if __name__ == "__main__":
    main()
