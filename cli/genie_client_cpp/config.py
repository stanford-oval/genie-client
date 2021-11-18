from pathlib import Path

from clavier import CFG, io

with CFG.configure("genie_client_cpp", src=__file__) as client:
    client.name = "genie-client-cpp"

    with client.configure("log") as log:
        log.level = "INFO"

    with client.configure("paths") as paths:
        paths.repo = Path(__file__).resolve().parents[2]

        with paths.configure("tmp") as tmp:
            tmp.root = paths.repo / "tmp"

        with paths.configure("cli") as cli:
            cli.root = paths.repo / "cli"

        with paths.configure("out") as out:
            out.root = paths.repo / "out"
            out.assets = out.root / "assets"
            out.lib = out.root / "lib"
            out.config = out.root / "config.ini"
            out.exe = out.root / "genie-client"
            out.pulseaudio = out.root / "pulseaudio"

            with out.configure("tools") as tools:
                tools.root = out.root / "tools"
                tools.gdbserver = tools.root / "gdbserver"
                tools.pactl = tools.root / "pactl"
                tools.parec = tools.root / "parec"
                tools.pacmd = tools.root / "pacmd"
                tools.pacat = tools.root / "pacat"

        with paths.configure("scripts") as scripts:
            scripts.root = paths.repo / "scripts"
            scripts.launch = scripts.root / "launch.sh"
            scripts.asoundrc = scripts.root / "asoundrc"
            scripts.dockerfile = scripts.root / "Dockerfile"
            scripts.pulseaudio_config = scripts.root / "pulse-system.pa"
            scripts.profile = scripts.root / "profile"

        with paths.configure("src") as src:
            src.root = paths.repo / "src"

        with paths.configure("container") as container:
            # When you mount the repo into the build container, where it goes
            container.repo = Path("/src")
            # When you mount the out directory into the build container, where
            # it goes
            container.out = Path("/out")

            with container.configure("pulse") as pulse:
                # Where the PulseAudio tools are installed in the build container
                pulse.bin = Path("/usr/local/bin")
                pulse.pactl = pulse.bin / "pactl"
                pulse.parec = pulse.bin / "parec"
                pulse.pacmd = pulse.bin / "pacmd"
                pulse.pacat = pulse.bin / "pacat"

    with client.configure("container") as container:
        container.name = "genie-builder"

    with client.configure("xiaodu") as xiaodu:
        xiaodu.network_interface = "wlan0"

        with xiaodu.configure("paths") as paths:
            paths.install = Path("/opt/genie")
            paths.assets = paths.install / "assets"
            paths.lib = paths.install / "lib"
            paths.asoundrc = paths.install / ".asoundrc"
            paths.config = paths.install / "config.ini"
            paths.launch = Path("/opt/duer/dcslaunch.sh")
            paths.exe = paths.install / "genie-client"
            paths.pulseaudio = paths.install / "pulseaudio"
            paths.pulseaudio_config = paths.install / ".system.pa"
            paths.profile = paths.install / ".profile"

            paths.wifi_config = Path("/data/wifi/wpa_supplicant.conf")
            paths.dns_config = Path("/data/wifi/resolv.conf")

            with paths.configure("tmp") as tmp:
                tmp.root = Path("/tmp")
                tmp.bin = tmp.root / "bin"

            paths.gdbserver = paths.tmp.bin / "gdbserver"

            with paths.configure("pulse") as pulse:
                pulse.exe = paths.install / "pulseaudio"
                pulse.config = paths.install / ".system.pa"
                pulse.pactl = paths.tmp.bin / "pactl"
                pulse.parec = paths.tmp.bin / "parec"
                pulse.pacmd = paths.tmp.bin / "pacmd"
                pulse.pacat = paths.tmp.bin / "pacat"

        xiaodu.deployables = {
            "lib": {"src": client.paths.out.lib, "dest": paths.lib},
            "assets": {"src": client.paths.out.assets, "dest": paths.assets},
            "launch": {
                "src": client.paths.scripts.launch,
                "dest": paths.launch,
            },
            "config": {"src": client.paths.out.config, "dest": paths.config},
            "exe": {"src": client.paths.out.exe, "dest": paths.exe},
            "alsa.pipe-config": {
                "src": client.paths.scripts.asoundrc,
                "dest": paths.asoundrc,
            },
            "pulse.exe": {
                "src": client.paths.out.pulseaudio,
                "dest": paths.pulse.exe,
            },
            "pulse.config": {
                "src": client.paths.scripts.pulseaudio_config,
                "dest": paths.pulse.config,
            },
            "pulse.tools.pactl": {
                "src": client.paths.out.tools.pactl,
                "dest": paths.pulse.pactl,
            },
            "pulse.tools.parec": {
                "src": client.paths.out.tools.parec,
                "dest": paths.pulse.parec,
            },
            "pulse.tools.pacmd": {
                "src": client.paths.out.tools.pacmd,
                "dest": paths.pulse.pacmd,
            },
            "pulse.tools.pacat": {
                "src": client.paths.out.tools.pacat,
                "dest": paths.pulse.pacat,
            },
            "profile": {
                "src": client.paths.scripts.profile,
                "dest": paths.profile,
            },
        }

with CFG.configure(io.rel, src=__file__) as rel:
    rel.to = CFG.genie_client_cpp.paths.repo

CONFIG = CFG.genie_client_cpp
