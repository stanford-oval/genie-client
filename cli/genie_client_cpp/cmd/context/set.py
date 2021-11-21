import splatlog as logging

from genie_client_cpp.context import Context

LOG = logging.getLogger(__name__)


def add_to(subparsers):
    parser = subparsers.add_parser(
        "set",
        target=set_in_context,
        help="Set values in a context",
    )

    parser.add_argument(
        "-t",
        "--target",
    )

    parser.add_argument(
        "-n",
        "--wifi-name",
    )

    parser.add_argument(
        "-p",
        "--wifi-password",
    )

    parser.add_argument(
        "-d",
        "--dns-servers",
        action="append",
        help="DNS servers to set in resolv.conf",
    )

    parser.add_argument(
        "context",
        help="Name of context",
    )


def set_in_context(context, **values):
    for name, value in values.items():
        if value is not None:
            Context.git_config_set((context, name), value)