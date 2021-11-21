from genie_client_cpp.context import Context


def add_to(subparsers):
    parser = subparsers.add_parser(
        "context",
        aliases=["ctx"],
        target=run,
        help="List available contexts (in repo's Git config)",
    )

    parser.add_children(__name__, __path__)


def run(list: bool = False):
    return Context.list()
