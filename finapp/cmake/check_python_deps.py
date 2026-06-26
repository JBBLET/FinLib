"""Verify the running interpreter has the distributions declared in a pyproject.toml.

Usage:
    python check_python_deps.py <path/to/pyproject.toml>

Checks [project].dependencies by *distribution* name (so import-name mismatches like
beautifulsoup4 -> bs4 are handled correctly). Prints any missing distributions to stdout
(space-separated) and exits 1 if there are any. Version pins are reported on stderr as
informational notes only and never cause failure (a consumer's transitive versions may
differ). Exit 2 means the check itself could not run.
"""

import re
import sys
from importlib import metadata


def dist_name(spec):
    # "yfinance==1.1.0" / "foo[extra]>=1 ; python_version>'3'" -> "yfinance" / "foo"
    spec = spec.split(";", 1)[0].strip()
    match = re.match(r"[A-Za-z0-9][A-Za-z0-9._-]*", spec)
    return match.group(0) if match else None


def pinned_version(spec):
    match = re.search(r"==\s*([^\s,;]+)", spec)
    return match.group(1) if match else None


def main():
    if len(sys.argv) < 2:
        print("usage: check_python_deps.py <pyproject.toml>", file=sys.stderr)
        return 2
    try:
        import tomllib  # stdlib in Python >= 3.11
    except ModuleNotFoundError:
        print(
            "note: tomllib unavailable (Python < 3.11) — skipping dep check",
            file=sys.stderr,
        )
        return 0
    try:
        with open(sys.argv[1], "rb") as handle:
            data = tomllib.load(handle)
    except OSError as exc:
        print(f"could not read {sys.argv[1]}: {exc}", file=sys.stderr)
        return 2

    deps = data.get("project", {}).get("dependencies", [])
    missing = []
    for spec in deps:
        name = dist_name(spec)
        if not name:
            continue
        try:
            have = metadata.version(name)
        except metadata.PackageNotFoundError:
            missing.append(name)
            continue
        want = pinned_version(spec)
        if want and have != want:
            print(
                f"note: {name} {have} installed, pyproject pins {want}", file=sys.stderr
            )

    if missing:
        print(" ".join(missing))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
