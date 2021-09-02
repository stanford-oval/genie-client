from typing import Optional

from clavier import err

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

    parser.add_argument(
        "-l",
        "--list",
        action="store_true",
        default=False,
        help="List current context values",
    )

def run(context: Optional[str], list: bool) -> Optional[str]:
    if context is None:
        return Context.get_current_name()
    else:
        if list is True:
            raise err.UserError(
                "List and set operations are in conflict"
            )
        return Context.set_current_name(context)