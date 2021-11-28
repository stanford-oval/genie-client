from typing import Iterable
from shlex import quote as q

from clavier import CFG, log as logging, sh

from genie_client_cpp.config import CONFIG
from genie_client_cpp.remote import Remote, TTarget
from genie_client_cpp.context import Context

LOG = logging.getLogger(__name__)


def add_to(subparsers):
    parser = subparsers.add_parser(
        "tools",
        target=deploy_tools,
        help="Copy a tool from the build container to the device",
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
        "--device-type",
        default=CONFIG.device.default_type,
    )

    parser.add_argument(
        "--tmp",
        action="store_true",
        default=False,
        help="Copy to a temporary location that will disappear on reboot",
    )

    parser.add_argument(
        "names",
        nargs="+",
        help="Tool names to deploy. Uses `which NAME` in container to find them",
    )


@Context.inject_current
def deploy_tools(
    target: TTarget,
    device_type: str,
    names: Iterable[str],
    *,
    tmp: bool = False,
):
    remote = Remote.create(target)
    device_config = CONFIG[device_type]
    dest_dir = (
        device_config.paths.tmp.bin if tmp else device_config.paths.tools.bin
    )

    for name in names:
        local_path = CONFIG.paths.out.tools / name

        if not local_path.exists():
            container_out_path = CONFIG.paths.container.out / name

            tag = f"genie-builder:{device_config.arch}"

            sh.run(
                "docker",
                "run",
                {
                    "rm": True,
                    "volume": f"{CONFIG.paths.out.root}:{CONFIG.paths.container.out}",
                    "security-opt": "label=disable",
                    "env": f"ARCH={device_config.arch}",
                },
                tag,
                "bash",
                "-c",
                f"""cp "$(which {q(name)})" {q(str(container_out_path))}""",
                cwd=CONFIG.paths.repo,
                log=LOG,
                rel_paths=True,
                opts_style=" ",
            )

        remote.push(local_path, dest_dir / name)
