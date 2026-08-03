#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from collections import OrderedDict
from dataclasses import dataclass
from pathlib import Path
from typing import Any


@dataclass(frozen=True)
class TypeParameter:
    name: str
    maximum_protected_bits: int


TYPE_PARAMETERS = (
    TypeParameter("fp64", 50),
    TypeParameter("fp32", 21),
    TypeParameter("fp16", 8),
    TypeParameter("e5m2", 0),
)
FULL_PROTECTION_VECTOR = tuple(
    parameter.maximum_protected_bits for parameter in TYPE_PARAMETERS
)
MINIMUM_UNIFORM_N = 0
MAXIMUM_UNIFORM_N = 50
EXPECTED_DYNAMIC_CONFIGURATIONS = 51
POLICY_NAME = "effective-type-quantization-v4"
POLICY_VERSION = 4
MANIFEST_SCHEMA_VERSION = 3
REFERENCE_CONTROLS = (
    {
        "id": "original-fp32-fp64",
        "source": "historical-artifact",
        "status": "provenance-pending",
    },
)

ProtectedBitsVector = tuple[int, ...]
Parameterization = dict[str, Any]


def vector_id(vector: ProtectedBitsVector) -> str:
    validate_vector(vector)
    entries = (
        f"{parameter.name}-{value}"
        for parameter, value in zip(TYPE_PARAMETERS, vector)
    )
    return "tp-" + "-".join(entries)


def cli_argument(vector: ProtectedBitsVector) -> str:
    validate_vector(vector)
    entries = (
        f"{parameter.name}:{value}"
        for parameter, value in zip(TYPE_PARAMETERS, vector)
    )
    return "--transprecision-type-protected-bits=" + ",".join(entries)


def protected_bits_mapping(
    vector: ProtectedBitsVector,
) -> OrderedDict[str, int]:
    validate_vector(vector)
    return OrderedDict(
        (parameter.name, value)
        for parameter, value in zip(TYPE_PARAMETERS, vector)
    )


def validate_vector(vector: ProtectedBitsVector) -> None:
    if len(vector) != len(TYPE_PARAMETERS):
        raise ValueError("a protected-bit vector must contain four values")
    for parameter, value in zip(TYPE_PARAMETERS, vector):
        if not 0 <= value <= parameter.maximum_protected_bits:
            raise ValueError(
                f"{parameter.name} protected width {value} is outside "
                f"0..{parameter.maximum_protected_bits}"
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

    for n in range(MINIMUM_UNIFORM_N, MAXIMUM_UNIFORM_N + 1):
        vector = tuple(
            min(n, parameter.maximum_protected_bits)
            for parameter in TYPE_PARAMETERS
        )
        add_configuration(
            configurations,
            vector,
            {"kind": "uniform-saturated", "n": n},
        )

    return configurations


def validate_configurations(
    configurations: OrderedDict[
        ProtectedBitsVector, list[Parameterization]
    ],
) -> None:
    if len(configurations) != EXPECTED_DYNAMIC_CONFIGURATIONS:
        raise ValueError(
            "the uniform sweep produced "
            f"{len(configurations)} dynamic configurations; expected "
            f"{EXPECTED_DYNAMIC_CONFIGURATIONS}"
        )

    for expected_n, (vector, parameterizations) in enumerate(
        configurations.items(), start=MINIMUM_UNIFORM_N
    ):
        expected_vector = tuple(
            min(expected_n, parameter.maximum_protected_bits)
            for parameter in TYPE_PARAMETERS
        )
        expected_parameterization = [
            {"kind": "uniform-saturated", "n": expected_n}
        ]
        if (vector != expected_vector
                or parameterizations != expected_parameterization):
            raise ValueError(
                f"configuration n={expected_n} does not match the "
                "uniform saturated definition"
            )

    if FULL_PROTECTION_VECTOR not in configurations:
        raise ValueError("the full-protection endpoint is missing")
    if tuple(0 for _ in TYPE_PARAMETERS) not in configurations:
        raise ValueError("the combined no-protection endpoint is missing")


def build_manifest() -> dict[str, Any]:
    configurations = generate_configurations()
    validate_configurations(configurations)

    entries = []
    for vector, parameterizations in configurations.items():
        entries.append(
            {
                "n": parameterizations[0]["n"],
                "id": vector_id(vector),
                "protected_bits": protected_bits_mapping(vector),
                "cli_argument": cli_argument(vector),
                "parameterizations": parameterizations,
            }
        )

    return {
        "schema_version": MANIFEST_SCHEMA_VERSION,
        "policy": {
            "name": POLICY_NAME,
            "version": POLICY_VERSION,
        },
        "type_order": [
            parameter.name for parameter in TYPE_PARAMETERS
        ],
        "parameterization": {
            "kind": "uniform-saturated",
            "minimum_n": MINIMUM_UNIFORM_N,
            "maximum_n": MAXIMUM_UNIFORM_N,
            "rule": "effective_n(type)=min(n,maximum_n(type))",
        },
        "dynamic_configuration_count": len(entries),
        "planned_new_execution_count": len(entries),
        "reference_controls": list(REFERENCE_CONTROLS),
        "reference_control_count": len(REFERENCE_CONTROLS),
        "planned_evaluation_point_count": (
            len(entries) + len(REFERENCE_CONTROLS)
        ),
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
            "Generate the approved uniform saturated transprecision "
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
