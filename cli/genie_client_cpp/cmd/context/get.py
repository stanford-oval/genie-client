from clavier import sh, log as logging

from genie_client_cpp.context import Context

LOG = logging.getLogger(__name__)

def add_to(subparsers):
    parser = subparsers.add_parser(
        "get",
        target=get_context,
        help="Get values in a context",
    )

    # parser.add_argument(
    #     "-t",
    #     "--target",
    # )

    # parser.add_argument(
    #     "-n",
    #     "--wifi-name",
    # )

    # parser.add_argument(
    #     "-p",
    #     "--wifi-password",
    # )

    # parser.add_argument(
    #     "-d",
    #     "--dns-servers",
    #     help="DNS servers to set in resolv.conf"
    # )

    parser.add_argument(
        "context",
        help="Name of context",
    )

def get_context(context):
    return Context.load(context)
