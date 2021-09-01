from typing import Optional

from genie_client_cpp.context import Context


def add_to(subparsers):
    parser = subparsers.add_parser(
        "current",
        target=run,
        help="Get/set current context",
    )

    parser.add_argument(
        "context",
        nargs="?",
        help="Context to set as current",
    )

def run(context: Optional[str]):
    if context is None:
        return Context.get_current_name()
    else:
        return Context.set_current_name(context)