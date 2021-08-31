from __future__ import annotations

from clavier import Sesh, CFG

import genie_client_cpp.config # NEED this! And FIRST!
from genie_client_cpp import cmd


def run():
    sesh = Sesh(
        __name__,
        CFG.genie_client_cpp.paths.cli.root / "README.md",
        cmd.add_to
    )
    sesh.setup(CFG.genie_client_cpp.log.level)
    sesh.parse()
    sesh.exec()
