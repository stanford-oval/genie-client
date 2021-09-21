from __future__ import annotations
import re
from dataclasses import dataclass

@dataclass
class PSEntry:
    LINE_RE = re.compile(r"\s*(\d+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(.*)")

    pid: int
    user: str
    virtual_memory: str
    process_state: str
    command: str

    @classmethod
    def from_line(cls, line: str) -> PSEntry:
        match = cls.LINE_RE.fullmatch(line)
        if match is None:
            raise ValueError(f"Failed to parse `ps` line: {repr(line)}")

        pid, user, virtual_memory, process_state, command = \
            match.groups()

        return cls(
            pid=int(pid),
            user=user,
            virtual_memory=virtual_memory,
            process_state=process_state,
            command=command,
        )
