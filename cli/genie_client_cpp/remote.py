from __future__ import annotations
from abc import ABC, abstractmethod
from pathlib import Path
from tempfile import mkstemp
from typing import Any, List, Literal, Optional, Union
import os
from subprocess import DEVNULL, PIPE, Popen
import shlex

import splatlog as logging
from clavier import sh


LOG = logging.getLogger(__name__)
DEFAULT_ENCODING = "utf-8"


class Remote(ABC):
    """\
    Abstract base class for interacting with remote devices, with concrete
    implementations for each of the supported access methods (`adb` and `ssh`
    as of 2021-08-30).
    """

    @classmethod
    def create(cls, target: Union[str, Remote]):
        if isinstance(target, cls):
            return target
        if not isinstance(target, str):
            raise TypeError(
                f"Expected str, given {type(target)}: {repr(target)}"
            )
        if target == "adb":
            return ADBRemote()
        else:
            return SSHRemote(target)

    @abstractmethod
    def write(
        self,
        dest: Union[str, Path],
        contents: str,
        *,
        encoding: Optional[str] = DEFAULT_ENCODING,
    ):
        pass

    @abstractmethod
    def run(self, *args, log=LOG, **opts) -> None:
        pass

    @abstractmethod
    def get(self, *args, log=LOG, **opts) -> Any:
        pass

    @abstractmethod
    def push(self, src: Union[str, Path], dest: Union[str, Path]):
        pass

    def read(self, path: Union[str, Path]):
        return self.get("cat", str(path))

    def write_lines(
        self,
        dest: Union[str, Path],
        *lines: str,
        encoding: Optional[str] = DEFAULT_ENCODING,
    ):
        return self.write(
            dest,
            "\n".join(lines) + "\n",
            encoding=encoding,
        )


class ADBRemote(Remote):
    def write(
        self,
        dest: Union[str, Path],
        contents: str,
        *,
        encoding: Optional[str] = DEFAULT_ENCODING,
    ):
        handle, path = mkstemp(text=True)
        try:
            with os.fdopen(handle, "w", encoding=encoding) as file:
                file.write(contents)
            sh.run("adb", "push", path, str(dest))
            LOG.info(
                "Wrote contents to remote",
                dest=dest,
                encoding=encoding,
                contents=contents,
            )
        finally:
            os.remove(path)

    def get(self, *args, log=LOG, **opts) -> Any:
        return sh.get("adb", "shell", *args, log=log, **opts)

    def run(
        self,
        *args,
        log=LOG,
        chdir: Union[None, Path, str] = None,
        **opts,
    ) -> None:
        if chdir is not None:
            raise NotImplementedError(
                "Can not set directory for adb remote command"
            )

        sh.run("adb", "shell", *args, log=log, **opts)

    def push(self, src: Path, dest: Path):
        sh.run("adb", "push", src, dest)


class SSHRemote(Remote):
    def __init__(self, target: str):
        self._target = target

    def get(self, *args, log=LOG, **opts) -> Any:
        return sh.get("ssh", self._target, *args, log=log, **opts)

    def run(
        self,
        *args,
        log=LOG,
        chdir: Union[None, Path, str] = None,
        **opts,
    ):
        if chdir is not None:
            raise NotImplementedError(
                "Can not set directory for ssh remote command"
            )

        return sh.run("ssh", self._target, *args, log=log, **opts)

    def write(
        self,
        dest: Union[str, Path],
        contents: str,
        *,
        encoding: Optional[str] = DEFAULT_ENCODING,
    ):
        handle, path = mkstemp(text=True)
        try:
            with os.fdopen(handle, "w", encoding=encoding) as file:
                file.write(contents)
            sh.run("scp", path, f"{self._target}:{dest}")
            LOG.info(
                "Wrote contents to remote",
                dest=dest,
                encoding=encoding,
                contents=contents,
            )
        finally:
            os.remove(path)

    def is_file(self, path: Union[str, Path]) -> bool:
        return sh.test(
            "ssh", self._target, f"[[ -f {shlex.quote(str(path))} ]]"
        )

    def is_dir(self, path: Union[str, Path]) -> bool:
        return sh.test(
            "ssh", self._target, f"[[ -d {shlex.quote(str(path))} ]]"
        )

    def rm(self, path: Union[str, Path]) -> sh.Result:
        return self.run("rm", "-rf", path)

    def push(self, src: Union[str, Path], dest: Union[str, Path]):
        if isinstance(src, str):
            src = Path(str)
        if isinstance(dest, str):
            dest = Path(dest)

        LOG.info("Pushing...", src=src, dest=dest)

        if not self.is_dir(dest.parent):
            LOG.info("Creating destination parent directory...")
            self.run("mkdir", "-p", dest.parent)

        if src.is_dir():
            if self.is_dir(dest):
                LOG.info(
                    "Removing destination in order to push directory...",
                    dest=dest,
                )
                self.rm(dest)

        sh.run(
            "scp",
            {"r": src.is_dir()},
            src,
            f"{self._target}:{dest}",
            stdout=DEVNULL,
        )

    def pull(self, src: Union[str, Path], dest: Union[str, Path]):
        if isinstance(src, str):
            src = Path(str)
        if isinstance(dest, str):
            dest = Path(dest)

        LOG.info("Pulling...", src=src, dest=dest)

        if not dest.parent.is_dir():
            LOG.info("Creating destination parent directory...")
            dest.parent.mkdir(parents=True)

        sh.run(
            "scp",
            {"r": self.is_dir(src)},
            f"{self._target}:{src}",
            dest,
            stdout=DEVNULL,
        )

    def spawn(self, *cmd: str) -> Popen:
        return Popen(
            ["ssh", self._target, *cmd],
            encoding="utf-8",
            stdout=PIPE,
            stderr=PIPE,
        )

    def exec(self, *cmd: str, **opts):
        return sh.run("ssh", self._target, *cmd, **opts)
