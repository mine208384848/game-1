"""TAC-OPS asset generator (Unreal Editor Python).

Creates the parent materials the game looks for at runtime:

    /Game/TacOps/Materials/M_TO_Master   opaque PBR master (BaseColor, Roughness, Metallic,
                                         EmissiveStrength, VertexColorAmount, Variation)
    /Game/TacOps/Materials/M_TO_Glass    translucent glass
    /Game/TacOps/Materials/M_TO_Water    translucent water surface
    /Game/TacOps/Materials/M_TO_Smoke    soft volumetric-lit smoke
    /Game/TacOps/Materials/M_TO_Unlit    emissive lamps / screens / tracers

Without these the game still runs (it falls back to the engine BasicShapeMaterial), but the
generated masters add world-space color variation, proper roughness / metal response and
translucency, which is a big part of the Delta Force-like look.

Artists can override any surface by creating a material at
    /Game/TacOps/Materials/Overrides/MO_<Name>     (e.g. MO_Grass, MO_Concrete, MO_Asphalt)
for example from a Quixel Megascans surface. Names are listed in TOMaterialLibrary.cpp.
"""

import unreal

MATERIAL_DIR = "/Game/TacOps/Materials"

_asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
_mel = unreal.MaterialEditingLibrary
_eal = unreal.EditorAssetLibrary


def _log(msg):
    unreal.log("[TacOps] " + msg)


def _set(obj, prop, value):
    try:
        obj.set_editor_property(prop, value)
        return True
    except Exception as exc:  # property names differ slightly between engine versions
        unreal.log_warning("[TacOps] could not set %s.%s: %s" % (obj.get_name(), prop, exc))
        return False


def _expr(mat, cls, x, y):
    return _mel.create_material_expression(mat, cls, x, y)


def _scalar(mat, name, default, x, y):
    e = _expr(mat, unreal.MaterialExpressionScalarParameter, x, y)
    _set(e, "parameter_name", name)
    _set(e, "default_value", float(default))
    return e


def _vector(mat, name, color, x, y):
    e = _expr(mat, unreal.MaterialExpressionVectorParameter, x, y)
    _set(e, "parameter_name", name)
    _set(e, "default_value", unreal.LinearColor(*color))
    return e


def _const(mat, value, x, y):
    e = _expr(mat, unreal.MaterialExpressionConstant, x, y)
    _set(e, "r", float(value))
    return e


def _binary(mat, cls, a, b, x, y, a_out="", b_out=""):
    e = _expr(mat, cls, x, y)
    _mel.connect_material_expressions(a, a_out, e, "A")
    _mel.connect_material_expressions(b, b_out, e, "B")
    return e


def _new_material(name):
    path = MATERIAL_DIR + "/" + name
    if _eal.does_asset_exist(path):
        _eal.delete_asset(path)
    mat = _asset_tools.create_asset(name, MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew())
    # Everything in the world is rendered through (hierarchical) instanced static meshes.
    _set(mat, "used_with_instanced_static_meshes", True)
    _set(mat, "used_with_static_lighting", True)
    return mat


def _finish(mat):
    _mel.layout_material_expressions(mat)
    _mel.recompile_material(mat)
    _eal.save_loaded_asset(mat)
    _log("created " + mat.get_path_name())


def _varied_color(mat):
    """BaseColor * (1 + Variation * worldNoise) * lerp(1, VertexColor, VertexColorAmount)."""
    base = _vector(mat, "BaseColor", (0.5, 0.5, 0.5, 1.0), -1400, -200)
    variation = _scalar(mat, "Variation", 0.18, -1400, 0)
    vca = _scalar(mat, "VertexColorAmount", 0.0, -1400, 200)

    wpos = _expr(mat, unreal.MaterialExpressionWorldPosition, -1400, 400)
    noise = _expr(mat, unreal.MaterialExpressionNoise, -1150, 400)
    _set(noise, "scale", 0.0035)
    _set(noise, "levels", 4)
    _set(noise, "output_min", -1.0)
    _set(noise, "output_max", 1.0)
    _set(noise, "turbulence", True)
    _mel.connect_material_expressions(wpos, "", noise, "Position")

    var_noise = _binary(mat, unreal.MaterialExpressionMultiply, variation, noise, -900, 100)
    one = _const(mat, 1.0, -900, 0)
    factor = _binary(mat, unreal.MaterialExpressionAdd, one, var_noise, -700, 50)
    varied = _binary(mat, unreal.MaterialExpressionMultiply, base, factor, -500, -100)

    vcol = _expr(mat, unreal.MaterialExpressionVertexColor, -900, 300)
    lerp = _expr(mat, unreal.MaterialExpressionLinearInterpolate, -700, 300)
    _mel.connect_material_expressions(one, "", lerp, "A")
    _mel.connect_material_expressions(vcol, "", lerp, "B")
    _mel.connect_material_expressions(vca, "", lerp, "Alpha")

    return _binary(mat, unreal.MaterialExpressionMultiply, varied, lerp, -300, 0)


def build_master():
    mat = _new_material("M_TO_Master")
    color = _varied_color(mat)
    rough = _scalar(mat, "Roughness", 0.7, -300, 250)
    metal = _scalar(mat, "Metallic", 0.0, -300, 350)
    emissive = _scalar(mat, "EmissiveStrength", 0.0, -300, 450)
    _scalar(mat, "Opacity", 1.0, -300, 550)  # unused by the opaque master, keeps parameter sets uniform
    emit = _binary(mat, unreal.MaterialExpressionMultiply, color, emissive, -100, 450)

    _mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    _mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    _mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    _mel.connect_material_property(emit, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    _finish(mat)


def _translucent(name, lighting_mode, default_color, default_opacity, default_rough):
    mat = _new_material(name)
    _set(mat, "blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    _set(mat, "translucency_lighting_mode", lighting_mode)
    base = _vector(mat, "BaseColor", default_color, -600, -100)
    opacity = _scalar(mat, "Opacity", default_opacity, -600, 100)
    rough = _scalar(mat, "Roughness", default_rough, -600, 200)
    metal = _scalar(mat, "Metallic", 0.0, -600, 300)
    emissive = _scalar(mat, "EmissiveStrength", 0.0, -600, 400)
    _scalar(mat, "VertexColorAmount", 0.0, -600, 500)
    _scalar(mat, "Variation", 0.0, -600, 600)
    emit = _binary(mat, unreal.MaterialExpressionMultiply, base, emissive, -300, 400)
    _mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    _mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    _mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    _mel.connect_material_property(emit, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    return mat, opacity


def build_glass():
    mat, opacity = _translucent("M_TO_Glass", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING, (0.55, 0.65, 0.7, 1.0), 0.22, 0.05)
    _set(mat, "two_sided", True)
    _mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    _finish(mat)


def build_water():
    mat, opacity = _translucent("M_TO_Water", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING, (0.05, 0.12, 0.12, 1.0), 0.82, 0.04)
    # Soft shore line where the water plane meets the terrain.
    fade = _expr(mat, unreal.MaterialExpressionDepthFade, -300, 100)
    _set(fade, "fade_distance_default", 120.0)
    _mel.connect_material_expressions(opacity, "", fade, "Opacity")
    _mel.connect_material_property(fade, "", unreal.MaterialProperty.MP_OPACITY)
    _finish(mat)


def build_smoke():
    mat, opacity = _translucent("M_TO_Smoke", unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL, (0.6, 0.6, 0.6, 1.0), 0.65, 1.0)
    _set(mat, "two_sided", True)
    # Soft, round puffs: fade towards the silhouette and where they intersect geometry.
    fresnel = _expr(mat, unreal.MaterialExpressionFresnel, -450, 250)
    _set(fresnel, "exponent", 1.5)
    soft = _expr(mat, unreal.MaterialExpressionOneMinus, -300, 250)
    _mel.connect_material_expressions(fresnel, "", soft, "")
    puff = _binary(mat, unreal.MaterialExpressionMultiply, opacity, soft, -200, 150)
    fade = _expr(mat, unreal.MaterialExpressionDepthFade, -50, 150)
    _set(fade, "fade_distance_default", 250.0)
    _mel.connect_material_expressions(puff, "", fade, "Opacity")
    _mel.connect_material_property(fade, "", unreal.MaterialProperty.MP_OPACITY)
    _finish(mat)


def build_unlit():
    mat = _new_material("M_TO_Unlit")
    _set(mat, "shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    base = _vector(mat, "BaseColor", (1.0, 0.9, 0.7, 1.0), -600, 0)
    strength = _scalar(mat, "EmissiveStrength", 2.0, -600, 200)
    for name, value, y in (("Roughness", 1.0, 300), ("Metallic", 0.0, 400), ("Opacity", 1.0, 500), ("VertexColorAmount", 0.0, 600), ("Variation", 0.0, 700)):
        _scalar(mat, name, value, -600, y)
    emit = _binary(mat, unreal.MaterialExpressionMultiply, base, strength, -300, 100)
    _mel.connect_material_property(emit, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    _finish(mat)


BUILDERS = (
    ("M_TO_Master", build_master),
    ("M_TO_Glass", build_glass),
    ("M_TO_Water", build_water),
    ("M_TO_Smoke", build_smoke),
    ("M_TO_Unlit", build_unlit),
)


def ensure_materials(force=False):
    """Creates any missing material (or all of them when force=True)."""
    if not _eal.does_directory_exist(MATERIAL_DIR):
        _eal.make_directory(MATERIAL_DIR)
    if not _eal.does_directory_exist(MATERIAL_DIR + "/Overrides"):
        _eal.make_directory(MATERIAL_DIR + "/Overrides")
    built = 0
    for name, builder in BUILDERS:
        if force or not _eal.does_asset_exist(MATERIAL_DIR + "/" + name):
            try:
                builder()
                built += 1
            except Exception as exc:
                unreal.log_error("[TacOps] failed to build %s: %s" % (name, exc))
    if built == 0:
        _log("materials up to date")
    return built
