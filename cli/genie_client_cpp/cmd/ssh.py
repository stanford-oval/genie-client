from typing import List, Union

import splatlog as logging
from clavier import sh

from genie_client_cpp.context import Context


LOG = logging.getLogger(__name__)

def add_to(subparsers):
    parser = subparsers.add_parser(
        "ssh",
        target=run,
        help="Exec an ssh session to a device, reverse-forwarding ports",
    )

    parser.add_argument(
        "-p",
        "--port",
        dest="ports",
        action="append",
        default=[8080],
        help="Port(s) to reverse-forward",
    )

    parser.add_argument(
        "target",
        nargs="?",
        help="_destination_ argument for `ssh`",
    )

def args_for_port(port: Union[str, int]) -> List[str]:
    if isinstance(port, str) and ":" in port:
        device_port, local_port = port.split(":", 1)
        return ["-R", f"{device_port}:localhost:{local_port}"]
    return ["-R", f"{port}:localhost:{port}"]

@Context.inject_current
def run(target: str, ports: List[Union[str, int]]):
    sh.replace("ssh", *(args_for_port(port) for port in ports), target)

