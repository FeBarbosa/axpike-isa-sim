#!/usr/bin/env python3
"""Run and gate the resumable 51-point uniform-n reduced LeNet matrix."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any, Callable

import run_reduced_lenet_validation as endpoint_runner
import summarize_reduced_lenet_validation as summarizer


CAMPAIGN_SCHEMA_VERSION = 2
RUN_MANIFEST_SCHEMA_VERSION = 4
MATRIX_SCHEMA_VERSION = 3
EXPECTED_CONFIGURATION_COUNT = 51
POLICY_NAME = "effective-type-quantization-v4"
POLICY_VERSION = 4
PURPOSE = "uniform-n reduced deterministic LeNet validation"
PASSED_INVARIANTS = {
    "outcome_partition": "passed",
    "policy_matches_command": "passed",
    "effective_type_totals_derived_by_instruction": "passed",
    "unclassified_effective_types_zero": "passed",
    "unclassified_operands_zero": "passed",
    "fallback_events_zero": "passed",
    "invalid_result_promotions_zero": "passed",
    "quantizations_not_above_operation_results": "passed",
    "quantization_range_event_invariants": "passed",
    "changed_tag_reductions_not_above_total": "passed",
    "effective_boxed_fp32_not_above_external_nan": "passed",
    "global_transprecision_equals_sum_of_regions": "passed",
    "expected_network_regions_present": "passed",
}


def read_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json_atomic(path: Path, value: Any) -> None:
    temporary_path = path.with_name(path.name + ".tmp")
    endpoint_runner.write_json(temporary_path, value)
    temporary_path.replace(path)


def expected_vector(n: int) -> tuple[int, ...]:
    return tuple(min(n, limit) for limit in (50, 21, 8, 0))


def validate_matrix(matrix: dict[str, Any]) -> list[dict[str, Any]]:
    if matrix.get("schema_version") != MATRIX_SCHEMA_VERSION:
        raise ValueError("uniform-n matrix requires schema version 3")
    if matrix.get("policy") != {
        "name": POLICY_NAME,
        "version": POLICY_VERSION,
    }:
        raise ValueError("unexpected matrix policy")
    if tuple(matrix.get("type_order", ())) != endpoint_runner.TYPE_ORDER:
        raise ValueError("unexpected matrix type order")
    if matrix.get("parameterization") != {
        "kind": "uniform-saturated",
        "minimum_n": 0,
        "maximum_n": 50,
        "rule": "effective_n(type)=min(n,maximum_n(type))",
    }:
        raise ValueError("unexpected matrix parameterization")

    configurations = matrix.get("configurations")
    if not isinstance(configurations, list):
        raise ValueError("matrix configurations must be a list")
    if (
        matrix.get("dynamic_configuration_count")
        != EXPECTED_CONFIGURATION_COUNT
        or matrix.get("planned_new_execution_count")
        != EXPECTED_CONFIGURATION_COUNT
        or len(configurations) != EXPECTED_CONFIGURATION_COUNT
    ):
        raise ValueError("uniform-n matrix must contain 51 configurations")

    identifiers = set()
    for n, configuration in enumerate(configurations):
        if configuration.get("n") != n:
            raise ValueError("matrix configurations must contain n=0..50")
        vector = expected_vector(n)
        expected_mapping = endpoint_runner.protected_bits_mapping(vector)
        if configuration.get("protected_bits") != expected_mapping:
            raise ValueError(f"n={n}: protected-bit vector is not saturated")
        if configuration.get("cli_argument") != endpoint_runner.policy_argument(
            vector
        ):
            raise ValueError(f"n={n}: noncanonical policy argument")
        identifier = configuration.get("id")
        if not isinstance(identifier, str) or not identifier:
            raise ValueError(f"n={n}: missing configuration identifier")
        if identifier in identifiers:
            raise ValueError(f"n={n}: duplicate configuration identifier")
        identifiers.add(identifier)
    return configurations


def require_fixture(data_directory: Path) -> dict[str, Any]:
    if not data_directory.is_dir():
        raise ValueError(f"data directory is not a directory: {data_directory}")
    for filename in endpoint_runner.MNIST_FILES:
        endpoint_runner.require_file(
            data_directory / filename,
            f"MNIST input {filename}",
        )
    fixture_manifest_path = data_directory / "fixture-manifest.json"
    endpoint_runner.require_file(
        fixture_manifest_path, "reduced-fixture manifest"
    )
    fixture_manifest = read_json(fixture_manifest_path)
    selection = fixture_manifest.get("selection", {})
    if (
        fixture_manifest.get("schema_version") != 2
        or fixture_manifest.get("image_count") != endpoint_runner.IMAGE_COUNT
        or not selection.get("covers_all_classes")
        or not selection.get("minimal_prefix_covering_all_classes")
    ):
        raise ValueError(
            "fixture must be the schema-2 minimal 62-image class-covering prefix"
        )
    return fixture_manifest


def artifact_identity(path: Path) -> dict[str, Any]:
    return {
        "path": str(path.resolve()),
        "sha256": endpoint_runner.sha256(path),
    }


def build_campaign(
    *,
    matrix_path: Path,
    matrix: dict[str, Any],
    configurations: list[dict[str, Any]],
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
        / "verification/scripts/summarize_reduced_lenet_validation.py",
        repository_root
        / "verification/scripts/generate_transprecision_experiment_matrix.py",
    )
    return {
        "schema_version": CAMPAIGN_SCHEMA_VERSION,
        "purpose": PURPOSE,
        "scope": "implementation and simulator-model validation only",
        "image_count": endpoint_runner.IMAGE_COUNT,
        "mode": endpoint_runner.EXPERIMENT_MODE,
        "type_order": list(endpoint_runner.TYPE_ORDER),
        "matrix": {
            **artifact_identity(matrix_path),
            "schema_version": matrix["schema_version"],
            "configuration_count": len(configurations),
        },
        "source_revisions": {
            "axpike": endpoint_runner.git_provenance(
                repository_root,
                excluded_paths=(
                    endpoint_runner.AXPIKE_PROVENANCE_EXCLUSIONS
                ),
            ),
            "adf": endpoint_runner.git_provenance(
                repository_root / "adele" / "adf"
            ),
            "application": endpoint_runner.git_provenance(
                application.resolve().parent
            ),
        },
        "inputs": {
            "axpike": artifact_identity(axpike),
            "application": artifact_identity(application),
            "data_directory": {
                "path": str(data_directory.resolve()),
                "fixture_manifest": artifact_identity(fixture_manifest_path),
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
            path.name: artifact_identity(path)
            for path in automation_paths
        },
        "configurations": [
            {
                "n": configuration["n"],
                "id": configuration["id"],
                "protected_bits": configuration["protected_bits"],
                "cli_argument": configuration["cli_argument"],
            }
            for configuration in configurations
        ],
    }


def initialize_or_validate_campaign(
    output_directory: Path,
    expected_campaign: dict[str, Any],
) -> tuple[Path, str]:
    if output_directory.exists() and not output_directory.is_dir():
        raise ValueError("output path exists and is not a directory")
    output_directory.mkdir(parents=True, exist_ok=True)
    campaign_path = output_directory / "campaign.json"
    if campaign_path.exists():
        observed_campaign = read_json(campaign_path)
        if observed_campaign != expected_campaign:
            raise ValueError(
                "existing campaign identity differs from current inputs or code"
            )
    else:
        if any(output_directory.iterdir()):
            raise ValueError(
                "nonempty output directory has no campaign identity"
            )
        write_json_atomic(campaign_path, expected_campaign)
    return campaign_path, endpoint_runner.sha256(campaign_path)


def next_attempt_directory(configuration_directory: Path) -> Path:
    configuration_directory.mkdir(parents=True, exist_ok=True)
    attempt_numbers = []
    for path in configuration_directory.iterdir():
        match = re.fullmatch(r"attempt-(\d{4})", path.name)
        if path.is_dir() and match:
            attempt_numbers.append(int(match.group(1)))
    next_number = max(attempt_numbers, default=0) + 1
    attempt_directory = configuration_directory / f"attempt-{next_number:04d}"
    attempt_directory.mkdir()
    return attempt_directory


def validate_completion(
    *,
    output_directory: Path,
    configuration: dict[str, Any],
    campaign_sha256: str,
    extract_run: Callable[
        [Path, dict[str, Any]], dict[str, Any]
    ] | None = None,
) -> dict[str, Any] | None:
    completion_path = (
        output_directory
        / "runs"
        / f"n-{configuration['n']:02d}"
        / "completion.json"
    )
    if not completion_path.exists():
        return None
    completion = read_json(completion_path)
    expected_identity = {
        "n": configuration["n"],
        "id": configuration["id"],
        "protected_bits": configuration["protected_bits"],
    }
    if (
        completion.get("schema_version") != 1
        or completion.get("status") != "passed"
        or completion.get("campaign_sha256") != campaign_sha256
        or completion.get("configuration") != expected_identity
    ):
        raise ValueError(f"invalid completion marker: {completion_path}")

    run_entry = completion.get("run")
    if not isinstance(run_entry, dict):
        raise ValueError(f"completion has no run entry: {completion_path}")
    extractor = summarizer.extract_run if extract_run is None else extract_run
    extracted = extractor(output_directory, run_entry)
    if (
        extracted["id"] != configuration["id"]
        or extracted["n"] != configuration["n"]
        or extracted["protected_bits"] != configuration["protected_bits"]
    ):
        raise ValueError(f"completed run does not match matrix: {completion_path}")

    gate_entry = completion.get("gate")
    if not isinstance(gate_entry, dict):
        raise ValueError(f"completion has no gate entry: {completion_path}")
    gate_path = output_directory / gate_entry["path"]
    if endpoint_runner.sha256(gate_path) != gate_entry["sha256"]:
        raise ValueError(f"gate hash mismatch: {gate_path}")
    gate = read_json(gate_path)
    if (
        gate.get("status") != "passed"
        or gate.get("campaign_sha256") != campaign_sha256
        or gate.get("invariants") != PASSED_INVARIANTS
    ):
        raise ValueError(f"invalid gate record: {gate_path}")
    return run_entry


def progress_record(
    *,
    campaign_sha256: str,
    configurations: list[dict[str, Any]],
    completed_ids: set[str],
    status: str,
    failure: dict[str, Any] | None = None,
) -> dict[str, Any]:
    record = {
        "schema_version": 1,
        "campaign_sha256": campaign_sha256,
        "status": status,
        "planned": len(configurations),
        "passed": len(completed_ids),
        "pending": len(configurations) - len(completed_ids),
        "passed_ids": [
            configuration["id"]
            for configuration in configurations
            if configuration["id"] in completed_ids
        ],
        "pending_ids": [
            configuration["id"]
            for configuration in configurations
            if configuration["id"] not in completed_ids
        ],
    }
    if failure is not None:
        record["last_failure"] = failure
    return record


def run_pending_configurations(
    *,
    output_directory: Path,
    campaign_sha256: str,
    configurations: list[dict[str, Any]],
    axpike: Path,
    proxy_kernel: str,
    application: Path,
    data_directory: Path,
    max_new_runs: int | None,
) -> list[dict[str, Any]]:
    completed_entries: dict[str, dict[str, Any]] = {}
    for configuration in configurations:
        entry = validate_completion(
            output_directory=output_directory,
            configuration=configuration,
            campaign_sha256=campaign_sha256,
        )
        if entry is not None:
            completed_entries[configuration["id"]] = entry

    write_json_atomic(
        output_directory / "progress.json",
        progress_record(
            campaign_sha256=campaign_sha256,
            configurations=configurations,
            completed_ids=set(completed_entries),
            status="running",
        ),
    )

    new_runs = 0
    for configuration in configurations:
        if configuration["id"] in completed_entries:
            continue
        if max_new_runs is not None and new_runs >= max_new_runs:
            break

        configuration_directory = (
            output_directory / "runs" / f"n-{configuration['n']:02d}"
        )
        attempt_directory = next_attempt_directory(configuration_directory)
        vector = tuple(
            configuration["protected_bits"][name]
            for name in endpoint_runner.TYPE_ORDER
        )
        runtime_configuration = endpoint_runner.Configuration(
            identifier=configuration["id"],
            label=f"Uniform saturated n={configuration['n']}",
            protected_bits=vector,
            n=configuration["n"],
        )
        try:
            local_entry = endpoint_runner.run_configuration(
                configuration=runtime_configuration,
                axpike=axpike,
                proxy_kernel=proxy_kernel,
                application=application,
                data_directory=data_directory,
                output_directory=attempt_directory,
                image_count=endpoint_runner.IMAGE_COUNT,
            )
            record_path = attempt_directory / local_entry["record"]
            run_entry = {
                "id": local_entry["id"],
                "record": str(record_path.relative_to(output_directory)),
                "sha256": local_entry["sha256"],
            }
            extracted = summarizer.extract_run(output_directory, run_entry)
            if (
                extracted["id"] != configuration["id"]
                or extracted["n"] != configuration["n"]
                or extracted["protected_bits"]
                != configuration["protected_bits"]
            ):
                raise ValueError("gated run does not match matrix configuration")

            gate = {
                "schema_version": 1,
                "status": "passed",
                "campaign_sha256": campaign_sha256,
                "configuration": {
                    "n": configuration["n"],
                    "id": configuration["id"],
                    "protected_bits": configuration["protected_bits"],
                },
                "run": run_entry,
                "invariants": PASSED_INVARIANTS,
            }
            gate_path = attempt_directory / "gate.json"
            write_json_atomic(gate_path, gate)
            completion = {
                "schema_version": 1,
                "status": "passed",
                "campaign_sha256": campaign_sha256,
                "configuration": gate["configuration"],
                "run": run_entry,
                "gate": {
                    "path": str(gate_path.relative_to(output_directory)),
                    "sha256": endpoint_runner.sha256(gate_path),
                },
            }
            write_json_atomic(
                configuration_directory / "completion.json", completion
            )
            completed_entries[configuration["id"]] = run_entry
            new_runs += 1
            write_json_atomic(
                output_directory / "progress.json",
                progress_record(
                    campaign_sha256=campaign_sha256,
                    configurations=configurations,
                    completed_ids=set(completed_entries),
                    status="running",
                ),
            )
            print(
                f"passed n={configuration['n']} "
                f"({len(completed_entries)}/{len(configurations)})",
                flush=True,
            )
        except Exception as error:
            failure = {
                "n": configuration["n"],
                "id": configuration["id"],
                "attempt": str(attempt_directory.relative_to(output_directory)),
                "error_type": type(error).__name__,
                "message": str(error),
            }
            write_json_atomic(attempt_directory / "failure.json", failure)
            write_json_atomic(
                output_directory / "progress.json",
                progress_record(
                    campaign_sha256=campaign_sha256,
                    configurations=configurations,
                    completed_ids=set(completed_entries),
                    status="failed",
                    failure=failure,
                ),
            )
            raise

    ordered_entries = [
        completed_entries[configuration["id"]]
        for configuration in configurations
        if configuration["id"] in completed_entries
    ]
    status = (
        "passed"
        if len(ordered_entries) == len(configurations)
        else "incomplete"
    )
    write_json_atomic(
        output_directory / "progress.json",
        progress_record(
            campaign_sha256=campaign_sha256,
            configurations=configurations,
            completed_ids=set(completed_entries),
            status=status,
        ),
    )
    return ordered_entries


def finalize_campaign(
    *,
    output_directory: Path,
    campaign: dict[str, Any],
    campaign_sha256: str,
    run_entries: list[dict[str, Any]],
) -> None:
    if len(run_entries) != EXPECTED_CONFIGURATION_COUNT:
        return
    run_manifest = {
        "schema_version": RUN_MANIFEST_SCHEMA_VERSION,
        "purpose": PURPOSE,
        "scope": campaign["scope"],
        "image_count": campaign["image_count"],
        "mode": campaign["mode"],
        "type_order": campaign["type_order"],
        "campaign": {
            "path": "campaign.json",
            "sha256": campaign_sha256,
        },
        "matrix": campaign["matrix"],
        "source_revisions": campaign["source_revisions"],
        "inputs": campaign["inputs"],
        "runs": run_entries,
    }
    manifest_path = output_directory / "run-manifest.json"
    write_json_atomic(manifest_path, run_manifest)
    summary = summarizer.build_summary(manifest_path)
    summary_directory = output_directory / "summary"
    summary_directory.mkdir(exist_ok=True)
    summary_path = summary_directory / "summary.json"
    summary_csv_path = summary_directory / "summary.csv"
    regions_csv_path = summary_directory / "regions.csv"
    regional_instructions_path = (
        summary_directory / "effective-types-by-region-instruction.csv"
    )
    write_json_atomic(summary_path, summary)
    summarizer.write_flat_csv(summary, summary_csv_path)
    summarizer.write_regions_csv(summary, regions_csv_path)
    summarizer.write_regional_instruction_csv(
        summary, regional_instructions_path
    )
    write_json_atomic(
        summary_directory / "summary-manifest.json",
        {
            "schema_version": RUN_MANIFEST_SCHEMA_VERSION,
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
    write_json_atomic(
        output_directory / "matrix-gate.json",
        {
            "schema_version": 1,
            "status": "passed",
            "campaign_sha256": campaign_sha256,
            "configuration_count": len(run_entries),
            "run_manifest_sha256": endpoint_runner.sha256(manifest_path),
            "summary_sha256": endpoint_runner.sha256(summary_path),
            "invariants": PASSED_INVARIANTS,
        },
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Run, resume, and gate all 51 uniform-n configurations on the "
            "deterministic 62-image LeNet fixture."
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
    matrix = read_json(matrix_path)
    configurations = validate_matrix(matrix)
    campaign = build_campaign(
        matrix_path=matrix_path,
        matrix=matrix,
        configurations=configurations,
        axpike=args.axpike,
        application=args.application,
        data_directory=args.data_directory,
        proxy_kernel=args.proxy_kernel,
    )
    campaign_path, campaign_sha256 = initialize_or_validate_campaign(
        args.output_directory, campaign
    )
    if campaign_path.parent != args.output_directory:
        raise ValueError("campaign path escaped the output directory")
    entries = run_pending_configurations(
        output_directory=args.output_directory,
        campaign_sha256=campaign_sha256,
        configurations=configurations,
        axpike=args.axpike,
        proxy_kernel=args.proxy_kernel,
        application=args.application,
        data_directory=args.data_directory,
        max_new_runs=args.max_new_runs,
    )
    finalize_campaign(
        output_directory=args.output_directory,
        campaign=campaign,
        campaign_sha256=campaign_sha256,
        run_entries=entries,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
