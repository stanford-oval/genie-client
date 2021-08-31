def add_to(subparsers):
    parser = subparsers.add_parser(
        "wifi",
        help="Get and set WiFi configuration",
    )

    parser.add_children(__name__, __path__)
