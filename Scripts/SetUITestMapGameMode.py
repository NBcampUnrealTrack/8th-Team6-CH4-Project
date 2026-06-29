import unreal

MAP = "/Game/Maps/L_UI_Test"
GM_CLASS = "/Script/SpaCh4.UITestGameMode"

unreal.EditorLoadingAndSavingUtils.load_map(MAP)
editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = editor_subsystem.get_editor_world()
if not world:
    raise RuntimeError("No editor world")

actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world_settings = None
for actor in actor_subsystem.get_all_level_actors():
    class_name = actor.get_class().get_name()
    if class_name in ("WorldSettings", "BP_WorldSettings_C"):
        world_settings = actor
        break

if not world_settings:
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.WorldSettings.static_class()):
        world_settings = actor
        break

if not world_settings:
    raise RuntimeError("WorldSettings not found")

gm = unreal.load_class(None, GM_CLASS)
world_settings.set_editor_property("default_game_mode", gm)
unreal.EditorLoadingAndSavingUtils.save_current_level()
unreal.log(f"Set {MAP} default_game_mode -> {GM_CLASS}")
