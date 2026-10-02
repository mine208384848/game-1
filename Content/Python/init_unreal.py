"""Runs automatically when the editor starts (Python Editor Script Plugin).

Generates the TacOps master materials the first time the project is opened.
The work is deferred to the first editor tick so the asset registry is ready.
"""

import unreal

_tick_handle = None


def _run_once(_delta_seconds):
    global _tick_handle
    if _tick_handle is not None:
        unreal.unregister_slate_post_tick_callback(_tick_handle)
        _tick_handle = None
    try:
        import tacops_setup
        tacops_setup.ensure_materials(force=False)
    except Exception as exc:
        unreal.log_warning("[TacOps] material setup skipped: %s" % exc)


_tick_handle = unreal.register_slate_post_tick_callback(_run_once)
