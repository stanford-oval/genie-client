import splatlog as logging
from clavier import sh

from genie_client_cpp.config import CONFIG
from genie_client_cpp.remote import Remote
from genie_client_cpp.context import Context

LOG = logging.getLogger(__name__)


def add_to(subparsers):
    parser = subparsers.add_parser(
        "record",
        target=run,
        help="Record stuff",
    )

    parser.add_argument(
        "-t",
        "--target",
        help=(
            "_destination_ argument for `ssh` or 'adb' to use Android debugger "
            "over a USB cable"
        ),
    )


@Context.inject_current
def run(target):
    remote = Remote.create(target)
    dest = CONFIG.xiaodu.paths.tmp.root / "echosrc.wav"

    source_profile_cmd = sh.join("source", CONFIG.xiaodu.paths.profile)
    record_cmd = sh.join(
        CONFIG.xiaodu.paths.pulse.parec,
        {
            "format": "s16le",
            "device": "echosrc",
            "file-format": "wav",
        },
        dest,
        opts_style="=",
    )

    composite_cmd = f"{source_profile_cmd} && {record_cmd}"

    LOG.info("Spawning on remote target...", cmd=composite_cmd)

    popen = remote.spawn(composite_cmd)

    input("Recoding, press enter to stop...")
    popen.kill()
    remote.pull(dest, CONFIG.paths.tmp.root / dest.name)
    remote.rm(dest)
    LOG.info("Done.")
