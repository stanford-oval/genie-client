import re
from argparse import BooleanOptionalAction
from typing import List, Union, Optional

from clavier import log as logging

from genie_client_cpp.config import CONFIG
from genie_client_cpp.remote import Remote
from genie_client_cpp.context import Context
from . import remove
from . import kill
from . import build as build_cmd


LOG = logging.getLogger(__name__)

COMMANDS_TO_KILL = (
    re.compile(r"/opt/duer/dcslaunch.sh$"),
    re.compile(r"/genie$"),
    re.compile(r"^/tmp/spotifyd\s"),
)


class MultipleChoice(list):
    def __contains__(self, value):
        if isinstance(value, list):
            for item in value:
                if not super().__contains__(value):
                    return False
            return True
        return super().__contains__(value)


def add_to(subparsers):
    parser = subparsers.add_parser(
        "deploy",
        target=run,
        help=f"Push build artifacts under {CONFIG.paths.out.root} to a target device",
    )

    parser.add_argument(
        "-t",
        "--target",
        help=(
            "_destination_ argument for `ssh` or 'adb' to use Android debugger "
            "over a USB cable"
        ),
    )

    parser.add_argument(
        "-b",
        "--build",
        action="store_true",
        default=False,
        help="Build before deploying",
    )

    parser.add_argument(
        "-p",
        "--plain",
        action="store_true",
        default=False,
        help="Pass `--progress plain` to `docker build` (real Docker only!)",
    )

    parser.add_argument(
        "deployables",
        nargs="*",
        choices=MultipleChoice(CONFIG.xiaodu.deployables.keys()),
        help="List of what to deploy. If empty, deploy everything.",
    )


@Context.inject_current
def run(
    target: Union[str, Remote],
    deployables: List[str],
    *,
    build: bool = False,
    plain: bool = False,
):
    if build:
        build_cmd.run(
            exe_only=len(deployables) > 0
            and not (
                "lib" in deployables
                or "assets" in deployables
                or "pulse.exe" in deployables
                or any("tools" in x for x in deployables)
            ),
            plain=plain,
        )

    kill.run(target)

    remote = Remote.create(target)

    if len(deployables) == 0:
        LOG.info("Deploying EVERYTHING!")
        deployables = CONFIG.xiaodu.deployables.keys()

    for deployable_name in deployables:
        deployable = CONFIG.xiaodu.deployables[deployable_name]
        LOG.info(f"Deploying `{deployable_name}`...")
        remote.push(deployable["src"], deployable["dest"])
