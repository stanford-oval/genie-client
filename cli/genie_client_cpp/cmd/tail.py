import re
from typing import Union

from clavier import log as logging, CFG, io
from rich.table import Table

from genie_client_cpp.remote import Remote
from genie_client_cpp.context import Context


LOG = logging.getLogger(__name__)


def add_to(subparsers):
    parser = subparsers.add_parser(
        "tail",
        target=run,
        help="Tail the Genie client log",
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
def run(target: Union[str, Remote]):
    LOG.info("Tailing Genie client...", target=target)

    remote = Remote.create(target)

    remote.exec("/usr/bin/tail", "-f", "/tmp/genie.log")
