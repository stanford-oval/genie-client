from time import sleep

from clavier import log as logging

from genie_client_cpp.config import CONFIG
from genie_client_cpp.remote import Remote
from genie_client_cpp.context import Context
from genie_client_cpp.cmd.wifi.set import run as wifi_set
from genie_client_cpp.cmd.kill import run as kill
from genie_client_cpp.cmd.deploy import run as deploy


LOG = logging.getLogger(__name__)


def add_to(subparsers):
    parser = subparsers.add_parser(
        "configure",
        target=configure,
        help=f"Configure a device",
    )

    parser.add_argument(
        "-c", "--config", default="demo", help="Config / context name"
    )

    parser.add_argument("-w", "--wifi", default=False, help="Setup wifi first")

    parser.add_argument(
        "target",
        help="IP address to configure",
    )

    parser.add_argument("access_token", help="Access token to configure")


def configure(
    config: str,
    target: str,
    access_token: str,
    wifi: bool = False,
):
    context = Context.load(config)

    if not target.startswith("root@"):
        target = f"root@{target}"

    remote = Remote.create(target)

    if wifi:
        wifi_set(
            target=target,
            wifi_name=context.wifi_name,
            wifi_password=context.wifi_password,
            dns_servers=context.dns_servers,
            reconfigure=True,
        )

    deploy(remote, ["exe", "launch", "pulse.config"])

    config_ini_path = CONFIG.paths.repo / "config" / config / "config.ini"

    with config_ini_path.open("r", encoding="utf-8") as file:
        config_ini_contexts = file.read().replace(
            "${ACCESS_TOKEN}", access_token
        )

    remote.write(CONFIG.xiaodu.paths.config, config_ini_contexts)
