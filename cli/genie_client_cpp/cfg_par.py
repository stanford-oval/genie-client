from __future__ import annotations
import configparser
from typing import Iterable, List, Optional
from pathlib import Path

from clavier.etc.iter import flatten
from clavier.etc.path import (
    TFilename,
    is_filename,
    path_for,
    check_readable_file,
)


class ConfigParser(configparser.ConfigParser):
    DEFAULT_DEFAULT_ENCODING = "utf-8"
    DEFAULT_META_SECTION = "meta"

    default_encoding: str
    meta_section: str

    def __init__(
        self,
        *,
        default_encoding: str = DEFAULT_DEFAULT_ENCODING,
        meta_section: str = DEFAULT_META_SECTION,
        **kwds
    ):
        super().__init__(**kwds)
        self.meta_section = meta_section
        self.default_encoding = default_encoding
        self.opts = {
            "meta_section": meta_section,
            "default_encoding": default_encoding,
            **kwds,
        }

        # Our option names are case-sensitive
        #
        # SEE https://docs.python.org/3.9/library/configparser.html#customizing-parser-behaviour
        #
        self.optionxform = lambda option: option

    def to_dict(self, include_default: bool = False):
        dct = {}
        for section_name, section in self.items():
            if include_default or section_name != "DEFAULT":
                dct[section_name] = {**section}
        return dct

    def _load_base(self, base_path: TFilename) -> None:
        base = self.__class__(**self.opts)
        base.load(base_path)
        for section, options in base.items():
            if section != "DEFAULT":
                if not self.has_section(section):
                    self.add_section(section)
                for option, value in options.items():
                    if option not in self[section]:
                        self[section][option] = value

    def load(
        self,
        filename: TFilename,
        encoding: Optional[str] = None,
    ) -> List[str]:
        if encoding is None:
            encoding = self.default_encoding
        path = path_for(filename).resolve()
        check_readable_file(path)
        self.read(path, encoding)
        if self.meta_section in self:
            meta_options = self[self.meta_section]
            if "base" in meta_options:
                self._load_base(
                    # If base is not absolute, resolve it relative to the
                    # directory the declaring file is in
                    Path(path.parent, meta_options["base"]).resolve()
                )
            self.remove_section(self.meta_section)
