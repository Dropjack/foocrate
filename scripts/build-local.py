"""Invoke the approved VS tools in an isolated, case-normalized environment."""
import os
from pathlib import Path
import subprocess
import sys

tool_root = Path(r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin")
tool = tool_root / ("ctest.exe" if sys.argv[1:2] == ["test"] else "cmake.exe")
if not tool.is_file():
    raise SystemExit("Approved Visual Studio tool is missing; consult task 002.")
env = {k: v for k, v in os.environ.items() if k.upper() != "PATH"}
env["Path"] = os.environ["PATH"]
env["MSBUILDDISABLENODEREUSE"] = "1"
args = sys.argv[2:] if sys.argv[1:2] == ["test"] else sys.argv[1:]
if args[:1] == ["--build"]:
    args += ["--", "/m:1", "/nr:false", "/p:UseMultiToolTask=false"]
raise SystemExit(subprocess.call([str(tool), *args], env=env))
