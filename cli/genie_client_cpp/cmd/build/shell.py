import splatlog as logging
from clavier import sh

from genie_client_cpp.config import CONFIG
from genie_client_cpp.cmd.build import DEFAULT_ARCH, TArch

LOG = logging.getLogger(__name__)

ARCH_TO_PLATFORM = {
    "arm32v7": "linux/arm/v7",
}

def add_to(subparsers):
    parser = subparsers.add_parser(
        "shell",
        target=run,
        help="Run a shell in a build container",
    )

    parser.add_argument(
        "--mount",
        choices=("repo", "out"),
        default="out",
        help="Mount either all of the repo or just the "
    )


def docker_is_podman():
    return False


def platform_args(arch: str):
    if docker_is_podman():
        return []
    if arch in ARCH_TO_PLATFORM:
        return ["--platform", ARCH_TO_PLATFORM[arch]]
    return []

def mount_args(mount: str):
    if mount == "repo":
        return [
            "--volume",
            f"{CONFIG.paths.repo}:{CONFIG.paths.container.repo}"
        ]
    if mount == "out":
        return [
            "--volume",
            f"{CONFIG.paths.out.root}:{CONFIG.paths.container.out}"
        ]
    raise ValueError(f"Expected 'repo' or 'out', got {repr(mount)}")

def run(arch: str, mount: str, **_kwds):
    sh.replace(
        "docker", "run", "--rm", "--interactive", "--tty",
        # `docker` complains if not there, podman can't take
        *platform_args(arch),
        # IDK what this does to be honest..
        "--security-opt", "label=disable",
        *mount_args(mount),
        f"{CONFIG.container.name}:{arch}"
    )
