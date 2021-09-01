def add_to(subparsers):
    parser = subparsers.add_parser(
        "context",
        help="Get and set context values (in repo's Git config)",
    )

    parser.add_children(__name__, __path__)
