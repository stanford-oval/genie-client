from argparse import BooleanOptionalAction

from rich.panel import Panel
from rich.rule import Rule
from clavier import CFG, log as logging, io

from genie_client_cpp.remote import Remote


LOG = logging.getLogger(__name__)


def add_to(subparsers):
    parser = subparsers.add_parser(
        "get",
        target=run,
        help="Get WiFi configuration",
    )

    parser.add_argument(
        "target",
        help=(
            "_destination_ argument for `ssh` or 'adb' to use Android debugger "
            "over a USB cable"
        ),
    )

class View(io.View):
    def render_rich(self):
        self.print(
            *io.header(str(CFG.genie_client_cpp.xiaodu.paths.wifi_config)),
            self.data["config"],
            io.NEWLINE,
            *io.header(CFG.genie_client_cpp.xiaodu.network_interface),
            self.data["interface"],
        )

def run(target: str) -> View:
    remote = Remote.create(target)
    config = remote.read(CFG.genie_client_cpp.xiaodu.paths.wifi_config)
    interface = remote.get(
        "ip", "addr", "show", CFG.genie_client_cpp.xiaodu.network_interface
    )
    return View({"config": config, "interface": interface})

