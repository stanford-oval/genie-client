from clavier import log as logging, CFG

from genie_client_cpp.remote import Remote
from . import kill

LOG = logging.getLogger(__name__)

def add_to(subparsers):
    parser = subparsers.add_parser(
        "remove",
        target=run,
        help="Remove the client from the target",
    )

    parser.add_argument(
        "target",
        help=(
            "_destination_ argument for `ssh` or 'adb' to use Android debugger "
            "over a USB cable"
        ),
    )


def run(target: str):
    kill.run(target)

    LOG.info("Removing client installation...", target=target)

    Remote.create(target).run(
        "rm", "-rf", str(CFG.genie_client_cpp.xiaodu.paths.install)
    )
