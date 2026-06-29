import unreal
from pathlib import Path


ROOT = Path(r"E:\Unreal\git\SpaCh4_Copy\Content\UI\HUD")


def main():
    tasks = []
    for source in ROOT.rglob("*.png"):
        rel_dir = source.parent.relative_to(ROOT).as_posix()
        destination = "/Game/UI/HUD" if rel_dir == "." else f"/Game/UI/HUD/{rel_dir}"

        task = unreal.AssetImportTask()
        task.filename = str(source)
        task.destination_path = destination
        task.destination_name = source.stem
        task.automated = True
        task.replace_existing = True
        task.replace_existing_settings = False
        task.save = True
        tasks.append(task)

    if not tasks:
        raise RuntimeError(f"No png files found under {ROOT}")

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    unreal.EditorAssetLibrary.save_directory("/Game/UI/HUD", only_if_is_dirty=False, recursive=True)
    unreal.log(f"Reimported {len(tasks)} HUD textures with transparent PNG sources.")


main()
