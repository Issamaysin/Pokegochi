"""Launch PlatformIO with the workspace's portable Git on Windows."""

from __future__ import annotations

import os
import runpy
import shutil
from pathlib import Path


project_root = Path(__file__).resolve().parent.parent
git_root = project_root / ".tools" / "git"
portable_paths = (git_root / "cmd", git_root / "mingw64" / "bin")
os.environ["PATH"] = os.pathsep.join(
    [*(str(path) for path in portable_paths), os.environ.get("PATH", "")]
)

# Some portable Python/PlatformIO combinations normalize PATH while loading a
# remote platform and then fail shutil.which("git") even though CreateProcess
# can launch the bundled executable. Keep the lookup deterministic and local.
_which = shutil.which
_bundled_git = git_root / "cmd" / "git.exe"


def _workspace_which(command: str, *args, **kwargs):
    if command.lower() in ("git", "git.exe") and _bundled_git.exists():
        return str(_bundled_git)
    return _which(command, *args, **kwargs)


shutil.which = _workspace_which

runpy.run_module("platformio", run_name="__main__")
