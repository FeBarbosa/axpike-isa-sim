#!/usr/bin/env python3
"""Run and gate the architectural LeNet baseline on all 10,000 images."""

from __future__ import annotations

import argparse
from pathlib import Path
from typing import Any

import run_reduced_lenet_validation as endpoint_runner
import run_uniform_n_full_matrix as full_runner
import run_uniform_n_reduced_matrix as shared_runner
import summarize_reduced_lenet_validation as summarizer


IMAGE_COUNT = 10_000
PURPOSE = "complete MNIST LeNet architectural baseline"
SCOPE = "scientific evaluation of the fixed 10,000-image MNIST test set"
CONFIGURATION = {
    "id": endpoint_runner.ARCHITECTURAL_BASELINE.identifier,
    "label": endpoint_runner.ARCHITECTURAL_BASELINE.label,
    "n": None,
    "transprecision_mode": "disabled",
    "protected_bits": None,
    "cli_argument": "--transprecision-mode=disabled",
}


def build_campaign(
    *,
    axpike: Path,
    application: Path,
    data_directory: Path,
    proxy_kernel: str,
) -> dict[str, Any]:
    repository_root = Path(__file__).resolve().parents[2]
    fixture_manifest_path = data_directory / "fixture-manifest.json"
    automation_paths = (
        Path(__file__).resolve(),
        repository_root
        / "verification/scripts/run_reduced_lenet_validation.py",
        repository_root
        / "verification/scripts/run_uniform_n_reduced_matrix.py",
        repository_root
        / "verification/scripts/run_uniform_n_full_matrix.py",
        repository_root
        / "verification/scripts/summarize_reduced_lenet_validation.py",
        repository_root
        / "verification/scripts/prepare_full_mnist_fixture.py",
    )
    return {
        "schema_version": 1,
        "purpose": PURPOSE,
        "scope": SCOPE,
        "image_count": IMAGE_COUNT,
        "mode": endpoint_runner.EXPERIMENT_MODE,
        "type_order": list(endpoint_runner.TYPE_ORDER),
        "source_revisions": {
            "axpike": endpoint_runner.git_provenance(
                repository_root,
                excluded_paths=endpoint_runner.AXPIKE_PROVENANCE_EXCLUSIONS,
            ),
            "adf": endpoint_runner.git_provenance(
                repository_root / "adele" / "adf"
            ),
            "application": endpoint_runner.git_provenance(
                application.resolve().parent
            ),
        },
        "inputs": {
            "axpike": shared_runner.artifact_identity(axpike),
            "application": shared_runner.artifact_identity(application),
            "data_directory": {
                "path": str(data_directory.resolve()),
                "fixture_manifest": shared_runner.artifact_identity(
                    fixture_manifest_path
                ),
                "files": {
                    filename: {
                        "size": (data_directory / filename).stat().st_size,
                        "sha256": endpoint_runner.sha256(
                            data_directory / filename
                        ),
                    }
                    for filename in endpoint_runner.MNIST_FILES
                },
            },
            "proxy_kernel": proxy_kernel,
        },
        "automation": {
            path.name: shared_runner.artifact_identity(path)
            for path in automation_paths
        },
        "execution_environment": shared_runner.execution_environment(
            axpike=axpike,
            application=application,
        ),
        "configurations": [
            shared_runner.configuration_identity(CONFIGURATION)
            | {"cli_argument": CONFIGURATION["cli_argument"]}
        ],
    }


def finalize_baseline(
    *,
    output_directory: Path,
    campaign: dict[str, Any],
    campaign_sha256: str,
    run_entries: list[dict[str, Any]],
) -> None:
    if len(run_entries) != 1:
        return
    run_manifest = {
        "schema_version": 5,
        "purpose": campaign["purpose"],
        "scope": campaign["scope"],
        "image_count": campaign["image_count"],
        "mode": campaign["mode"],
        "type_order": campaign["type_order"],
        "campaign": {
            "path": "campaign.json",
            "sha256": campaign_sha256,
        },
        "source_revisions": campaign["source_revisions"],
        "inputs": campaign["inputs"],
        "runs": run_entries,
    }
    manifest_path = output_directory / "run-manifest.json"
    shared_runner.write_json_atomic(manifest_path, run_manifest)
    summary = summarizer.build_summary(manifest_path)
    summary_directory = output_directory / "summary"
    summary_directory.mkdir(exist_ok=True)
    summary_path = summary_directory / "summary.json"
    summary_csv_path = summary_directory / "summary.csv"
    regions_csv_path = summary_directory / "regions.csv"
    regional_instructions_path = (
        summary_directory / "effective-types-by-region-instruction.csv"
    )
    shared_runner.write_json_atomic(summary_path, summary)
    summarizer.write_flat_csv(summary, summary_csv_path)
    summarizer.write_regions_csv(summary, regions_csv_path)
    summarizer.write_regional_instruction_csv(
        summary, regional_instructions_path
    )
    shared_runner.write_json_atomic(
        summary_directory / "summary-manifest.json",
        {
            "schema_version": 5,
            "source_manifest_sha256": endpoint_runner.sha256(manifest_path),
            "artifacts": {
                "summary.json": endpoint_runner.sha256(summary_path),
                "summary.csv": endpoint_runner.sha256(summary_csv_path),
                "regions.csv": endpoint_runner.sha256(regions_csv_path),
                "effective-types-by-region-instruction.csv":
                    endpoint_runner.sha256(regional_instructions_path),
            },
        },
    )
    shared_runner.write_json_atomic(
        output_directory / "baseline-gate.json",
        {
            "schema_version": 1,
            "status": "passed",
            "campaign_sha256": campaign_sha256,
            "configuration_count": 1,
            "run_manifest_sha256": endpoint_runner.sha256(manifest_path),
            "summary_sha256": endpoint_runner.sha256(summary_path),
            "invariants": shared_runner.PASSED_INVARIANTS,
        },
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Run, resume, and gate the architectural LeNet baseline on the "
            "complete 10,000-image MNIST test set."
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
    parser.add_argument(
        "--max-new-runs",
        type=int,
        help=(
            "Use zero to initialize and validate the campaign without "
            "executing the baseline."
        ),
    )
    args = parser.parse_args()
    if args.max_new_runs is not None and args.max_new_runs not in (0, 1):
        raise ValueError("--max-new-runs must be zero or one")

    endpoint_runner.require_file(args.axpike, "AxPIKE executable")
    endpoint_runner.require_file(args.application, "LeNet application")
    full_runner.require_fixture(args.data_directory)
    campaign = build_campaign(
        axpike=args.axpike,
        application=args.application,
        data_directory=args.data_directory,
        proxy_kernel=args.proxy_kernel,
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
        configurations=[CONFIGURATION],
        axpike=args.axpike,
        proxy_kernel=args.proxy_kernel,
        application=args.application,
        data_directory=args.data_directory,
        max_new_runs=args.max_new_runs,
        image_count=IMAGE_COUNT,
    )
    finalize_baseline(
        output_directory=args.output_directory,
        campaign=campaign,
        campaign_sha256=campaign_sha256,
        run_entries=entries,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
