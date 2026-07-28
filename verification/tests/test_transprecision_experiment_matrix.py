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

    def test_expected_counts(self) -> None:
        self.assertEqual(
            self.manifest["parameterization_point_count"], 216
        )
        self.assertEqual(
            self.manifest["dynamic_configuration_count"], 201
        )
        self.assertEqual(self.manifest["fixed_control_count"], 3)
        self.assertEqual(self.manifest["full_application_run_count"], 204)
        self.assertEqual(len(self.configurations), 201)
        self.assertEqual(
            sum(
                len(entry["parameterizations"])
                for entry in self.manifest["configurations"]
            ),
            216,
        )

    def test_proportional_vectors_use_exact_ceiling(self) -> None:
        expected = {
            "1.00": (21, 13, 50, 42, 29),
            "0.75": (16, 10, 38, 32, 22),
            "0.50": (11, 7, 25, 21, 15),
            "0.25": (6, 4, 13, 11, 8),
            "0.00": (0, 0, 0, 0, 0),
        }
        observed = {}
        for vector, entry in self.configurations.items():
            for parameterization in entry["parameterizations"]:
                if parameterization["kind"] == "proportional":
                    observed[
                        parameterization["protection_ratio"]
                    ] = vector
        self.assertEqual(observed, expected)

    def test_per_transition_sweeps_cover_every_width(self) -> None:
        observed = {
            transition.name: set() for transition in matrix.TRANSITIONS
        }
        for entry in self.manifest["configurations"]:
            for parameterization in entry["parameterizations"]:
                if parameterization["kind"] == "per-transition":
                    observed[parameterization["transition"]].add(
                        parameterization["protected_bits"]
                    )
        for transition in matrix.TRANSITIONS:
            self.assertEqual(
                observed[transition.name],
                set(range(transition.maximum_protected_bits + 1)),
            )

    def test_global_sweep_uses_saturated_absolute_width(self) -> None:
        observed = {}
        for vector, entry in self.configurations.items():
            for parameterization in entry["parameterizations"]:
                if parameterization["kind"] == "global-absolute":
                    observed[parameterization["protected_bits"]] = vector

        self.assertEqual(set(observed), set(range(51)))
        for protected_bits, vector in observed.items():
            self.assertEqual(
                vector,
                tuple(
                    min(
                        protected_bits,
                        transition.maximum_protected_bits,
                    )
                    for transition in matrix.TRANSITIONS
                ),
            )

    def test_equivalent_endpoints_are_deduplicated_with_aliases(self) -> None:
        exact = self.configurations[(21, 13, 50, 42, 29)]
        self.assertEqual(len(exact["parameterizations"]), 7)
        no_protection = self.configurations[(0, 0, 0, 0, 0)]
        self.assertEqual(len(no_protection["parameterizations"]), 2)

    def test_cli_arguments_are_complete_and_canonical(self) -> None:
        for vector, entry in self.configurations.items():
            expected_entries = ",".join(
                f"{transition.name}:{value}"
                for transition, value in zip(matrix.TRANSITIONS, vector)
            )
            self.assertEqual(
                entry["cli_argument"],
                "--transprecision-protected-bits=" + expected_entries,
            )
            self.assertEqual(entry["id"], matrix.vector_id(vector))

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
