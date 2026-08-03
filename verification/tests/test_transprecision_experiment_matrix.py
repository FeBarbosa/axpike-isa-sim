from __future__ import annotations

import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPT = (
    Path(__file__).resolve().parents[1]
    / "scripts"
    / "generate_transprecision_experiment_matrix.py"
)
SPEC = importlib.util.spec_from_file_location(
    "generate_transprecision_experiment_matrix", SCRIPT
)
assert SPEC is not None
assert SPEC.loader is not None
matrix = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = matrix
SPEC.loader.exec_module(matrix)


class TransprecisionExperimentMatrixTest(unittest.TestCase):
    def setUp(self) -> None:
        self.manifest = matrix.build_manifest()
        self.configurations = {
            tuple(entry["protected_bits"].values()): entry
            for entry in self.manifest["configurations"]
        }

    def test_schema_policy_scope_and_expected_counts(self) -> None:
        self.assertEqual(self.manifest["schema_version"], 3)
        self.assertEqual(
            self.manifest["policy"],
            {
                "name": "effective-type-quantization-v4",
                "version": 4,
            },
        )
        self.assertEqual(
            self.manifest["type_order"],
            ["fp64", "fp32", "fp16", "e5m2"],
        )
        self.assertEqual(
            self.manifest["parameterization"],
            {
                "kind": "uniform-saturated",
                "minimum_n": 0,
                "maximum_n": 50,
                "rule": "effective_n(type)=min(n,maximum_n(type))",
            },
        )
        self.assertEqual(self.manifest["dynamic_configuration_count"], 51)
        self.assertEqual(self.manifest["planned_new_execution_count"], 51)
        self.assertEqual(self.manifest["reference_control_count"], 1)
        self.assertEqual(self.manifest["planned_evaluation_point_count"], 52)
        self.assertEqual(len(self.configurations), 51)
        self.assertEqual(
            self.manifest["reference_controls"],
            [{
                "id": "original-fp32-fp64",
                "source": "historical-artifact",
                "status": "provenance-pending",
            }],
        )

    def test_vectors_match_independent_uniform_reference(self) -> None:
        limits = (50, 21, 8, 0)
        for n, entry in enumerate(self.manifest["configurations"]):
            vector = tuple(entry["protected_bits"].values())
            self.assertEqual(entry["n"], n)
            self.assertEqual(
                vector, tuple(min(n, limit) for limit in limits)
            )
            self.assertEqual(
                entry["parameterizations"],
                [{"kind": "uniform-saturated", "n": n}],
            )

    def test_regime_breakpoints_and_endpoints(self) -> None:
        expected = {
            0: (0, 0, 0, 0),
            8: (8, 8, 8, 0),
            9: (9, 9, 8, 0),
            21: (21, 21, 8, 0),
            22: (22, 21, 8, 0),
            50: (50, 21, 8, 0),
        }
        for n, vector in expected.items():
            entry = self.manifest["configurations"][n]
            self.assertEqual(tuple(entry["protected_bits"].values()), vector)

    def test_cli_arguments_and_ids_are_complete_and_canonical(self) -> None:
        expected_names = ("fp64", "fp32", "fp16", "e5m2")
        identifiers = set()
        for vector, entry in self.configurations.items():
            expected_entries = ",".join(
                f"{name}:{value}"
                for name, value in zip(expected_names, vector)
            )
            self.assertEqual(
                entry["cli_argument"],
                "--transprecision-type-protected-bits="
                + expected_entries,
            )
            self.assertEqual(entry["id"], matrix.vector_id(vector))
            identifiers.add(entry["id"])
        self.assertEqual(len(identifiers), len(self.configurations))

    def test_invalid_vectors_are_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "four values"):
            matrix.validate_vector((50, 21, 8))
        with self.assertRaisesRegex(ValueError, "fp64"):
            matrix.validate_vector((51, 21, 8, 0))
        with self.assertRaisesRegex(ValueError, "e5m2"):
            matrix.validate_vector((50, 21, 8, 1))

    def test_written_manifest_is_deterministic(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            first = Path(temporary_directory) / "first.json"
            second = Path(temporary_directory) / "second.json"
            matrix.write_manifest(matrix.build_manifest(), first)
            matrix.write_manifest(matrix.build_manifest(), second)
            self.assertEqual(first.read_bytes(), second.read_bytes())
            self.assertEqual(
                json.loads(first.read_text(encoding="utf-8")),
                self.manifest,
            )


if __name__ == "__main__":
    unittest.main()
