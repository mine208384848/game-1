"""Manually (re)generates the TacOps materials.

Editor:   Tools > Execute Python Script... > Tools/setup_assets.py
Command:  UnrealEditor-Cmd TacOps.uproject -run=pythonscript -script="<project>/Tools/setup_assets.py"

Pass "force" as the first script argument to rebuild materials that already exist.
"""

import os
import sys

import unreal

PROJECT_PY = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()), "Python")
if PROJECT_PY not in sys.path:
    sys.path.append(PROJECT_PY)

import tacops_setup  # noqa: E402

FORCE = any(arg.lower() == "force" for arg in sys.argv[1:])
count = tacops_setup.ensure_materials(force=FORCE)
unreal.log("[TacOps] setup_assets finished, %d material(s) built" % count)
