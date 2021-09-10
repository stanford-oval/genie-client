from typing import Optional
import re

from clavier import CFG

from genie_client_cpp.context import Context
from genie_client_cpp.remote import Remote

def add_to(subparsers):
    parser = subparsers.add_parser(
        "fetch",
        target=fetch,
        help="Fetch audio streams to `@/tmp/raw`."
    )

    parser.add_argument(
        "-t",
        "--target",
        help=(
            "_destination_ argument for `ssh` or 'adb' to use Android debugger "
            "over a USB cable"
        ),
    )

@Context.inject_current
def fetch(target: Optional[str]):
    remote = Remote.create(target)
    base_dir = CFG.genie_client_cpp.paths.tmp / "raw"
    base_dir.mkdir(exist_ok=True)
    existing = [
        entry.name
        for entry
        in base_dir.iterdir()
        if entry.is_dir() and re.fullmatch(r'\d{2}', entry.name)
    ]

    if len(existing) == 0:
        fetch_number = 1
    else:
        fetch_number = max(int(name) for name in existing) + 1

    if fetch_number > 99:
        raise Exception("Too many dirs")

    dir_name = f"{fetch_number:02}"
    dir = base_dir / dir_name
    dir.mkdir()
    for base_name in ("input", "playback", "filter"):
        remote.pull(f"/tmp/{base_name}.raw", dir)
