from argparse import BooleanOptionalAction
from typing import List

from clavier import CFG, log as logging

from genie_client_cpp.remote import Remote
from genie_client_cpp.context import Context


LOG = logging.getLogger(__name__)


def add_to(subparsers):
    parser = subparsers.add_parser(
        "set",
        target=run,
        help="Set WiFi configuration",
    )

    parser.add_argument(
        "-n",
        "--wifi-network",
        help="Network name (SSID)",
    )

    parser.add_argument(
        "-p",
        "--wifi-password",
        help="Network password (pre-shared key)",
    )

    parser.add_argument(
        "-d",
        "--dns-servers",
        action="append",
        help="DNS servers (can repeat option)",
    )

    parser.add_argument(
        "--reconfigure",
        action=BooleanOptionalAction,
        default=True,
        help="Reconfigure the interface after set",
    )

    parser.add_argument(
        "target",
        nargs="?",
        help=(
            "_destination_ argument for `ssh` or 'adb' to use Android debugger "
            "over a USB cable"
        ),
    )


@Context.inject_current
def run(
    target: str,
    wifi_network: str,
    wifi_password: str,
    dns_servers: List[str],
    reconfigure: bool,
):
    remote = Remote.create(target)
    if wifi_network is not None and wifi_password is not None:
        remote.write_lines(
            CFG.genie_client_cpp.xiaodu.paths.wifi_config,
            "ctrl_interface=/var/run/wpa_supplicant",
            "update_config=1",
            "network={",
            f'    ssid="{wifi_network}"',
            f'    psk="{wifi_password}"',
            "}",
        )

    if dns_servers:
        remote.write_lines(
            CFG.genie_client_cpp.xiaodu.paths.dns_config,
            *(f"nameserver {server}" for server in dns_servers)
        )

    if reconfigure:
        remote.run("wpa_cli", "reconfigure")
