"""Batch-rename Tripo AI-generated assets under /Game/Assets to short UE names."""
import unreal

# (package path without object suffix, new asset name)
RENAMES = [
    # --- Collectibles / Dangerous ---
    ("/Game/Assets/Collectibles/Dangerous/Texture/toxic+canister+3d+model_basecolor", "T_ToxicCanister_BC"),
    ("/Game/Assets/Collectibles/Dangerous/Material/tripo_node_1b20ad64-7b3b-4d6a-854a-5bb0e485cc79_material", "M_ToxicCanister"),
    ("/Game/Assets/Collectibles/Dangerous/toxic+canister+3d+model", "SM_ToxicCanister"),

    # --- Collectibles / Large ---
    ("/Game/Assets/Collectibles/Large/Texture/rusty+dual+gas+cylinders+3d+model_basecolor", "T_DualGasCylinders_BC"),
    ("/Game/Assets/Collectibles/Large/Texture/rusty+industrial+machine+3d+model_basecolor", "T_IndustrialMachine_BC"),
    ("/Game/Assets/Collectibles/Large/Material/tripo_node_289ecaa9-0fb0-45fe-b4b7-6b1792f4eb45_material", "M_DualGasCylinders"),
    ("/Game/Assets/Collectibles/Large/Material/tripo_node_3748a887-ede2-414d-b412-04ea91864fbd_material", "M_IndustrialMachine"),
    ("/Game/Assets/Collectibles/Large/rusty+dual+gas+cylinders+3d+model", "SM_DualGasCylinders"),
    ("/Game/Assets/Collectibles/Large/rusty+industrial+machine+3d+model", "SM_IndustrialMachine"),

    # --- Collectibles / Medium ---
    ("/Game/Assets/Collectibles/Medium/Texture/rusty+gas+cylinder+3d+model_basecolor", "T_GasCylinder_BC"),
    ("/Game/Assets/Collectibles/Medium/Texture/rusty+toolbox+3d+model_basecolor", "T_Toolbox_BC"),
    ("/Game/Assets/Collectibles/Medium/Material/tripo_node_5fd31f02-4df3-4622-a11b-3240db2c0cd0_material", "M_GasCylinder"),
    ("/Game/Assets/Collectibles/Medium/Material/tripo_node_c171611d-c355-450b-8f2e-1c79edae3690_material", "M_Toolbox"),
    ("/Game/Assets/Collectibles/Medium/rusty+gas+cylinder+3d+model", "SM_GasCylinder"),
    ("/Game/Assets/Collectibles/Medium/rusty+toolbox+3d+model", "SM_Toolbox"),

    # --- Collectibles / Small ---
    ("/Game/Assets/Collectibles/Small/Texture/futuristic+barrel+3d+model_basecolor", "T_Barrel_BC"),
    ("/Game/Assets/Collectibles/Small/Texture/rugged+ecu+3d+model_basecolor", "T_ECU_BC"),
    ("/Game/Assets/Collectibles/Small/Texture/rusty+metal+tin+3d+model_basecolor", "T_MetalTin_BC"),
    ("/Game/Assets/Collectibles/Small/Texture/rusty+power+box+3d+model_basecolor", "T_PowerBox_BC"),
    ("/Game/Assets/Collectibles/Small/Texture/valve+wheel+3d+model_basecolor", "T_ValveWheel_BC"),
    ("/Game/Assets/Collectibles/Small/Material/tripo_node_52c47225-219a-4e72-93cc-2bed8f665f5f_material", "M_Barrel"),
    ("/Game/Assets/Collectibles/Small/Material/tripo_node_7790adaf-c03f-4d45-8dbf-202c4f3648a6_material", "M_ECU"),
    ("/Game/Assets/Collectibles/Small/Material/tripo_node_bca2d456-bfa9-45d6-ae6a-ae770dbeaa6f_material", "M_MetalTin"),
    ("/Game/Assets/Collectibles/Small/Material/tripo_node_cf1ca7b2-59ed-4339-9471-a09a56595785_material", "M_PowerBox"),
    ("/Game/Assets/Collectibles/Small/Material/tripo_node_fc5d8450-b528-4383-8f23-1694c999f995_material", "M_ValveWheel"),
    ("/Game/Assets/Collectibles/Small/futuristic+barrel+3d+model", "SM_Barrel"),
    ("/Game/Assets/Collectibles/Small/rugged+ecu+3d+model", "SM_ECU"),
    ("/Game/Assets/Collectibles/Small/rusty+metal+tin+3d+model", "SM_MetalTin"),
    ("/Game/Assets/Collectibles/Small/rusty+power+box+3d+model", "SM_PowerBox"),
    ("/Game/Assets/Collectibles/Small/valve+wheel+3d+model", "SM_ValveWheel"),

    # --- Killer ---
]

for i in range(15):
    part_key = str(i)
    part_label = f"{i:02d}"
    RENAMES.append((f"/Game/Assets/Killer/Texture/Killer_1_0_tripo_part_{part_key}_basecolor", f"T_Killer_Part{part_label}_BC"))
    RENAMES.append((f"/Game/Assets/Killer/Material/tripo_part_{part_key}_material", f"M_Killer_Part{part_label}"))

RENAMES += [
    ("/Game/Assets/Killer/tripo_convert_fd3f9c29-31cd-4daf-b38e-65264f60645b_Skeleton", "SK_Killer_Skeleton"),
    ("/Game/Assets/Killer/tripo_convert_fd3f9c29-31cd-4daf-b38e-65264f60645b_PhysicsAsset", "SK_Killer_Physics"),
    ("/Game/Assets/Killer/tripo_convert_fd3f9c29-31cd-4daf-b38e-65264f60645b", "SK_Killer"),

    # --- Survivor (Roller) ---
]

for i in range(11):
    part_key = str(i)
    part_label = f"{i:02d}"
    RENAMES.append((f"/Game/Assets/Survivor/Texture/Roller_tripo_part_{part_key}_basecolor", f"T_Roller_Part{part_label}_BC"))
    RENAMES.append((f"/Game/Assets/Survivor/Material/tripo_part_{part_key}_material_001", f"M_Roller_Part{part_label}"))

RENAMES += [
    ("/Game/Assets/Survivor/tripo_convert_56166637-a58d-4d92-a4ef-eaa456176ad6_Skeleton", "SK_Roller_Skeleton"),
    ("/Game/Assets/Survivor/tripo_convert_56166637-a58d-4d92-a4ef-eaa456176ad6_PhysicsAsset", "SK_Roller_Physics"),
    ("/Game/Assets/Survivor/tripo_convert_56166637-a58d-4d92-a4ef-eaa456176ad6", "SK_Roller"),

    # --- Props / Delivery Station ---
    ("/Game/Assets/Pops/DeliveryStation/Texture/delivery_station_basecolor", "T_DeliveryStation_BC"),
    ("/Game/Assets/Pops/DeliveryStation/Material/tripo_node_c093709d-2f85-4040-8cdc-f43eb13f50d7_material", "M_DeliveryStation"),
    ("/Game/Assets/Pops/DeliveryStation/delivery_station", "SM_DeliveryStation"),

    # --- Props / Sci-Fi Door ---
    ("/Game/Assets/Pops/Sci-Fi_Door/Texture/sci_fi_door_texture_0", "T_SciFiDoor_0"),
    ("/Game/Assets/Pops/Sci-Fi_Door/Texture/sci_fi_door_texture_0_cartoon_flat", "T_SciFiDoor_0_CartoonFlat"),
    ("/Game/Assets/Pops/Sci-Fi_Door/Texture/sci_fi_door_texture_0_cartoon_soft", "T_SciFiDoor_0_CartoonSoft"),
    ("/Game/Assets/Pops/Sci-Fi_Door/Texture/sci_fi_door_texture_1", "T_SciFiDoor_1"),
    ("/Game/Assets/Pops/Sci-Fi_Door/Texture/sci_fi_door_texture_2", "T_SciFiDoor_2"),
    ("/Game/Assets/Pops/Sci-Fi_Door/Texture/sci_fi_door_texture_3", "T_SciFiDoor_3"),
    ("/Game/Assets/Pops/Sci-Fi_Door/Material/Sci-fi-door", "M_SciFiDoor"),
    ("/Game/Assets/Pops/Sci-Fi_Door/sci_fi_door_Submesh_C70853CC", "SM_SciFiDoor_Frame"),
    ("/Game/Assets/Pops/Sci-Fi_Door/sci_fi_door_Submesh_E2FEAD16", "SM_SciFiDoor_Door"),
    ("/Game/Assets/Pops/Sci-Fi_Door/sci_fi_door", "SM_SciFiDoor"),
    ("/Game/Assets/Pops/Sci-Fi_Door/Sci-fi-door", "SM_SciFiDoor_Import"),
]


def resolve_object_path(package_path: str) -> str:
    if "." in package_path.split("/")[-1]:
        return package_path
    short_name = package_path.rsplit("/", 1)[-1]
    return f"{package_path}.{short_name}"


def main():
    results = {"renamed": [], "skipped": [], "failed": []}

    for package_path, new_name in RENAMES:
        src = resolve_object_path(package_path)
        if not unreal.EditorAssetLibrary.does_asset_exist(src):
            results["skipped"].append({"src": src, "reason": "not_found"})
            continue

        folder = package_path.rsplit("/", 1)[0]
        dst = f"{folder}/{new_name}.{new_name}"

        if unreal.EditorAssetLibrary.does_asset_exist(dst):
            results["skipped"].append({"src": src, "reason": "dest_exists", "dst": dst})
            continue

        ok = unreal.EditorAssetLibrary.rename_asset(src, dst)
        if ok:
            results["renamed"].append({"from": src, "to": dst})
            unreal.log(f"[rename_ai_assets] {src} -> {dst}")
        else:
            results["failed"].append({"src": src, "dst": dst})
            unreal.log_error(f"[rename_ai_assets] FAILED {src} -> {dst}")

    saved = unreal.EditorAssetLibrary.save_directory("/Game/Assets", only_if_is_dirty=True, recursive=True)
    results["save_directory"] = saved
    unreal.log(f"[rename_ai_assets] done renamed={len(results['renamed'])} skipped={len(results['skipped'])} failed={len(results['failed'])}")
    return results


if __name__ == "__main__":
    main()
