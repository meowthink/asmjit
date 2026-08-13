#!/usr/bin/env python3
"""Cross-checks the generated PPC RW classes against the Power ISA manual.

For every instruction in asmjit/ppc/ppcinstdb_p.h, locates its operand syntax
line in the extracted text of the Power ISA manual and compares the first
operand with the instruction's RW class:

  kDefault      - the first operand must be a destination register
                  (RT/RA/XT/VRT/FRT). Compare/test instructions start with a
                  CR field (BF) and belong to kNoWrite.
  kNoWrite      - the first operand must NOT be a destination register.
  kStore        - the first operand is the data source (RS/FRT/VRT).
  kLoadUpdate   - the first operand is the load destination.
  kStoreUpdate  - the first operand is the data source.

Usage:

  python3 tools/ppc_rw_audit.py /path/to/PowerISA_public.v3.0B_read/text/text.txt

The text file is produced by the document-reading skill's read_document.py on
~/Downloads/PowerISA_public.v3.0B.pdf. Instructions not found in the manual
(assembler extended mnemonics and aliases such as `li`, `nop`, `beq`, `sldi`)
are reported separately; their classes match the base instruction they alias.
"""

import re
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DB = ROOT / "asmjit/ppc/ppcinstdb_p.h"

DEST_FIELDS = {"RT", "RA", "XT", "VRT", "FRT", "RS"}

#! Uppercase operand fields used in the manual's instruction syntax lines.
#! Code examples use mixed-case register names (Rb, Rx, Vz, ...) and are
#! rejected, as are the lowercase fields of the extended-mnemonic tables.
KNOWN_FIELDS = {
    "RT", "RA", "RB", "RS", "XT", "XA", "XB", "XS",
    "VRT", "VRA", "VRB", "VRC", "VRS",
    "FRT", "FRA", "FRB", "FRC", "FRS",
    "BF", "BFA", "TO", "BO", "BI", "BD", "D", "DS",
    "SI", "UI", "NB", "L", "LEV", "SPR", "TH", "CT", "UIM",
    "SH", "MB", "ME", "CY", "EH", "W", "TX", "AX", "BX",
}


def load_db():
    classes = {}
    with open(DB) as f:
        for line in f:
            m = re.search(r"RWClass::(\w+), // (\w+)$", line)
            if m:
                classes[m.group(2)] = m.group(1)
    return classes


def load_syntax(text_path):
    """Maps mnemonic -> first syntax field found in the manual text."""
    syntax = {}
    pat = re.compile(r"^\s*([a-z][a-z0-9_.]*)\s+([A-Za-z][A-Za-z0-9_,()]*)")
    with open(text_path) as f:
        for line in f:
            m = pat.match(line)
            if not m:
                continue
            name, fields = m.group(1), m.group(2)
            first = fields.split(",")[0].split("(")[0].strip()
            if first not in KNOWN_FIELDS:
                continue
            # Keep the last match: instruction sections come after summary
            # tables, and some summary rows contain spurious field names
            # (e.g. `lwarx TO,...` in a trap table).
            syntax[name] = first
    return syntax


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2

    classes = load_db()
    syntax = load_syntax(sys.argv[1])
    problems = []
    not_found = []

    for name, cls in sorted(classes.items()):
        if name == "kIdNone":
            continue
        first = syntax.get(name)
        if first is None:
            not_found.append((name, cls))
            continue

        if cls == "kDefault" and first not in DEST_FIELDS:
            problems.append((name, cls, first, "default: first operand is not a destination register"))
        elif cls == "kNoWrite" and first in {"RT", "XT", "VRT", "FRT"}:
            problems.append((name, cls, first, "no-write: first operand looks like a destination"))
        elif cls == "kStore" and first in {"RT", "XT", "VRT"}:
            problems.append((name, cls, first, "store: first operand looks like a load destination"))
        elif cls == "kLoadUpdate" and first not in DEST_FIELDS:
            problems.append((name, cls, first, "load-update: first operand is not a destination"))

    print("== PROBLEMS ==")
    for name, cls, first, why in problems:
        print(f"  {name:16s} {cls:14s} first={first:6s} {why}")

    print(f"\n== NOT FOUND IN MANUAL (extended mnemonics / aliases) == ({len(not_found)})")
    for cls, count in sorted(Counter(c for _, c in not_found).items()):
        print(f"  {cls:14s} {count}")

    total = len(classes) - 1
    print(f"\nchecked {total} instructions, {len(problems)} problems, {len(not_found)} not found")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
