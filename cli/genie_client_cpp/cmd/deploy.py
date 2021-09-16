import re
from argparse import BooleanOptionalAction

from clavier import log as logging, CFG

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

OUT_PATHS = CFG.genie_client_cpp.paths.out
SCRIPT_PATHS = CFG.genie_client_cpp.paths.scripts
DEPLOY_PATHS = CFG.genie_client_cpp.xiaodu.paths


def add_to(subparsers):
    parser = subparsers.add_parser(
        "deploy",
        target=run,
        help=f"Push build artifacts under {CONFIG.paths.out} to a target device",
    )

    parser.add_argument(
        "target",
        nargs="?",
        help=(
            "_destination_ argument for `ssh` or 'adb' to use Android debugger "
            "over a USB cable"
        ),
    )

    parser.add_argument(
        "-x",
        "--exe",
        action="store_true",
        default=False,
        help="Push the `genie` bin to the target",
    )

    parser.add_argument(
        "-c",
        "--config",
        action="store_true",
        default=False,
        help="Push `config.ini` to the target",
    )

    parser.add_argument(
        "--assets",
        action="store_true",
        default=False,
        help="Deploy the assets",
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


@LOG.inject
def deploy_all(target: str, log=LOG):
    log.info("FULL deploy (kill, remove, deploy all)...")

    remove.run(target)

    remote = Remote.create(target)

    remote.run("mkdir", "-p", CFG.genie_client_cpp.xiaodu.paths.install)
    remote.push(OUT_PATHS.lib, DEPLOY_PATHS.lib)
    remote.push(OUT_PATHS.assets, DEPLOY_PATHS.assets)
    remote.push(SCRIPT_PATHS.launch, DEPLOY_PATHS.launch)
    remote.push(SCRIPT_PATHS.asoundrc, DEPLOY_PATHS.asoundrc)
    remote.push(OUT_PATHS.config, DEPLOY_PATHS.config)
    remote.push(OUT_PATHS.exe, DEPLOY_PATHS.exe)


@LOG.inject
def deploy_exe(target: str, log=LOG):
    log.info("Deploying executable...")
    kill.run(target)
    Remote.create(target).push(OUT_PATHS.exe, DEPLOY_PATHS.exe)


@LOG.inject
def deploy_config(target: str, log=LOG):
    log.info("Deploying config file...")
    Remote.create(target).push(OUT_PATHS.config, DEPLOY_PATHS.config)

@LOG.inject
def deploy_assets(target: str, log=LOG):
    log.info("Deploying assets...")
    Remote.create(target).push(OUT_PATHS.assets, DEPLOY_PATHS.assets)

@Context.inject_current
def run(
    target: str,
    *,
    build: bool,
    plain: bool,
    exe: bool,
    config: bool,
    assets: bool,
):
    if build:
        build_cmd.run(exe_only=exe, plain=plain)

    if exe or config or assets:
        if exe:
            deploy_exe(target)
        if config:
            deploy_config(target)
        if assets:
            deploy_assets(target)
    else:
        deploy_all(target)
