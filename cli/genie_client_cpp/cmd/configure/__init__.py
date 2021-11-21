from clavier import arg_par


def add_parser(subparsers: arg_par.Subparsers):
    parser = subparsers.add_parser(
        "configure",
        aliases=["cfg"],
        help="Configure a device",
    )
    parser.add_children(__name__, __path__)
