from pathlib import Path

from clavier import CFG, io

with CFG.configure("genie_client_cpp", src=__file__) as client:
    client.name = "genie-client-cpp"

    with client.configure("log") as log:
        log.level = "INFO"

    with client.configure("paths") as paths:
        paths.repo = Path(__file__).resolve().parents[2]

        with paths.configure("cli") as cli:
            cli.root = paths.repo / "cli"

        with paths.configure("out") as out:
            out.root = paths.repo / "out"
            out.assets = out.root / "assets"
            out.lib = out.root / "lib"
            out.config = out.root / "config.ini"
            out.exe = out.root / "src" / "genie"

        with paths.configure("scripts") as scripts:
            scripts.root = paths.repo / "scripts"
            scripts.launch = scripts.root / "launch.sh"
            scripts.dockerfile = scripts.root / "Dockerfile"
            scripts.asoundrc = scripts.root / "asoundrc"

    with client.configure("xiaodu") as xiaodu:
        xiaodu.network_interface = "wlan0"

        with xiaodu.configure("paths") as paths:
            paths.install = Path("/opt/genie")
            paths.assets = paths.install / "assets"
            paths.lib = paths.install / "lib"
            paths.asoundrc = paths.install / ".asoundrc"
            paths.config = paths.install / "config.ini"
            paths.launch = Path("/opt/duer/dcslaunch.sh")
            paths.exe = paths.install / "genie"

            paths.wifi_config = Path("/data/wifi/wpa_supplicant.conf")
            paths.dns_config = Path("/data/wifi/resolv.conf")

with CFG.configure(io.rel, src=__file__) as rel:
    rel.to = CFG.genie_client_cpp.paths.repo
