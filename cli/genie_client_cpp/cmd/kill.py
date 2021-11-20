import re
from typing import Union

from clavier import log as logging, CFG, io
from rich.table import Table

from genie_client_cpp.remote import Remote
from genie_client_cpp.context import Context
from genie_client_cpp.ps_entry import PSEntry


LOG = logging.getLogger(__name__)

COMMAND_PATTERNS = {
    "dcslaunch.sh": re.compile(r".*\s/opt/duer/dcslaunch.sh$"),
    "genie-client": re.compile(r".*/genie-client$"),
    "spotifyd": re.compile(r"^/tmp/spotifyd\s.*"),
    "pulseaudio": re.compile(r".*/pulseaudio\s.*"),
}


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


def kill_process(remote: Remote, pattern: re.Pattern) -> int:
    kills = 0
    for line in remote.get("ps").splitlines()[1:]:
        ps_entry = PSEntry.from_line(line)
        if pattern.fullmatch(ps_entry.command):
            LOG.debug(
                "Killing process",
                ps_entry=ps_entry,
            )
            remote.run("kill", "-9", ps_entry.pid)
            kills += 1
    if kills == 0:
        LOG.debug(f"Killed {kills} processes for {pattern.pattern}")
    else:
        LOG.debug(f"No processes found for {pattern.pattern}")
    return kills


@Context.inject_current
def run(target: Union[str, Remote]):
    LOG.info("Killing client...", target=target)

    remote = Remote.create(target)

    return _View(
        [
            [process, pattern, kill_process(remote, pattern)]
            for process, pattern in COMMAND_PATTERNS.items()
        ]
    )


class _View(io.View):
    def render_rich(self):
        table = Table(title="Killed Processes")
        table.add_column("Process")
        table.add_column("Pattern")
        table.add_column("Kills")
        for process, pattern, kills in self.data:
            table.add_row(process, pattern.pattern, str(kills))
        self.print(table)
