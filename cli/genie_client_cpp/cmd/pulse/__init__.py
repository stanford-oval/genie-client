def add_to(subparsers):
    parser = subparsers.add_parser(
        "pulse",
        help="PulseAudio tools",
    )

    parser.add_children(__name__, __path__)
