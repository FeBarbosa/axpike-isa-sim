#!/usr/bin/env python3
"""Run the deterministic 62-image LeNet validation endpoints.

This runner intentionally executes only the full-protection and no-protection
dynamic policies. It is an implementation/model-validation aid, not the
scientific experiment matrix.
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


IMAGE_COUNT = 62
EXPERIMENT_MODE = "softmax"
TYPE_ORDER = ("fp64", "fp32", "fp16", "e5m2")
FULL_PROTECTION_BITS = (50, 21, 8, 0)
NO_PROTECTION_BITS = (0, 0, 0, 0)
MNIST_FILES = (
    "train-images-idx3-ubyte",
    "train-labels-idx1-ubyte",
    "t10k-images-idx3-ubyte",
    "t10k-labels-idx1-ubyte",
)
AXPIKE_PROVENANCE_EXCLUSIONS = (
    (
        "paper-sscad2026",
        "independently versioned article sources do not affect the "
        "simulator or application executables",
    ),
)


@dataclass(frozen=True)
class Configuration:
    identifier: str
    label: str
    protected_bits: tuple[int, ...]
    n: int | None = None


CONFIGURATIONS = (
    Configuration(
        "full-protection",
        "Full-protection policy",
        FULL_PROTECTION_BITS,
    ),
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
    return dict(zip(TYPE_ORDER, values))


def policy_argument(values: tuple[int, ...]) -> str:
    entries = ",".join(
        f"{name}:{value}"
        for name, value in zip(TYPE_ORDER, values)
    )
    return f"--transprecision-type-protected-bits={entries}"


def parse_outcome(
    log_text: str,
    expected_image_count: int = IMAGE_COUNT,
) -> dict[str, int]:
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
    if outcome["processed"] != expected_image_count:
        raise ValueError(
            f"processed {outcome['processed']} images; expected "
            f"{expected_image_count}"
        )
    if outcome["correct"] + outcome["errors"] != outcome["processed"]:
        raise ValueError("correct + errors does not equal processed")
    return outcome


def find_single_csv(run_directory: Path, stem: str) -> Path:
    matches = sorted(run_directory.glob(f"AxPIKE_{stem}_*.csv"))
    if stem == "transprecision":
        matches = [
            path for path in matches
            if not path.name.startswith("AxPIKE_transprecision_sections_")
        ]
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
    image_count: int,
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
        EXPERIMENT_MODE,
        str(image_count),
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

    outcome = parse_outcome(completed.stdout, image_count)
    csv_paths = {
        stem: find_single_csv(run_directory, stem)
        for stem in (
            "counters",
            "energy",
            "transprecision",
            "transprecision_sections",
        )
    }
    run_record = {
        "schema_version": 4,
        "id": configuration.identifier,
        "label": configuration.label,
        "image_count": image_count,
        "mode": EXPERIMENT_MODE,
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
        } | {
            "application_log": {
                "path": log_path.name,
                "sha256": sha256(log_path),
            },
        },
    }
    if configuration.n is not None:
        run_record["n"] = configuration.n
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


def run_git(
    root: Path | str,
    arguments: list[str],
    *,
    check: bool = True,
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["git", "-C", str(root), *arguments],
        check=check,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )


def git_provenance(
    path: Path,
    *,
    excluded_paths: tuple[tuple[str, str], ...] = (),
) -> dict[str, Any]:
    for relative_path, _reason in excluded_paths:
        candidate = Path(relative_path)
        if (
            candidate.is_absolute()
            or not candidate.parts
            or ".." in candidate.parts
            or relative_path == "."
        ):
            raise ValueError(
                f"provenance exclusion must be a repository-relative path: "
                f"{relative_path!r}"
            )

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
    status_arguments = ["status", "--porcelain", "--", "."]
    status_arguments.extend(
        f":(exclude,top){relative_path}"
        for relative_path, _reason in excluded_paths
    )
    status = run_git(root, status_arguments).stdout
    provenance: dict[str, Any] = {
        "root": root,
        "commit": commit,
        "dirty": bool(status),
    }
    if not excluded_paths:
        return provenance

    excluded_states = []
    for relative_path, reason in excluded_paths:
        target = Path(root) / relative_path
        parent_status = run_git(
            root,
            ["status", "--porcelain", "--", relative_path],
        ).stdout.splitlines()
        tracked_object_result = run_git(
            root,
            ["rev-parse", f"HEAD:{relative_path}"],
            check=False,
        )
        state: dict[str, Any] = {
            "path": relative_path,
            "reason": reason,
            "parent_status_entries": parent_status,
            "tracked_object": (
                tracked_object_result.stdout.strip()
                if tracked_object_result.returncode == 0
                else None
            ),
        }

        if target.is_dir():
            nested_root_result = run_git(
                target,
                ["rev-parse", "--show-toplevel"],
                check=False,
            )
            if (
                nested_root_result.returncode == 0
                and Path(nested_root_result.stdout.strip()).resolve()
                == target.resolve()
            ):
                nested_status = run_git(
                    target, ["status", "--porcelain"]
                ).stdout
                state["independent_worktree"] = {
                    "root": str(target.resolve()),
                    "commit": run_git(
                        target, ["rev-parse", "HEAD"]
                    ).stdout.strip(),
                    "dirty": bool(nested_status),
                    "status_entries": nested_status.splitlines(),
                }
        excluded_states.append(state)

    provenance["status_scope"] = {
        "included": ".",
        "excluded_paths": [
            relative_path for relative_path, _reason in excluded_paths
        ],
    }
    provenance["excluded_path_states"] = excluded_states
    return provenance


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Run full-protection and no-protection policies on the "
            "deterministic 62-image LeNet fixture."
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
    fixture_manifest_path = args.data_directory / "fixture-manifest.json"
    require_file(fixture_manifest_path, "reduced-fixture manifest")
    fixture_manifest = json.loads(
        fixture_manifest_path.read_text(encoding="utf-8")
    )
    if fixture_manifest.get("schema_version") != 2:
        raise ValueError("unsupported reduced-fixture manifest schema")
    if fixture_manifest.get("image_count") != IMAGE_COUNT:
        raise ValueError(
            f"fixture contains {fixture_manifest.get('image_count')} images; "
            f"expected {IMAGE_COUNT}"
        )
    selection = fixture_manifest.get("selection", {})
    if (
        not selection.get("covers_all_classes")
        or not selection.get("minimal_prefix_covering_all_classes")
    ):
        raise ValueError(
            "fixture is not the minimal contiguous prefix covering all classes"
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
            image_count=IMAGE_COUNT,
        )
        for configuration in CONFIGURATIONS
    ]
    manifest = {
        "schema_version": 3,
        "purpose": "reduced deterministic LeNet validation",
        "scope": "implementation and simulator-model validation only",
        "image_count": IMAGE_COUNT,
        "mode": EXPERIMENT_MODE,
        "type_order": list(TYPE_ORDER),
        "source_revisions": {
            "axpike": git_provenance(
                Path(__file__).resolve().parents[2],
                excluded_paths=AXPIKE_PROVENANCE_EXCLUSIONS,
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
                "fixture_manifest": {
                    "path": str(fixture_manifest_path.resolve()),
                    "sha256": sha256(fixture_manifest_path),
                    "selection": selection,
                },
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
