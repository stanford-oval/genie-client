def add_to(subparsers):
    parser = subparsers.add_parser(
        "streams",
        help="Tools for dumped audio streams (`/tmp/*.raw` files on device)",
    )

    parser.add_children(__name__, __path__)
