from typing import Optional
import re

from clavier import CFG, sh

from genie_client_cpp.context import Context
from genie_client_cpp.remote import Remote

def add_to(subparsers):
    parser = subparsers.add_parser(
        "play",
        target=play,
        help="Play captured raw audio streams."
    )

    parser.add_argument(
        "file",
        help="File to play",
    )

def play(file: str):
    sh.run(
        "ffplay",
        "-f", "s16le",
        "-ar", "16k",
        "-ac", 1,
        file
    )
