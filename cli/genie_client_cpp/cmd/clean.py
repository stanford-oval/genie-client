from typing import Literal
from argparse import BooleanOptionalAction
import shutil

from clavier import log as logging, sh

from genie_client_cpp.config import CONFIG

LOG = logging.getLogger(__name__)


def add_to(subparsers):
    parser = subparsers.add_parser(
        "clean",
        target=clean,
        help="Remove build artifacts (delete output directory)",
    )


def clean():
    out_dir = CONFIG.paths.out.root
    if out_dir.exists():
        LOG.info("Removing output directory...", path=out_dir)
        shutil.rmtree(out_dir)
    else:
        LOG.info("Output directory does not exist, skipping.")
