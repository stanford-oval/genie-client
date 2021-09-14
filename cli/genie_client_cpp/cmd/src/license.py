from pathlib import Path
from typing import List
from clavier import CFG, log as logging, io


# Constants
# ============================================================================

LOG = logging.getLogger(__name__)

MARKER = "// This file is part of Genie"

COMMON_HEADER = """\
// This file is part of Genie
//
// Copyright 2021 The Board of Trustees of the Leland Stanford Junior University
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
"""

CPP_PREFIX = """\
// -*- mode: cpp; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
"""

# Argument Parser
# ============================================================================

def add_to(subparsers):
    parser = subparsers.add_parser(
        "license",
        target=run,
        help="Add license header to source files that don't have it",
    )

    parser.add_argument(
        "-m",
        "--missing",
        action="store_true",
        default=False,
        help="Show file paths that are missing the license",
    )

def run(*, missing: bool = False):
    if missing:
        return missing_paths()
    return add_missing()

# Functionality
# ============================================================================

def has_license(path: Path) -> bool:
    return MARKER in path.read_text(encoding="utf-8").splitlines()

def missing_paths() -> List[Path]:
    return [
        path
        for path
        in CFG.genie_client_cpp.paths.src.root.glob("**/*.[hc]pp")
        if not has_license(path)
    ]

def has_missing() -> bool:
    return len(missing_paths()) > 0

def add_missing() -> List[Path]:
    missing = missing_paths()
    for path in missing:
        add_license_to(path)
    return missing

def is_cpp_path(path: Path) -> bool:
    return path.suffix in (".hpp", ".cpp")

def add_license_to(path: Path):
    new_content = f"{COMMON_HEADER}\n{path.read_text(encoding='utf-8')}"
    if is_cpp_path(path):
        new_content = CPP_PREFIX + new_content
    path.write_text(new_content, encoding="utf-8")
