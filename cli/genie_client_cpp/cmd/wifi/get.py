from typing import Optional
from genie_client_cpp.context import Context

import splatlog as logging
from clavier import io

from genie_client_cpp.remote import Remote
from genie_client_cpp.config import CONFIG


LOG = logging.getLogger(__name__)


def add_to(subparsers):
    parser = subparsers.add_parser(
        "get",
        target=run,
        help="Get WiFi configuration",
    )

    parser.add_argument(
        "-t",
        "--target",
        help=(
            "_destination_ argument for `ssh` or 'adb' to use Android debugger "
            "over a USB cable"
        ),
    )


class View(io.View):
    def render_rich(self):
        self.print(
            *io.header(str(CONFIG.xiaodu.paths.wifi_config)),
            self.data["config"],
            io.NEWLINE,
            *io.header(CONFIG.xiaodu.network_interface),
            self.data["interface"],
            *io.header(str(CONFIG.xiaodu.paths.dns_config)),
            self.data["dns"]
        )


@Context.inject_current
def run(target: Optional[str]) -> View:
    remote = Remote.create(target)
    config = remote.read(CONFIG.xiaodu.paths.wifi_config)
    interface = remote.get(
        "ip", "addr", "show", CONFIG.xiaodu.network_interface
    )
    dns = remote.read(CONFIG.xiaodu.paths.dns_config)
    return View({"config": config, "interface": interface, "dns": dns})
