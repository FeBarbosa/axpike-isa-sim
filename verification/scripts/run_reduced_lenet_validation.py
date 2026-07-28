#!/usr/bin/env python3
"""Run the deterministic 59-image LeNet validation endpoints.

This runner intentionally executes only the exact and no-protection dynamic
policies.  It is an implementation/model-validation aid, not the scientific
experiment matrix.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Any


IMAGE_COUNT = 59
TRANSITION_ORDER = (
    "fp32-e5m2",
    "fp32-fp16",
    "fp64-e5m2",
    "fp64-fp16",
    "fp64-fp32",
)
EXACT_PROTECTED_BITS = (21, 13, 50, 42, 29)
NO_PROTECTION_BITS = (0, 0, 0, 0, 0)
MNIST_FILES = (
    "train-images-idx3-ubyte",
    "train-labels-idx1-ubyte",
    "t10k-images-idx3-ubyte",
    "t10k-labels-idx1-ubyte",
)


@dataclass(frozen=True)
class Configuration:
    identifier: str
    label: str
    protected_bits: tuple[int, ...]


CONFIGURATIONS = (
    Configuration("exact", "Exact policy", EXACT_PROTECTED_BITS),
    Configuration(
        "no-protection",
        "No-protection endpoint",
        NO_PROTECTION_BITS,
    ),
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as input_file:
        for chunk in iter(lambda: input_file.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def protected_bits_mapping(
    values: tuple[int, ...],
) -> dict[str, int]:
    return dict(zip(TRANSITION_ORDER, values))


def policy_argument(values: tuple[int, ...]) -> str:
    entries = ",".join(
        f"{name}:{value}"
        for name, value in zip(TRANSITION_ORDER, values)
    )
    return f"--transprecision-protected-bits={entries}"


def parse_outcome(log_text: str) -> dict[str, int]:
    outcome: dict[str, int] = {}
    for field in ("processed", "correct", "errors"):
        matches = re.findall(
            rf"(?m)^{field}:\s*(\d+)\s*$",
            log_text,
        )
        if len(matches) != 1:
            raise ValueError(
                f"expected one '{field}' line, observed {len(matches)}"
            )
        outcome[field] = int(matches[0])
    if outcome["processed"] != IMAGE_COUNT:
        raise ValueError(
            f"processed {outcome['processed']} images; expected {IMAGE_COUNT}"
        )
    if outcome["correct"] + outcome["errors"] != outcome["processed"]:
        raise ValueError("correct + errors does not equal processed")
    return outcome


def find_single_csv(run_directory: Path, stem: str) -> Path:
    matches = sorted(run_directory.glob(f"AxPIKE_{stem}_*.csv"))
    if len(matches) != 1:
        raise ValueError(
            f"expected one AxPIKE {stem} CSV in {run_directory}, "
            f"observed {len(matches)}"
        )
    return matches[0]


def write_json(path: Path, value: Any) -> None:
    path.write_text(
        json.dumps(value, indent=2, sort_keys=True, ensure_ascii=True) + "\n",
        encoding="utf-8",
    )


def run_configuration(
    *,
    configuration: Configuration,
    axpike: Path,
    proxy_kernel: str,
    application: Path,
    data_directory: Path,
    output_directory: Path,
) -> dict[str, Any]:
    run_directory = output_directory / configuration.identifier
    run_directory.mkdir()
    (run_directory / "data").symlink_to(
        data_directory.resolve(),
        target_is_directory=True,
    )

    command = [
        str(axpike.resolve()),
        policy_argument(configuration.protected_bits),
        proxy_kernel,
        str(application.resolve()),
        "direct-logits",
        str(IMAGE_COUNT),
    ]
    completed = subprocess.run(
        command,
        cwd=run_directory,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    log_path = run_directory / "application.log"
    log_path.write_text(completed.stdout, encoding="utf-8")
    if completed.returncode != 0:
        raise RuntimeError(
            f"{configuration.identifier} exited with "
            f"{completed.returncode}; see {log_path}"
        )

    outcome = parse_outcome(completed.stdout)
    csv_paths = {
        stem: find_single_csv(run_directory, stem)
        for stem in ("counters", "energy", "transprecision")
    }
    run_record = {
        "schema_version": 1,
        "id": configuration.identifier,
        "label": configuration.label,
        "image_count": IMAGE_COUNT,
        "mode": "direct-logits",
        "protected_bits": protected_bits_mapping(
            configuration.protected_bits
        ),
        "command": command,
        "outcome": outcome,
        "artifacts": {
            name: {
                "path": path.name,
                "sha256": sha256(path),
            }
            for name, path in csv_paths.items()
        }
        | {
            "application_log": {
                "path": log_path.name,
                "sha256": sha256(log_path),
            }
        },
    }
    run_record_path = run_directory / "run.json"
    write_json(run_record_path, run_record)
    return {
        "id": configuration.identifier,
        "record": str(run_record_path.relative_to(output_directory)),
        "sha256": sha256(run_record_path),
    }


def require_file(path: Path, description: str) -> None:
    if not path.is_file():
        raise ValueError(f"{description} is not a file: {path}")


def git_provenance(path: Path) -> dict[str, Any]:
    root = subprocess.run(
        ["git", "-C", str(path), "rev-parse", "--show-toplevel"],
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    ).stdout.strip()
    commit = subprocess.run(
        ["git", "-C", root, "rev-parse", "HEAD"],
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    ).stdout.strip()
    status = subprocess.run(
        ["git", "-C", root, "status", "--porcelain"],
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    ).stdout
    return {
        "root": root,
        "commit": commit,
        "dirty": bool(status),
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Run exact and no-protection policies on the deterministic "
            "59-image LeNet fixture."
        )
    )
    parser.add_argument("--axpike", type=Path, required=True)
    parser.add_argument(
        "--proxy-kernel",
        default="pk",
        help="Proxy-kernel argument passed to AxPIKE (default: pk).",
    )
    parser.add_argument("--application", type=Path, required=True)
    parser.add_argument("--data-directory", type=Path, required=True)
    parser.add_argument("--output-directory", type=Path, required=True)
    args = parser.parse_args()

    require_file(args.axpike, "AxPIKE executable")
    require_file(args.application, "LeNet application")
    if not args.data_directory.is_dir():
        raise ValueError(
            f"data directory is not a directory: {args.data_directory}"
        )
    for filename in MNIST_FILES:
        require_file(
            args.data_directory / filename,
            f"MNIST input {filename}",
        )
    if args.output_directory.exists():
        raise ValueError(
            "output directory already exists; choose a new directory to "
            "preserve prior validation artifacts"
        )
    args.output_directory.mkdir(parents=True)

    runs = [
        run_configuration(
            configuration=configuration,
            axpike=args.axpike,
            proxy_kernel=args.proxy_kernel,
            application=args.application,
            data_directory=args.data_directory,
            output_directory=args.output_directory,
        )
        for configuration in CONFIGURATIONS
    ]
    manifest = {
        "schema_version": 1,
        "purpose": "reduced deterministic LeNet validation",
        "scope": "implementation and simulator-model validation only",
        "image_count": IMAGE_COUNT,
        "mode": "direct-logits",
        "transition_order": list(TRANSITION_ORDER),
        "source_revisions": {
            "axpike": git_provenance(
                Path(__file__).resolve().parents[2]
            ),
            "adf": git_provenance(
                Path(__file__).resolve().parents[2] / "adele" / "adf"
            ),
            "application": git_provenance(args.application.resolve().parent),
        },
        "inputs": {
            "axpike": {
                "path": str(args.axpike.resolve()),
                "sha256": sha256(args.axpike),
            },
            "application": {
                "path": str(args.application.resolve()),
                "sha256": sha256(args.application),
            },
            "data_directory": {
                "path": str(args.data_directory.resolve()),
                "files": {
                    filename: {
                        "size": (args.data_directory / filename).stat().st_size,
                        "sha256": sha256(args.data_directory / filename),
                    }
                    for filename in MNIST_FILES
                },
            },
            "proxy_kernel": args.proxy_kernel,
        },
        "runs": runs,
    }
    write_json(args.output_directory / "run-manifest.json", manifest)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
