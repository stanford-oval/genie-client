import splatlog as logging

from genie_client_cpp.config import CONFIG
from genie_client_cpp.remote import Remote
from genie_client_cpp.context import Context


LOG = logging.getLogger(__name__)


def add_parser(subparsers):
    parser = subparsers.add_parser(
        "apply",
        target=apply,
        help=f"Apply configuration to a device",
    )

    parser.add_argument(
        "-c", "--config", default="demo", help="Config / context name"
    )

    parser.add_argument(
        "target",
        help="IP address to configure",
    )

    parser.add_argument("access_token", help="Access token to configure")


def apply(
    config: str,
    target: str,
    access_token: str,
):
    if not target.startswith("root@"):
        target = f"root@{target}"

    remote = Remote.create(target)

    config_ini_path = CONFIG.paths.repo / "config" / config / "config.ini"

    with config_ini_path.open("r", encoding="utf-8") as file:
        config_ini_contexts = file.read().replace(
            "${ACCESS_TOKEN}", access_token
        )

    remote.write(CONFIG.xiaodu.paths.config, config_ini_contexts)
