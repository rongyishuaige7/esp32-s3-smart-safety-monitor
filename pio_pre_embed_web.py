# PlatformIO pre-script: generate the embedded HTML C source in .pio, never in the public source tree.
Import("env")

from pathlib import Path
import subprocess

project = Path(env.subst("$PROJECT_DIR"))
source = project / "main" / "web" / "index.html"
generator = project / "tools" / "embed_web.py"
output = project / ".pio" / "generated" / "web_index_embed.c"
output.parent.mkdir(parents=True, exist_ok=True)
subprocess.check_call(["python3", str(generator), str(source), str(output)], cwd=project)
