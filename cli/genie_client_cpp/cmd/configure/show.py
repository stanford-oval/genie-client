import splatlog as logging
from clavier import arg_par

from genie_client_cpp.config import CONFIG
from genie_client_cpp.cfg_par import ConfigParser

LOG = logging.getLogger(__name__)


def add_parser(subparsers: arg_par.Subparsers):
    parser = subparsers.add_parser(
        "show",
        target=show,
        help="Display config",
    )

    parser.add_argument(
        "-c", "--config", default="demo", help="Config / context name"
    )


def show(config: str):
    config_ini_path = CONFIG.paths.repo / "config" / config / "config.ini"
    config_ini = ConfigParser()
    config_ini.load(config_ini_path)
    return config_ini.to_dict()
