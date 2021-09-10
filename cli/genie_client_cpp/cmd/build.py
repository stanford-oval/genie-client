from typing import Literal
from argparse import BooleanOptionalAction

from clavier import log as logging, CFG, sh


LOG = logging.getLogger(__name__)
DEFAULT_ARCH = "arm32v7"

TArch = Literal["arm32v7", "amd64", "arm64v8"]

def add_to(subparsers):
    parser = subparsers.add_parser(
        "build",
        target=run,
        help="Build the client",
    )

    parser.add_argument(
        "-a",
        "--arch",
        choices=TArch.__args__,
        default=DEFAULT_ARCH,
        help="Architecture to build for",
    )

    parser.add_argument(
        "-s",
        "--static",
        action=BooleanOptionalAction,
        default=False,
        help="Do a static build (may not work?)",
    )

    parser.add_argument(
        "-x",
        "--exe-only",
        action=BooleanOptionalAction,
        default=False,
        help="Only copy the resulting binary over from the build container",
    )

    parser.add_argument(
        "-p",
        "--plain",
        action=BooleanOptionalAction,
        default=False,
        help="Pass `--progress plain` to `docker build` (real Docker only!)",
    )

def run(
    arch: str = DEFAULT_ARCH,
    static: bool = False,
    exe_only: bool = False,
    plain: bool = False
):
    tag = f"genie-builder:{arch}"

    opts = {
        "build-arg": [
            f"ARCH={arch}/", # TODO Why is this `/` added here?
            f"STATIC={int(static)}"
        ],
        "tag": tag,
        "file": CFG.genie_client_cpp.paths.scripts.dockerfile,
    }

    if plain:
        opts["progress"] = "plain"

    sh.run(
        "docker",
        "build",
        opts,
        ".",
        chdir=CFG.genie_client_cpp.paths.repo,
        log=LOG,
        rel_paths=True,
        opts_style=" ",
    )

    if exe_only:
        script = "/src/scripts/binonly.sh"
    else:
        script = "/src/scripts/blob.sh"

    sh.run(
        "docker",
        "run",
        {
            "rm": True,
            "volume": f"{CFG.genie_client_cpp.paths.out.root}:/out",
            "security-opt": "label=disable",
            "env": f"ARCH={arch}",
        },
        tag,
        script,
        chdir=CFG.genie_client_cpp.paths.repo,
        log=LOG,
        rel_paths=True,
        opts_style=" ",
    )
