from clavier import dyn

def add_to(subparsers):
    parser = subparsers.add_parser(
        "src",
        help="Commands for source files",
    )

    parser.add_children(__name__, __path__)
