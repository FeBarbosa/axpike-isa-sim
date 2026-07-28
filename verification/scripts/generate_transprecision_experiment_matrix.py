#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from collections import OrderedDict
from dataclasses import dataclass
from fractions import Fraction
from pathlib import Path
from typing import Any


@dataclass(frozen=True)
class Transition:
    name: str
    maximum_protected_bits: int


TRANSITIONS = (
    Transition("fp32-e5m2", 21),
    Transition("fp32-fp16", 13),
    Transition("fp64-e5m2", 50),
    Transition("fp64-fp16", 42),
    Transition("fp64-fp32", 29),
)
EXACT_VECTOR = tuple(
    transition.maximum_protected_bits for transition in TRANSITIONS
)
PROTECTION_RATIOS = (
    ("1.00", Fraction(1, 1)),
    ("0.75", Fraction(3, 4)),
    ("0.50", Fraction(1, 2)),
    ("0.25", Fraction(1, 4)),
    ("0.00", Fraction(0, 1)),
)
EXPECTED_PARAMETERIZATION_POINTS = 216
EXPECTED_DYNAMIC_CONFIGURATIONS = 201
FIXED_CONTROLS = (
    "original-fp32-fp64",
    "fixed-fp16",
    "fixed-e5m2",
)

ProtectedBitsVector = tuple[int, ...]
Parameterization = dict[str, Any]


def ceil_fraction(value: Fraction) -> int:
    return (value.numerator + value.denominator - 1) // value.denominator


def vector_id(vector: ProtectedBitsVector) -> str:
    return "tp-" + "-".join(str(value) for value in vector)


def cli_argument(vector: ProtectedBitsVector) -> str:
    entries = (
        f"{transition.name}:{value}"
        for transition, value in zip(TRANSITIONS, vector)
    )
    return "--transprecision-protected-bits=" + ",".join(entries)


def protected_bits_mapping(
    vector: ProtectedBitsVector,
) -> OrderedDict[str, int]:
    return OrderedDict(
        (transition.name, value)
        for transition, value in zip(TRANSITIONS, vector)
    )


def validate_vector(vector: ProtectedBitsVector) -> None:
    if len(vector) != len(TRANSITIONS):
        raise ValueError("a protected-bit vector must contain five values")
    for transition, value in zip(TRANSITIONS, vector):
        if not 0 <= value <= transition.maximum_protected_bits:
            raise ValueError(
                f"{transition.name} protected width {value} is outside "
                f"0..{transition.maximum_protected_bits}"
            )


def add_configuration(
    configurations: OrderedDict[ProtectedBitsVector, list[Parameterization]],
    vector: ProtectedBitsVector,
    parameterization: Parameterization,
) -> None:
    validate_vector(vector)
    configurations.setdefault(vector, []).append(parameterization)


def generate_configurations(
) -> OrderedDict[ProtectedBitsVector, list[Parameterization]]:
    configurations: OrderedDict[
        ProtectedBitsVector, list[Parameterization]
    ] = OrderedDict()

    for ratio_text, ratio in PROTECTION_RATIOS:
        vector = tuple(
            ceil_fraction(ratio * transition.maximum_protected_bits)
            for transition in TRANSITIONS
        )
        add_configuration(
            configurations,
            vector,
            {
                "kind": "proportional",
                "protection_ratio": ratio_text,
            },
        )

    for transition_index, transition in enumerate(TRANSITIONS):
        for protected_bits in range(transition.maximum_protected_bits + 1):
            vector = list(EXACT_VECTOR)
            vector[transition_index] = protected_bits
            add_configuration(
                configurations,
                tuple(vector),
                {
                    "kind": "per-transition",
                    "transition": transition.name,
                    "protected_bits": protected_bits,
                },
            )

    largest_width = max(
        transition.maximum_protected_bits for transition in TRANSITIONS
    )
    for protected_bits in range(largest_width + 1):
        vector = tuple(
            min(protected_bits, transition.maximum_protected_bits)
            for transition in TRANSITIONS
        )
        add_configuration(
            configurations,
            vector,
            {
                "kind": "global-absolute",
                "protected_bits": protected_bits,
            },
        )

    return configurations


def validate_configurations(
    configurations: OrderedDict[
        ProtectedBitsVector, list[Parameterization]
    ],
) -> None:
    parameterization_points = sum(
        len(parameterizations)
        for parameterizations in configurations.values()
    )
    if parameterization_points != EXPECTED_PARAMETERIZATION_POINTS:
        raise ValueError(
            "the parameterizations produced "
            f"{parameterization_points} points; expected "
            f"{EXPECTED_PARAMETERIZATION_POINTS}"
        )
    if len(configurations) != EXPECTED_DYNAMIC_CONFIGURATIONS:
        raise ValueError(
            "deduplication produced "
            f"{len(configurations)} dynamic configurations; expected "
            f"{EXPECTED_DYNAMIC_CONFIGURATIONS}"
        )
    if EXACT_VECTOR not in configurations:
        raise ValueError("the exact baseline is missing")
    if tuple(0 for _ in TRANSITIONS) not in configurations:
        raise ValueError("the combined no-protection endpoint is missing")


def build_manifest() -> dict[str, Any]:
    configurations = generate_configurations()
    validate_configurations(configurations)

    entries = []
    for vector, parameterizations in configurations.items():
        entries.append(
            {
                "id": vector_id(vector),
                "protected_bits": protected_bits_mapping(vector),
                "cli_argument": cli_argument(vector),
                "parameterizations": parameterizations,
            }
        )

    return {
        "schema_version": 1,
        "transition_order": [
            transition.name for transition in TRANSITIONS
        ],
        "parameterization_point_count": EXPECTED_PARAMETERIZATION_POINTS,
        "dynamic_configuration_count": len(entries),
        "fixed_controls": list(FIXED_CONTROLS),
        "fixed_control_count": len(FIXED_CONTROLS),
        "full_application_run_count": len(entries) + len(FIXED_CONTROLS),
        "configurations": entries,
    }


def write_manifest(manifest: dict[str, Any], output: Path | None) -> None:
    serialized = json.dumps(manifest, indent=2, ensure_ascii=True) + "\n"
    if output is None:
        print(serialized, end="")
        return
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(serialized, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Generate and deduplicate the approved transprecision "
            "protected-bit experiment matrix."
        )
    )
    parser.add_argument(
        "--output",
        type=Path,
        help="Write the JSON manifest to this path instead of standard output.",
    )
    args = parser.parse_args()

    manifest = build_manifest()
    write_manifest(manifest, args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
