import re
from typing import Dict, Union

from clavier import log as logging, CFG

from genie_client_cpp.remote import Remote
from . import remove


LOG = logging.getLogger(__name__)

COMMANDS_TO_KILL = (
    re.compile(r"/opt/duer/dcslaunch.sh$"),
    re.compile(r"/genie$"),
    re.compile(r"^/tmp/spotifyd\s")
)

def add_to(subparsers):
    parser = subparsers.add_parser(
        "deploy",
        target=run,
        help="Run a command on the remote host",
    )

    parser.add_argument(
        "target",
        help=(
            "_destination_ argument for `ssh` or 'adb' to use Android debugger "
            "over a USB cable"
        ),
    )

def run(target: str):
    remove.run(target)

    remote = Remote.create(target)

    build_paths = CFG.genie_client_cpp.paths.build
    script_paths = CFG.genie_client_cpp.paths.scripts
    deploy_paths = CFG.genie_client_cpp.xiaodu.paths

    remote.run("mkdir", "-p", CFG.genie_client_cpp.xiaodu.paths.install)
    remote.push(build_paths.lib, deploy_paths.lib)
    remote.push(build_paths.assets, deploy_paths.assets)
    remote.push(script_paths.launch, deploy_paths.launch)
    remote.push(build_paths.config, deploy_paths.config)
    remote.push(build_paths.exe, deploy_paths.exe)
