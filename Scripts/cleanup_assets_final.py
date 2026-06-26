"""Final cleanup: redirectors, stray source files, and folder rename Sci-Fi_Door -> SciFi_Door."""
import os
import unreal

OLD_FOLDER = "/Game/Assets/Pops/Sci-Fi_Door"
NEW_FOLDER = "/Game/Assets/Pops/SciFi_Door"
GAME_ROOTS = ["/Game/Assets", "/Game/Fab", "/Game/_GENERATED"]


def collect_redirectors(roots):
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(True)
    found = []
    for root in roots:
        if not unreal.EditorAssetLibrary.does_directory_exist(root):
            continue
        filter = unreal.ARFilter(
            package_paths=[root],
            recursive_paths=True,
            class_names=["ObjectRedirector"],
        )
        found.extend(registry.get_assets(filter))
    return found


def move_sci_fi_door_folder():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(True)
    filter = unreal.ARFilter(package_paths=[OLD_FOLDER], recursive_paths=True)
    assets = registry.get_assets(filter)
    if not assets:
        return {"moved": [], "skipped": ["folder_not_found"]}

    if not unreal.EditorAssetLibrary.does_directory_exist(NEW_FOLDER):
        unreal.EditorAssetLibrary.make_directory(NEW_FOLDER)

    moved = []
    failed = []
    for asset_data in assets:
        pkg_name = str(asset_data.package_name)
        asset_name = str(asset_data.asset_name)
        src = f"{pkg_name}.{asset_name}"

        if not unreal.EditorAssetLibrary.does_asset_exist(src):
            continue

        sub_path = pkg_name[len(OLD_FOLDER) :].lstrip("/")
        if not sub_path or "/" not in sub_path:
            dest_dir = NEW_FOLDER
        else:
            dest_dir = f"{NEW_FOLDER}/{sub_path.rsplit('/', 1)[0]}"
        unreal.EditorAssetLibrary.make_directory(dest_dir)

        dst = f"{dest_dir}/{asset_name}.{asset_name}"
        if unreal.EditorAssetLibrary.does_asset_exist(dst):
            failed.append({"src": src, "reason": "dest_exists", "dst": dst})
            continue

        if unreal.EditorAssetLibrary.rename_asset(src, dst):
            moved.append({"from": src, "to": dst})
            unreal.log(f"[cleanup] moved {src} -> {dst}")
        else:
            failed.append({"src": src, "dst": dst})

    if unreal.EditorAssetLibrary.does_directory_exist(OLD_FOLDER):
        unreal.EditorAssetLibrary.delete_directory(OLD_FOLDER)

    return {"moved": moved, "failed": failed}


def delete_stray_project_source():
    project_dir = unreal.Paths.project_dir()
    candidates = [
        os.path.join(project_dir, "sci_fi_door_texture_0.PNG"),
        os.path.join(project_dir, "sci_fi_door_texture_0.png"),
    ]
    removed = []
    for path in candidates:
        if os.path.isfile(path):
            os.remove(path)
            removed.append(path)
            unreal.log(f"[cleanup] removed stray source file {path}")
    return removed


def main():
    deleted_redirectors = []
    failed_redirectors = []
    for asset_data in collect_redirectors(GAME_ROOTS):
        pkg_name = str(asset_data.package_name)
        asset_name = str(asset_data.asset_name)
        obj_path = f"{pkg_name}.{asset_name}"
        if unreal.EditorAssetLibrary.delete_asset(obj_path):
            deleted_redirectors.append(obj_path)
        else:
            failed_redirectors.append(obj_path)

    folder_result = move_sci_fi_door_folder()
    removed_files = delete_stray_project_source()

    unreal.EditorAssetLibrary.save_directory("/Game/Assets", only_if_is_dirty=False, recursive=True)
    result = {
        "deleted_redirectors": deleted_redirectors,
        "failed_redirectors": failed_redirectors,
        "folder_move": folder_result,
        "removed_source_files": removed_files,
    }
    unreal.log(f"[cleanup] done {result}")
    return result


if __name__ == "__main__":
    main()
