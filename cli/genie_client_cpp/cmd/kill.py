import re

from clavier import log as logging, CFG

from genie_client_cpp.remote import Remote
from genie_client_cpp.context import Context
from genie_client_cpp.ps_entry import PSEntry


LOG = logging.getLogger(__name__)

COMMAND_PATTERNS = (
    re.compile(r".*\s/opt/duer/dcslaunch.sh$"),
    re.compile(r".*/genie$"),
    re.compile(r"^/tmp/spotifyd\s.*"),
    re.compile(r".*/pulseaudio\s.*"),
)

def add_to(subparsers):
    parser = subparsers.add_parser(
        "kill",
        target=run,
        help="Kill client on target",
    )

    parser.add_argument(
        "target",
        nargs="?",
        help=(
            "_destination_ argument for `ssh` or 'adb' to use Android debugger "
            "over a USB cable"
        ),
    )

def kill_process(remote: Remote, pattern: re.Pattern):
    kills = 0
    for line in remote.get("ps").splitlines()[1:]:
        ps_entry = PSEntry.from_line(line)
        if pattern.fullmatch(ps_entry.command):
            LOG.info(
                "Killing process",
                ps_entry=ps_entry,
            )
            remote.run("kill", "-9", ps_entry.pid)
            kills += 1
    if kills == 0:
        LOG.info(f"Killed {kills} processes for {pattern.pattern}")
    else:
        LOG.info(f"No processes found for {pattern.pattern}")

@Context.inject_current
def run(target: str):
    LOG.info("Killing client...", target=target)

    remote = Remote.create(target)

    for pattern in COMMAND_PATTERNS:
        kill_process(remote, pattern)

