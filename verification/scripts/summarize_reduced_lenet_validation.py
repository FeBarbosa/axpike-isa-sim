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
TYPE_ORDER = ("fp64", "fp32", "fp16", "e5m2")
POLICY_NAME = "effective-type-quantization-v4"
POLICY_VERSION = 4
QUANTIZATION_PAIRS = (
    ("FP32", "E5M2"),
    ("FP32", "FP16"),
    ("FP64", "E5M2"),
    ("FP64", "FP16"),
    ("FP64", "FP32"),
)
REGION_NAMES = {
    0: "unscoped",
    1: "input-normalization",
    2: "conv1-block",
    3: "conv2-block",
    4: "fc1",
    5: "fc2",
    6: "softmax",
    7: "argmax",
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, value: Any) -> None:
    path.write_text(
        json.dumps(value, indent=2, sort_keys=True, ensure_ascii=True) + "\n",
        encoding="utf-8",
    )


def normalize_csv_value(value: str | None) -> str:
    normalized = (value or "").strip()
    if len(normalized) >= 2 and normalized[0] == normalized[-1] == '"':
        normalized = normalized[1:-1]
    return normalized


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
                key: normalize_csv_value(value)
                for key, value in source_row.items()
            }
            try:
                row["Value"] = int(row["Value"])
            except ValueError as error:
                raise ValueError(f"non-integer counter row in {path}: {row}") \
                    from error
            rows.append(row)
    return rows


def parse_transprecision_sections_csv(
    path: Path,
) -> list[dict[str, str | int]]:
    rows: list[dict[str, str | int]] = []
    with path.open(newline="", encoding="utf-8") as input_file:
        reader = csv.DictReader(input_file)
        expected_fields = [
            "Section", "Category", "Instruction", "From", "To",
            "Type", "Class", "Value",
        ]
        if reader.fieldnames != expected_fields:
            raise ValueError(
                f"unexpected transprecision-sections CSV header in {path}"
            )
        for source_row in reader:
            try:
                row = {
                    key: normalize_csv_value(value)
                    for key, value in source_row.items()
                }
                row["Section"] = int(row["Section"])
                row["Value"] = int(row["Value"])
            except (TypeError, ValueError) as error:
                raise ValueError(
                    f"invalid regional counter row in {path}: {source_row}"
                ) from error
            rows.append(row)
    return rows


def verify_regional_partition(
    global_rows: list[dict[str, str | int]],
    regional_rows: list[dict[str, str | int]],
) -> None:
    dimensions = ("Category", "Instruction", "From", "To", "Type", "Class")
    global_values: dict[tuple[str | int, ...], int] = {}
    for row in global_rows:
        if str(row["Category"]).startswith("policy_"):
            continue
        key = tuple(row[name] for name in dimensions)
        global_values[key] = global_values.get(key, 0) + int(row["Value"])
    regional_values: dict[tuple[str | int, ...], int] = {}
    for row in regional_rows:
        key = tuple(row[name] for name in dimensions)
        regional_values[key] = regional_values.get(key, 0) + int(row["Value"])
    if global_values != regional_values:
        missing = sorted(set(global_values) ^ set(regional_values))
        mismatched = sorted(
            key for key in set(global_values) & set(regional_values)
            if global_values[key] != regional_values[key]
        )
        raise ValueError(
            "global transprecision counters do not equal the sum of regions; "
            f"different keys={missing[:3]}, mismatched keys={mismatched[:3]}"
        )


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


def summarize_region(
    region_id: int,
    rows: list[dict[str, str | int]],
) -> dict[str, Any]:
    effective_rows = [
        row for row in rows
        if row["Category"] == "effective_type_by_instruction"
    ]
    instructions = sorted({str(row["Instruction"]) for row in effective_rows})
    return {
        "id": region_id,
        "name": REGION_NAMES[region_id],
        "effective_types": {
            type_name: sum(
                int(row["Value"])
                for row in effective_rows
                if row["Type"] == type_name
            )
            for type_name in EFFECTIVE_TYPES
        },
        "effective_type_by_instruction": {
            instruction: {
                type_name: sum(
                    int(row["Value"])
                    for row in effective_rows
                    if row["Instruction"] == instruction
                    and row["Type"] == type_name
                )
                for type_name in EFFECTIVE_TYPES
            }
            for instruction in instructions
        },
        "event_totals": {
            category: sum_category(rows, category)
            for category in sorted({str(row["Category"]) for row in rows})
        },
        "quantization": {
            category: {
                f"{carrier.lower()}-{effective.lower()}": select_value(
                    rows, category, From=carrier, To=effective
                )
                for carrier, effective in QUANTIZATION_PAIRS
            }
            for category in (
                "result_quantization_total_from_to",
                "result_quantization_changed_from_to",
                "result_quantization_to_zero_from_to",
                "result_quantization_overflow_from_to",
                "result_quantization_underflow_from_to",
            )
        },
    }


def extract_run(
    manifest_directory: Path,
    run_entry: dict[str, Any],
) -> dict[str, Any]:
    record_path = manifest_directory / run_entry["record"]
    if sha256(record_path) != run_entry["sha256"]:
        raise ValueError(f"run-record hash mismatch: {record_path}")
    record = read_json(record_path)
    if record.get("schema_version") != 4:
        raise ValueError(f"unsupported run-record schema in {record_path}")
    if record["id"] != run_entry["id"]:
        raise ValueError(f"run identifier mismatch in {record_path}")

    run_directory = record_path.parent
    for artifact_name, artifact_entry in record["artifacts"].items():
        artifact_path = run_directory / artifact_entry["path"]
        if sha256(artifact_path) != artifact_entry["sha256"]:
            raise ValueError(
                f"{artifact_name} artifact hash mismatch: {artifact_path}"
            )
    artifact = record["artifacts"]["transprecision"]
    csv_path = run_directory / artifact["path"]
    rows = parse_transprecision_csv(csv_path)
    regional_artifact = record["artifacts"]["transprecision_sections"]
    regional_csv_path = run_directory / regional_artifact["path"]
    regional_rows = parse_transprecision_sections_csv(regional_csv_path)
    observed_regions = sorted({int(row["Section"]) for row in regional_rows})
    if observed_regions != list(REGION_NAMES):
        raise ValueError(
            f"{record['id']}: expected regions {list(REGION_NAMES)}, "
            f"observed {observed_regions}"
        )
    regions = [
        summarize_region(
            region_id,
            [row for row in regional_rows if row["Section"] == region_id],
        )
        for region_id in observed_regions
    ]

    policy_version = select_value(
        rows,
        "policy_version",
        Type=POLICY_NAME,
    )
    if policy_version != POLICY_VERSION:
        raise ValueError(
            f"{record['id']}: unsupported transprecision policy version "
            f"{policy_version}"
        )
    observed_policy = {
        type_name.lower(): select_value(
            rows,
            "policy_protected_bits",
            Type=type_name,
        )
        for type_name in ("FP64", "FP32", "FP16", "E5M2")
    }
    expected_policy = record["protected_bits"]
    if observed_policy != expected_policy:
        raise ValueError(
            f"{record['id']}: CSV policy {observed_policy} does not match "
            f"run record {expected_policy}"
        )
    if record.get("n") is not None:
        expected_argument = (
            "--transprecision-type-protected-bits="
            + ",".join(
                f"{name}:{expected_policy[name]}" for name in TYPE_ORDER
            )
        )
        if record.get("command", []).count(expected_argument) != 1:
            raise ValueError(
                f"{record['id']}: command does not contain its policy vector"
            )

    effective_rows = [
        row
        for row in rows
        if row["Category"] == "effective_type_by_instruction"
    ]
    if not effective_rows:
        raise ValueError(
            f"{record['id']}: no effective-type instruction observations"
        )
    effective_types = {
        effective_type: sum(
            int(row["Value"])
            for row in effective_rows
            if row["Type"] == effective_type
        )
        for effective_type in EFFECTIVE_TYPES
    }
    effective_observations = sum(effective_types.values())
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
        "result_tag_reduction_to_zero_total": select_value(
            rows, "result_tag_reduction_to_zero_total"
        ),
        "external_write_masked_to_zero_total": select_value(
            rows, "external_write_masked_to_zero_total"
        ),
        "invalid_result_promotion_total": select_value(
            rows, "invalid_result_promotion_total"
        ),
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
    if diagnostics["invalid_result_promotion_total"] != 0:
        raise ValueError(f"{record['id']}: invalid result promotions remain")
    if (
        diagnostics["fp64_load_nan_boxed_fp32_effective_total"]
        > diagnostics["external_nan_total"]
    ):
        raise ValueError(
            f"{record['id']}: effective boxed-FP32 count exceeds external NaNs"
        )

    quantization = {
        category: {
            f"{carrier.lower()}-{effective.lower()}": select_value(
                rows,
                category,
                From=carrier,
                To=effective,
            )
            for carrier, effective in QUANTIZATION_PAIRS
        }
        for category in (
            "result_quantization_total_from_to",
            "result_quantization_changed_from_to",
            "result_quantization_to_zero_from_to",
            "result_quantization_overflow_from_to",
            "result_quantization_underflow_from_to",
        )
    }
    quantization_total = sum(
        quantization["result_quantization_total_from_to"].values()
    )
    operation_result_total = sum_category(
        rows, "operation_result_class_total"
    )
    if quantization_total > operation_result_total:
        raise ValueError(
            f"{record['id']}: quantization events exceed operation results"
        )
    for pair in quantization["result_quantization_total_from_to"]:
        total = quantization["result_quantization_total_from_to"][pair]
        changed = quantization[
            "result_quantization_changed_from_to"][pair]
        to_zero = quantization[
            "result_quantization_to_zero_from_to"][pair]
        overflow = quantization[
            "result_quantization_overflow_from_to"][pair]
        underflow = quantization[
            "result_quantization_underflow_from_to"][pair]
        if (
            changed > total
            or overflow > changed
            or underflow > total
            or to_zero > changed
            or to_zero > underflow
            or overflow + underflow > total
        ):
            raise ValueError(
                f"{record['id']}: invalid quantization partition for {pair}"
            )

    transitions = {
        category: sum_category(rows, category)
        for category in (
            "operand_promotion_from_to",
            "result_tag_reduction_total_from_to",
            "result_tag_reduction_changed_from_to",
            "external_write_masked_from_to",
        )
    }
    for source_index, source in enumerate(("E5M2", "FP16", "FP32", "FP64")):
        for destination in ("E5M2", "FP16", "FP32", "FP64")[:source_index]:
            total = select_value(
                rows,
                "result_tag_reduction_total_from_to",
                From=source,
                To=destination,
            )
            changed = select_value(
                rows,
                "result_tag_reduction_changed_from_to",
                From=source,
                To=destination,
            )
            if changed > total:
                raise ValueError(
                    f"{record['id']}: changed tag reductions exceed total "
                    f"for {source.lower()}-{destination.lower()}"
                )
    if (
        transitions["result_tag_reduction_changed_from_to"]
        > transitions["result_tag_reduction_total_from_to"]
    ):
        raise ValueError(
            f"{record['id']}: changed tag reductions exceed total reductions"
        )
    verify_regional_partition(rows, regional_rows)
    accuracy = outcome["correct"] / outcome["processed"]
    return {
        "id": record["id"],
        "n": record.get("n"),
        "label": record["label"],
        "mode": record["mode"],
        "protected_bits": expected_policy,
        "outcome": outcome,
        "accuracy": accuracy,
        "effective_type_observations": effective_observations,
        "effective_types": effective_types,
        "diagnostics": diagnostics,
        "quantization": quantization,
        "transitions": transitions,
        "regions": regions,
        "source": {
            "run_record": str(record_path),
            "run_record_sha256": run_entry["sha256"],
            "transprecision_csv": str(csv_path),
            "transprecision_csv_sha256": artifact["sha256"],
            "transprecision_sections_csv": str(regional_csv_path),
            "transprecision_sections_csv_sha256": regional_artifact["sha256"],
        },
    }


def build_summary(manifest_path: Path) -> dict[str, Any]:
    manifest = read_json(manifest_path)
    schema_version = manifest.get("schema_version")
    if schema_version not in (3, 4):
        raise ValueError("unsupported reduced-validation manifest schema")
    purpose = manifest.get("purpose")
    supported_purposes = {
        "reduced deterministic LeNet validation",
        "uniform-n reduced deterministic LeNet validation",
        "uniform-n complete MNIST LeNet scientific evaluation",
    }
    if purpose not in supported_purposes:
        raise ValueError("manifest is not a supported LeNet matrix manifest")
    if tuple(manifest.get("type_order", ())) != TYPE_ORDER:
        raise ValueError("unexpected protected-bit type order")
    runs = [
        extract_run(manifest_path.parent, run_entry)
        for run_entry in manifest["runs"]
    ]
    if purpose == "reduced deterministic LeNet validation":
        if [run["id"] for run in runs] != [
            "full-protection",
            "no-protection",
        ]:
            raise ValueError(
                "expected full-protection and no-protection runs in that order"
            )
    else:
        if schema_version != 4:
            raise ValueError("uniform-n matrix requires manifest schema 4")
        if len(runs) != 51 or [run["n"] for run in runs] != list(range(51)):
            raise ValueError("uniform-n matrix must contain n=0..50")
        limits = (50, 21, 8, 0)
        for run in runs:
            expected = {
                name: min(run["n"], limit)
                for name, limit in zip(TYPE_ORDER, limits)
            }
            if run["protected_bits"] != expected:
                raise ValueError(
                    f"{run['id']}: protected bits do not match uniform n"
                )
    if any(
        run["outcome"]["processed"] != manifest["image_count"]
        for run in runs
    ):
        raise ValueError("manifest/run image-count mismatch")
    if any(run["mode"] != manifest["mode"] for run in runs):
        raise ValueError("manifest/run mode mismatch")
    invariants = {
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
    return {
        "schema_version": schema_version,
        "purpose": manifest["purpose"],
        "scope": manifest["scope"],
        "image_count": manifest["image_count"],
        "mode": manifest["mode"],
        "type_order": manifest["type_order"],
        "source_manifest": {
            "path": str(manifest_path.resolve()),
            "sha256": sha256(manifest_path),
        },
        "runs": runs,
        "invariants": invariants,
    }


def write_flat_csv(summary: dict[str, Any], path: Path) -> None:
    fields = [
        "id",
        "n",
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
        "result_quantization_total",
        "result_quantization_changed",
        "result_quantization_to_zero",
        "result_quantization_overflow",
        "result_quantization_underflow",
        "operand_promotion_total",
        "result_tag_reduction_total",
        "result_tag_reduction_changed",
        "external_write_masked_total",
        "external_nan_total",
        "operation_nan_total",
        "fp64_load_nan_boxed_fp32_effective_total",
        "operand_unclassified_total",
        "result_tag_reduction_to_zero_total",
        "external_write_masked_to_zero_total",
        "invalid_result_promotion_total",
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
                    "n": "" if run["n"] is None else run["n"],
                    "label": run["label"],
                    **run["outcome"],
                    "accuracy": f"{run['accuracy']:.12f}",
                    "effective_type_observations":
                        run["effective_type_observations"],
                    "effective_e5m2": run["effective_types"]["E5M2"],
                    "effective_fp16": run["effective_types"]["FP16"],
                    "effective_fp32": run["effective_types"]["FP32"],
                    "effective_fp64": run["effective_types"]["FP64"],
                    "result_quantization_total": sum(
                        run["quantization"][
                            "result_quantization_total_from_to"
                        ].values()
                    ),
                    "result_quantization_changed": sum(
                        run["quantization"][
                            "result_quantization_changed_from_to"
                        ].values()
                    ),
                    "result_quantization_to_zero": sum(
                        run["quantization"][
                            "result_quantization_to_zero_from_to"
                        ].values()
                    ),
                    "result_quantization_overflow": sum(
                        run["quantization"][
                            "result_quantization_overflow_from_to"
                        ].values()
                    ),
                    "result_quantization_underflow": sum(
                        run["quantization"][
                            "result_quantization_underflow_from_to"
                        ].values()
                    ),
                    "operand_promotion_total": run["transitions"][
                        "operand_promotion_from_to"
                    ],
                    "result_tag_reduction_total": run["transitions"][
                        "result_tag_reduction_total_from_to"
                    ],
                    "result_tag_reduction_changed": run["transitions"][
                        "result_tag_reduction_changed_from_to"
                    ],
                    "external_write_masked_total": run["transitions"][
                        "external_write_masked_from_to"
                    ],
                    **run["diagnostics"],
                }
            )


def write_regions_csv(summary: dict[str, Any], path: Path) -> None:
    fields = [
        "id", "n", "region_id", "region_name",
        "effective_e5m2", "effective_fp16", "effective_fp32",
        "effective_fp64", "effective_unclassified",
        "operand_promotion_total", "result_quantization_total",
        "result_quantization_changed", "result_quantization_to_zero",
        "result_quantization_overflow", "result_quantization_underflow",
        "result_tag_reduction_total", "result_tag_reduction_changed",
        "external_write_masked_total", "operation_result_total",
        "external_write_total",
    ]
    with path.open("w", newline="", encoding="utf-8") as output_file:
        writer = csv.DictWriter(output_file, fieldnames=fields)
        writer.writeheader()
        for run in summary["runs"]:
            for region in run["regions"]:
                events = region["event_totals"]
                quantization = region["quantization"]
                writer.writerow({
                    "id": run["id"],
                    "n": "" if run["n"] is None else run["n"],
                    "region_id": region["id"],
                    "region_name": region["name"],
                    **{
                        f"effective_{type_name.lower()}": value
                        for type_name, value in region["effective_types"].items()
                    },
                    "operand_promotion_total": events[
                        "operand_promotion_from_to"
                    ],
                    "result_quantization_total": sum(quantization[
                        "result_quantization_total_from_to"
                    ].values()),
                    "result_quantization_changed": sum(quantization[
                        "result_quantization_changed_from_to"
                    ].values()),
                    "result_quantization_to_zero": sum(quantization[
                        "result_quantization_to_zero_from_to"
                    ].values()),
                    "result_quantization_overflow": sum(quantization[
                        "result_quantization_overflow_from_to"
                    ].values()),
                    "result_quantization_underflow": sum(quantization[
                        "result_quantization_underflow_from_to"
                    ].values()),
                    "result_tag_reduction_total": events[
                        "result_tag_reduction_total_from_to"
                    ],
                    "result_tag_reduction_changed": events[
                        "result_tag_reduction_changed_from_to"
                    ],
                    "external_write_masked_total": events[
                        "external_write_masked_from_to"
                    ],
                    "operation_result_total": events[
                        "operation_result_class_total"
                    ],
                    "external_write_total": events[
                        "external_write_class_total"
                    ],
                })


def write_regional_instruction_csv(
    summary: dict[str, Any], path: Path,
) -> None:
    fields = [
        "id", "n", "region_id", "region_name", "instruction", "type",
        "value",
    ]
    with path.open("w", newline="", encoding="utf-8") as output_file:
        writer = csv.DictWriter(output_file, fieldnames=fields)
        writer.writeheader()
        for run in summary["runs"]:
            for region in run["regions"]:
                for instruction, types in region[
                    "effective_type_by_instruction"
                ].items():
                    for type_name, value in types.items():
                        if value == 0:
                            continue
                        writer.writerow({
                            "id": run["id"],
                            "n": "" if run["n"] is None else run["n"],
                            "region_id": region["id"],
                            "region_name": region["name"],
                            "instruction": instruction,
                            "type": type_name,
                            "value": value,
                        })


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
    write_regions_csv(summary, args.output_directory / "regions.csv")
    write_regional_instruction_csv(
        summary,
        args.output_directory / "effective-types-by-region-instruction.csv",
    )
    write_json(
        args.output_directory / "summary-manifest.json",
        {
            "schema_version": summary["schema_version"],
            "source_manifest_sha256":
                summary["source_manifest"]["sha256"],
            "artifacts": {
                "summary.json": sha256(summary_path),
                "summary.csv": sha256(
                    args.output_directory / "summary.csv"
                ),
                "regions.csv": sha256(
                    args.output_directory / "regions.csv"
                ),
                "effective-types-by-region-instruction.csv": sha256(
                    args.output_directory
                    / "effective-types-by-region-instruction.csv"
                ),
            },
        },
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
