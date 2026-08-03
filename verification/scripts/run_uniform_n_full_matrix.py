#!/usr/bin/env python3
"""Run the resumable 51-point uniform-n campaign on all 10,000 images."""

from __future__ import annotations

import argparse
from pathlib import Path
from typing import Any

import prepare_full_mnist_fixture as full_fixture
import run_reduced_lenet_validation as endpoint_runner
import run_uniform_n_reduced_matrix as shared_runner


IMAGE_COUNT = 10_000
PURPOSE = "uniform-n complete MNIST LeNet scientific evaluation"
SCOPE = "scientific evaluation of the fixed 10,000-image MNIST test set"
REDUCED_PURPOSE = "uniform-n reduced deterministic LeNet validation"


def require_fixture(data_directory: Path) -> dict[str, Any]:
    if not data_directory.is_dir():
        raise ValueError(f"data directory is not a directory: {data_directory}")
    manifest_path = data_directory / "fixture-manifest.json"
    endpoint_runner.require_file(manifest_path, "full-fixture manifest")
    manifest = shared_runner.read_json(manifest_path)
    if (
        manifest.get("schema_version") != 1
        or manifest.get("purpose") != "complete standard MNIST fixture"
        or manifest.get("test_image_count") != IMAGE_COUNT
        or manifest.get("train_image_count") != full_fixture.TRAIN_COUNT
    ):
        raise ValueError("unexpected complete-MNIST fixture identity")

    fixture_files = manifest.get("fixture_files")
    if not isinstance(fixture_files, dict):
        raise ValueError("full-fixture manifest has no file records")
    for filename, (kind, count) in full_fixture.FILES.items():
        path = data_directory / filename
        endpoint_runner.require_file(path, f"MNIST input {filename}")
        full_fixture.validate_idx(path, kind, count)
        expected = fixture_files.get(filename)
        if not isinstance(expected, dict):
            raise ValueError(f"fixture manifest omits {filename}")
        if (
            expected.get("size") != path.stat().st_size
            or expected.get("sha256") != endpoint_runner.sha256(path)
        ):
            raise ValueError(f"fixture file differs from manifest: {filename}")
    return manifest


def executable_revision(revision: dict[str, Any]) -> dict[str, Any]:
    return {
        "commit": revision.get("commit"),
        "dirty": revision.get("dirty"),
    }


def require_reduced_gate(
    gate_path: Path,
    campaign: dict[str, Any],
) -> dict[str, Any]:
    endpoint_runner.require_file(gate_path, "reduced matrix gate")
    gate = shared_runner.read_json(gate_path)
    reduced_campaign_path = gate_path.parent / "campaign.json"
    endpoint_runner.require_file(
        reduced_campaign_path, "reduced campaign identity"
    )
    reduced_campaign = shared_runner.read_json(reduced_campaign_path)
    if (
        gate.get("schema_version") != 1
        or gate.get("status") != "passed"
        or gate.get("configuration_count")
        != shared_runner.EXPECTED_CONFIGURATION_COUNT
        or gate.get("invariants") != shared_runner.PASSED_INVARIANTS
        or gate.get("campaign_sha256")
        != endpoint_runner.sha256(reduced_campaign_path)
    ):
        raise ValueError("reduced matrix gate is not a complete passed gate")
    if (
        reduced_campaign.get("schema_version")
        != shared_runner.CAMPAIGN_SCHEMA_VERSION
        or reduced_campaign.get("purpose") != REDUCED_PURPOSE
        or reduced_campaign.get("image_count") != endpoint_runner.IMAGE_COUNT
    ):
        raise ValueError("unexpected reduced campaign identity")

    for repository in ("axpike", "adf", "application"):
        if executable_revision(
            reduced_campaign["source_revisions"][repository]
        ) != executable_revision(campaign["source_revisions"][repository]):
            raise ValueError(
                f"full and reduced campaigns use different {repository} "
                "source revisions"
            )
    if (
        reduced_campaign["matrix"]["sha256"]
        != campaign["matrix"]["sha256"]
        or reduced_campaign["inputs"]["axpike"]["sha256"]
        != campaign["inputs"]["axpike"]["sha256"]
        or reduced_campaign["inputs"]["application"]["sha256"]
        != campaign["inputs"]["application"]["sha256"]
        or reduced_campaign["inputs"]["proxy_kernel"]
        != campaign["inputs"]["proxy_kernel"]
    ):
        raise ValueError(
            "full campaign matrix or executable inputs differ from reduced gate"
        )
    return {
        "reduced_matrix_gate": shared_runner.artifact_identity(gate_path),
        "reduced_campaign": shared_runner.artifact_identity(
            reduced_campaign_path
        ),
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Run, resume, and gate all 51 uniform-n configurations on the "
            "complete 10,000-image MNIST test set."
        )
    )
    parser.add_argument("--matrix", type=Path, required=True)
    parser.add_argument("--axpike", type=Path, required=True)
    parser.add_argument(
        "--proxy-kernel",
        default="pk",
        help="Proxy-kernel argument passed to AxPIKE (default: pk).",
    )
    parser.add_argument("--application", type=Path, required=True)
    parser.add_argument("--data-directory", type=Path, required=True)
    parser.add_argument("--reduced-gate", type=Path, required=True)
    parser.add_argument("--output-directory", type=Path, required=True)
    parser.add_argument(
        "--max-new-runs",
        type=int,
        help=(
            "Stop successfully after this many new runs; completed runs "
            "remain resumable."
        ),
    )
    args = parser.parse_args()
    if args.max_new_runs is not None and args.max_new_runs < 0:
        raise ValueError("--max-new-runs must be nonnegative")

    endpoint_runner.require_file(args.matrix, "uniform-n matrix manifest")
    endpoint_runner.require_file(args.axpike, "AxPIKE executable")
    endpoint_runner.require_file(args.application, "LeNet application")
    require_fixture(args.data_directory)
    matrix_path = args.matrix.resolve()
    matrix = shared_runner.read_json(matrix_path)
    configurations = shared_runner.validate_matrix(matrix)
    repository_root = Path(__file__).resolve().parents[2]
    automation_paths = (
        Path(__file__).resolve(),
        repository_root
        / "verification/scripts/run_uniform_n_reduced_matrix.py",
        repository_root
        / "verification/scripts/run_reduced_lenet_validation.py",
        repository_root
        / "verification/scripts/summarize_reduced_lenet_validation.py",
        repository_root
        / "verification/scripts/generate_transprecision_experiment_matrix.py",
        repository_root
        / "verification/scripts/prepare_full_mnist_fixture.py",
    )
    campaign = shared_runner.build_campaign(
        matrix_path=matrix_path,
        matrix=matrix,
        configurations=configurations,
        axpike=args.axpike,
        application=args.application,
        data_directory=args.data_directory,
        proxy_kernel=args.proxy_kernel,
        image_count=IMAGE_COUNT,
        purpose=PURPOSE,
        scope=SCOPE,
        automation_paths=automation_paths,
    )
    campaign["prerequisites"] = require_reduced_gate(
        args.reduced_gate.resolve(), campaign
    )
    campaign_path, campaign_sha256 = (
        shared_runner.initialize_or_validate_campaign(
            args.output_directory, campaign
        )
    )
    if campaign_path.parent != args.output_directory:
        raise ValueError("campaign path escaped the output directory")
    entries = shared_runner.run_pending_configurations(
        output_directory=args.output_directory,
        campaign_sha256=campaign_sha256,
        configurations=configurations,
        axpike=args.axpike,
        proxy_kernel=args.proxy_kernel,
        application=args.application,
        data_directory=args.data_directory,
        max_new_runs=args.max_new_runs,
        image_count=IMAGE_COUNT,
    )
    shared_runner.finalize_campaign(
        output_directory=args.output_directory,
        campaign=campaign,
        campaign_sha256=campaign_sha256,
        run_entries=entries,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
