#!/usr/bin/env python3
"""Extract reduced-LeNet metrics and enforce counter invariants."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
from pathlib import Path
from typing import Any


EFFECTIVE_TYPES = ("E5M2", "FP16", "FP32", "FP64", "UNCLASSIFIED")
TRANSITION_ORDER = (
    "fp32-e5m2",
    "fp32-fp16",
    "fp64-e5m2",
    "fp64-fp16",
    "fp64-fp32",
)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, value: Any) -> None:
    path.write_text(
        json.dumps(value, indent=2, sort_keys=True, ensure_ascii=True) + "\n",
        encoding="utf-8",
    )


def parse_transprecision_csv(path: Path) -> list[dict[str, str | int]]:
    rows: list[dict[str, str | int]] = []
    with path.open(newline="", encoding="utf-8") as input_file:
        reader = csv.DictReader(input_file)
        expected = {
            "Category",
            "Instruction",
            "From",
            "To",
            "Type",
            "Class",
            "Value",
        }
        if set(reader.fieldnames or ()) != expected:
            raise ValueError(f"unexpected transprecision CSV header in {path}")
        for source_row in reader:
            row = {
                key: (value or "").strip()
                for key, value in source_row.items()
            }
            try:
                row["Value"] = int(row["Value"])
            except ValueError as error:
                raise ValueError(f"non-integer counter row in {path}: {row}") \
                    from error
            rows.append(row)
    return rows


def select_value(
    rows: list[dict[str, str | int]],
    category: str,
    **dimensions: str,
) -> int:
    matches = [
        row
        for row in rows
        if row["Category"] == category
        and all(row[dimension] == value for dimension, value in dimensions.items())
    ]
    if len(matches) != 1:
        raise ValueError(
            f"expected one {category} row with {dimensions}, "
            f"observed {len(matches)}"
        )
    return int(matches[0]["Value"])


def sum_category(
    rows: list[dict[str, str | int]],
    category: str,
) -> int:
    return sum(
        int(row["Value"])
        for row in rows
        if row["Category"] == category
    )


def extract_run(
    manifest_directory: Path,
    run_entry: dict[str, Any],
) -> dict[str, Any]:
    record_path = manifest_directory / run_entry["record"]
    if sha256(record_path) != run_entry["sha256"]:
        raise ValueError(f"run-record hash mismatch: {record_path}")
    record = read_json(record_path)
    if record["id"] != run_entry["id"]:
        raise ValueError(f"run identifier mismatch in {record_path}")

    run_directory = record_path.parent
    artifact = record["artifacts"]["transprecision"]
    csv_path = run_directory / artifact["path"]
    if sha256(csv_path) != artifact["sha256"]:
        raise ValueError(f"transprecision CSV hash mismatch: {csv_path}")
    rows = parse_transprecision_csv(csv_path)

    observed_policy = {
        f"{source.lower()}-{target.lower()}": select_value(
            rows,
            "policy_protected_bits",
            From=source,
            To=target,
        )
        for source, target in (
            ("FP32", "E5M2"),
            ("FP32", "FP16"),
            ("FP64", "E5M2"),
            ("FP64", "FP16"),
            ("FP64", "FP32"),
        )
    }
    expected_policy = record["protected_bits"]
    if observed_policy != expected_policy:
        raise ValueError(
            f"{record['id']}: CSV policy {observed_policy} does not match "
            f"run record {expected_policy}"
        )

    effective_types = {
        effective_type: select_value(
            rows,
            "effective_type_total",
            Type=effective_type,
        )
        for effective_type in EFFECTIVE_TYPES
    }
    effective_observations = select_value(
        rows,
        "transprecision_effective_type_observations",
    )
    if sum(effective_types.values()) != effective_observations:
        raise ValueError(
            f"{record['id']}: effective-type sum does not match observations"
        )
    if effective_types["UNCLASSIFIED"] != 0:
        raise ValueError(f"{record['id']}: unclassified effective types remain")

    outcome = record["outcome"]
    if outcome["processed"] != record["image_count"]:
        raise ValueError(f"{record['id']}: processed count mismatch")
    if outcome["correct"] + outcome["errors"] != outcome["processed"]:
        raise ValueError(f"{record['id']}: outcome count invariant failed")

    diagnostics = {
        "operand_unclassified_total": select_value(
            rows, "operand_unclassified_total"
        ),
        "masked_to_zero_total": select_value(rows, "masked_to_zero_total"),
        "lazy_reclassification_total": select_value(
            rows, "lazy_reclassification_total"
        ),
        "unclassified_fallback_total": select_value(
            rows, "unclassified_fallback_total"
        ),
        "fp64_load_nan_boxed_fp32_effective_total": select_value(
            rows, "fp64_load_nan_boxed_fp32_effective_total"
        ),
        "external_nan_total": select_value(
            rows,
            "external_write_class_total",
            Class="NAN",
        ),
        "operation_nan_total": select_value(
            rows,
            "operation_result_class_total",
            Class="NAN",
        ),
    }
    if diagnostics["operand_unclassified_total"] != 0:
        raise ValueError(f"{record['id']}: unclassified operands remain")
    if diagnostics["unclassified_fallback_total"] != 0:
        raise ValueError(f"{record['id']}: fallback events remain")
    if (
        diagnostics["fp64_load_nan_boxed_fp32_effective_total"]
        > diagnostics["external_nan_total"]
    ):
        raise ValueError(
            f"{record['id']}: effective boxed-FP32 count exceeds external NaNs"
        )

    transitions = {
        category: sum_category(rows, category)
        for category in (
            "operand_promotion_from_to",
            "result_promotion_from_to",
            "result_demotion_exact_from_to",
            "result_demotion_masked_from_to",
            "external_write_masked_from_to",
        )
    }
    accuracy = outcome["correct"] / outcome["processed"]
    return {
        "id": record["id"],
        "label": record["label"],
        "protected_bits": expected_policy,
        "outcome": outcome,
        "accuracy": accuracy,
        "effective_type_observations": effective_observations,
        "effective_types": effective_types,
        "diagnostics": diagnostics,
        "transitions": transitions,
        "source": {
            "run_record": str(record_path),
            "run_record_sha256": run_entry["sha256"],
            "transprecision_csv": str(csv_path),
            "transprecision_csv_sha256": artifact["sha256"],
        },
    }


def build_summary(manifest_path: Path) -> dict[str, Any]:
    manifest = read_json(manifest_path)
    if manifest.get("purpose") != "reduced deterministic LeNet validation":
        raise ValueError("manifest is not a reduced LeNet validation manifest")
    runs = [
        extract_run(manifest_path.parent, run_entry)
        for run_entry in manifest["runs"]
    ]
    if [run["id"] for run in runs] != ["exact", "no-protection"]:
        raise ValueError("expected exact and no-protection runs in that order")
    if any(
        run["outcome"]["processed"] != manifest["image_count"]
        for run in runs
    ):
        raise ValueError("manifest/run image-count mismatch")
    return {
        "schema_version": 1,
        "purpose": manifest["purpose"],
        "scope": manifest["scope"],
        "image_count": manifest["image_count"],
        "mode": manifest["mode"],
        "transition_order": manifest["transition_order"],
        "source_manifest": {
            "path": str(manifest_path.resolve()),
            "sha256": sha256(manifest_path),
        },
        "runs": runs,
        "invariants": {
            "outcome_partition": "passed",
            "policy_matches_command": "passed",
            "effective_type_partition": "passed",
            "unclassified_effective_types_zero": "passed",
            "unclassified_operands_zero": "passed",
            "fallback_events_zero": "passed",
            "effective_boxed_fp32_not_above_external_nan": "passed",
        },
    }


def write_flat_csv(summary: dict[str, Any], path: Path) -> None:
    fields = [
        "id",
        "label",
        "processed",
        "correct",
        "errors",
        "accuracy",
        "effective_type_observations",
        "effective_e5m2",
        "effective_fp16",
        "effective_fp32",
        "effective_fp64",
        "external_nan_total",
        "operation_nan_total",
        "fp64_load_nan_boxed_fp32_effective_total",
        "operand_unclassified_total",
        "masked_to_zero_total",
        "lazy_reclassification_total",
        "unclassified_fallback_total",
    ]
    with path.open("w", newline="", encoding="utf-8") as output_file:
        writer = csv.DictWriter(output_file, fieldnames=fields)
        writer.writeheader()
        for run in summary["runs"]:
            writer.writerow(
                {
                    "id": run["id"],
                    "label": run["label"],
                    **run["outcome"],
                    "accuracy": f"{run['accuracy']:.12f}",
                    "effective_type_observations":
                        run["effective_type_observations"],
                    "effective_e5m2": run["effective_types"]["E5M2"],
                    "effective_fp16": run["effective_types"]["FP16"],
                    "effective_fp32": run["effective_types"]["FP32"],
                    "effective_fp64": run["effective_types"]["FP64"],
                    **run["diagnostics"],
                }
            )


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Extract reduced-LeNet metrics and check instrumentation "
            "invariants."
        )
    )
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output-directory", type=Path, required=True)
    args = parser.parse_args()

    if args.output_directory.exists():
        raise ValueError(
            "output directory already exists; choose a new directory"
        )
    args.output_directory.mkdir(parents=True)
    summary = build_summary(args.manifest.resolve())
    summary_path = args.output_directory / "summary.json"
    write_json(summary_path, summary)
    write_flat_csv(summary, args.output_directory / "summary.csv")
    write_json(
        args.output_directory / "summary-manifest.json",
        {
            "schema_version": 1,
            "source_manifest_sha256":
                summary["source_manifest"]["sha256"],
            "artifacts": {
                "summary.json": sha256(summary_path),
                "summary.csv": sha256(
                    args.output_directory / "summary.csv"
                ),
            },
        },
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
