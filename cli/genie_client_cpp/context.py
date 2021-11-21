from __future__ import annotations
from dataclasses import dataclass, fields
from typing import Callable, Iterable, List, Optional, Union
from subprocess import CalledProcessError
from functools import wraps

import splatlog as logging
from clavier import sh

LOG = logging.getLogger(__name__)


@dataclass
class Context:
    GIT_CONFIG_PREFIX = "context"
    LIST_SEP = ";"

    TValue = Union[None, str, List[str]]

    name: str
    target: Optional[str]
    wifi_name: Optional[str]
    wifi_password: Optional[str]
    dns_servers: Optional[List[str]]

    @classmethod
    def git_config_name(cls, key_path: Iterable[str]) -> str:
        return ".".join(
            (
                cls.GIT_CONFIG_PREFIX,
                *(key.replace("_", "-") for key in key_path),
            )
        )

    @classmethod
    def git_config_get(
        cls, key_path: Union[str, Iterable[str]], *, type="str"
    ) -> str:
        if isinstance(key_path, str):
            key_path = [key_path]
        try:
            value = sh.get(
                "git",
                "config",
                "--local",
                cls.git_config_name(key_path),
                format="strip",
            )
        except CalledProcessError as error:
            if error.returncode == 1 and error.stdout == "":
                return None
            raise error
        if type == "Optional[List[str]]":
            return value.split(";")
        return value

    @classmethod
    def git_config_set(
        cls, key_path: Union[str, Iterable[str]], value: TValue
    ) -> None:
        if isinstance(key_path, str):
            key_path = [key_path]
        if value is None:
            sh.run("git", "config", "--unset", cls.git_config_name(key_path))
        else:
            sh.run(
                "git",
                "config",
                "--local",
                cls.git_config_name(key_path),
                cls.encode(value),
            )

    @classmethod
    def encode(cls, value: TValue) -> str:
        if value is None or isinstance(value, str):
            return value
        if isinstance(value, (tuple, list)):
            for item in value:
                if not isinstance(item, str):
                    raise TypeError(
                        "only lists/tuples of str accepted, "
                        f"given {repr(value)}"
                    )
                if cls.LIST_SEP in item:
                    raise ValueError(
                        "list/tuple items may not contain "
                        f"{repr(cls.LIST_SEP)}, given {repr(value)}"
                    )
            return cls.LIST_SEP.join(value)
        raise TypeError(f"can't encode type {type(value)}: {repr(value)}")

    @classmethod
    def list(cls):
        config_names = sh.get(
            "git",
            "config",
            "--local",
            "--list",
        ).splitlines()

        return sorted(
            {
                parts[1]
                for parts in (name.split(".") for name in config_names)
                if parts[0] == cls.GIT_CONFIG_PREFIX and len(parts) > 2
            }
        )

    @classmethod
    def get_current_name(cls):
        return cls.git_config_get("current")

    @classmethod
    def set_current_name(cls, name: str) -> None:
        cls.git_config_set("current", name)

    @classmethod
    def load(cls, name: str) -> Context:
        return cls(
            name=name,
            **{
                field.name: cls.git_config_get(
                    (name, field.name), type=field.type
                )
                for field in fields(cls)
                if field.name != "name"
            },
        )

    @classmethod
    def current(cls) -> Optional[Context]:
        if name := cls.get_current_name():
            return cls.load(name)
        return None

    @classmethod
    def inject_current(cls, fn: Callable) -> Callable:
        @wraps(fn)
        def wrapped(*args, **kwds):
            field_names = {field.name for field in fields(cls)}
            if current := cls.current():
                for name, value in kwds.items():
                    if name in field_names and value is None:
                        context_value = getattr(current, name)
                        kwds[name] = context_value
            return fn(*args, **kwds)

        return wrapped
