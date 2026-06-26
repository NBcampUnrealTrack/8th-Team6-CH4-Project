"""Remove leftover ObjectRedirectors and duplicate import stubs under /Game/Assets."""
import unreal

ROOT = "/Game/Assets"


def collect_redirectors():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(True)
    filter = unreal.ARFilter(
        package_paths=[ROOT],
        recursive_paths=True,
        class_names=["ObjectRedirector"],
    )
    return registry.get_assets(filter)


def main():
    deleted = []
    failed = []
    skipped = []

    for asset_data in collect_redirectors():
        pkg_name = str(asset_data.package_name)
        asset_name = str(asset_data.asset_name)
        obj_path = f"{pkg_name}.{asset_name}"

        if not unreal.EditorAssetLibrary.does_asset_exist(obj_path):
            skipped.append({"path": obj_path, "reason": "missing"})
            continue

        if unreal.EditorAssetLibrary.delete_asset(obj_path):
            deleted.append(obj_path)
            unreal.log(f"[cleanup] deleted redirector {obj_path}")
        else:
            failed.append(obj_path)
            unreal.log_error(f"[cleanup] failed {obj_path}")

    unreal.EditorAssetLibrary.save_directory(ROOT, only_if_is_dirty=False, recursive=True)
    unreal.log(
        f"[cleanup] redirectors deleted={len(deleted)} failed={len(failed)} skipped={len(skipped)}"
    )
    return {"deleted": deleted, "failed": failed, "skipped": skipped}


if __name__ == "__main__":
    main()
