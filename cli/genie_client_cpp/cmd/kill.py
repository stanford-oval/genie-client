import re

from clavier import log as logging, CFG

from genie_client_cpp.remote import Remote
from genie_client_cpp.ps_entry import PSEntry


LOG = logging.getLogger(__name__)

COMMANDS_TO_KILL = (
    re.compile(r".*\s/opt/duer/dcslaunch.sh$"),
    re.compile(r".*/genie$"),
    re.compile(r"^/tmp/spotifyd\s.*")
)

def add_to(subparsers):
    parser = subparsers.add_parser(
        "kill",
        target=run,
        help="Kill client on target",
    )

    parser.add_argument(
        "target",
        help=(
            "_destination_ argument for `ssh` or 'adb' to use Android debugger "
            "over a USB cable"
        ),
    )

def run(target: str):
    LOG.info("Killing client...", target=target)

    remote = Remote.create(target)

    for line in remote.get("ps").splitlines()[1:]:
        ps_entry = PSEntry.from_line(line)
        for exp in COMMANDS_TO_KILL:
            if exp.fullmatch(ps_entry.command):
                LOG.info(
                    "Killing process",
                    ps_entry=ps_entry,
                )
                remote.run("kill", ps_entry.pid)

