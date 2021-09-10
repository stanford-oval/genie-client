from __future__ import annotations
from abc import ABC, abstractmethod
from pathlib import Path
from tempfile import mkstemp
from typing import Any, List, Literal, Optional, Union
import os
from subprocess import DEVNULL

from clavier import sh, log as logging


LOG = logging.getLogger(__name__)
DEFAULT_ENCODING = "utf-8"


class Remote(ABC):
    """\
    Abstract base class for interacting with remote devices, with concrete
    implementations for each of the supported access methods (`adb` and `ssh`
    as of 2021-08-30).
    """

    @classmethod
    def create(cls, target: str):
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

    @abstractmethod
    def pull(self, src: Union[str, Path], dest: Union[str, Path]):
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

    def push(self, src: Union[str, Path], dest: Union[str, Path]):
        sh.run("adb", "push", src, dest)

    def pull(self, src: Union[str, Path], dest: Union[str, Path]):
        sh.run("adb", "pull", src, dest)


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
    ) -> None:
        if chdir is not None:
            raise NotImplementedError(
                "Can not set directory for ssh remote command"
            )

        sh.run("ssh", self._target, *args, log=log, **opts)

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

    def push(self, src: Union[str, Path], dest: Union[str, Path]):
        if isinstance(src, str):
            src = Path(src)
        if isinstance(dest, str):
            dest = Path(dest)

        LOG.info("Pushing...", src=src, dest=dest)

        sh.run(
            "scp",
            {"r": src.is_dir()},
            src,
            f"{self._target}:{dest}",
            stdout=DEVNULL,
        )

    def pull(self, src: Union[str, Path], dest: Union[str, Path]):
        if isinstance(src, str):
            src = Path(src)
        if isinstance(dest, str):
            dest = Path(dest)

        LOG.info("Pulling...", src=src, dest=dest)

        sh.run(
            "scp",
            {"r": src.is_dir()},
            f"{self._target}:{src}",
            dest,
            stdout=DEVNULL,
        )