import re
from argparse import BooleanOptionalAction

from clavier import log as logging, CFG

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
        help="Run a command on the remote host",
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
        "--exe-only",
        action=BooleanOptionalAction,
        default=False,
        help="Only push the `genie` bin to the target",
    )

    parser.add_argument(
        "-c",
        "--config-only",
        action=BooleanOptionalAction,
        default=False,
        help="Only push `config.ini` to the target",
    )

    parser.add_argument(
        "-b",
        "--build",
        action=BooleanOptionalAction,
        default=False,
        help="Build before deploying",
    )

    parser.add_argument(
        "-p",
        "--plain",
        action=BooleanOptionalAction,
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


@Context.inject_current
def run(
    target: str,
    build: bool,
    exe_only: bool,
    config_only: bool,
    plain: bool
):
    if build:
        build_cmd.run(exe_only=exe_only, plain=plain)

    if exe_only or config_only:
        if exe_only:
            deploy_exe(target)
        if config_only:
            deploy_config(target)
    else:
        deploy_all(target)
