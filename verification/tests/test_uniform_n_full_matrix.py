from __future__ import annotations

import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


SCRIPTS = Path(__file__).resolve().parents[1] / "scripts"
sys.path.insert(0, str(SCRIPTS))

import prepare_full_mnist_fixture as fixture
import run_uniform_n_full_matrix as full_runner


class UniformNFullMatrixTest(unittest.TestCase):
    def test_idx_validation_checks_header_and_exact_size(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            images = root / "images"
            images.write_bytes(
                struct.pack(">IIII", fixture.IMAGE_MAGIC, 2, 28, 28)
                + bytes(2 * 28 * 28)
            )
            labels = root / "labels"
            labels.write_bytes(
                struct.pack(">II", fixture.LABEL_MAGIC, 2) + bytes((1, 9))
            )
            fixture.validate_idx(images, "images", 2)
            fixture.validate_idx(labels, "labels", 2)

            images.write_bytes(images.read_bytes() + b"unexpected")
            with self.assertRaisesRegex(ValueError, "unexpected IDX file size"):
                fixture.validate_idx(images, "images", 2)

    def test_reduced_gate_is_bound_to_full_campaign_executables(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            reduced_campaign_path = root / "campaign.json"
            revisions = {
                name: {"commit": f"{name}-commit", "dirty": False}
                for name in ("axpike", "adf", "application")
            }
            reduced_campaign = {
                "schema_version": 2,
                "purpose": full_runner.REDUCED_PURPOSE,
                "image_count": 62,
                "source_revisions": revisions,
                "matrix": {"sha256": "matrix"},
                "inputs": {
                    "axpike": {"sha256": "axpike-bin"},
                    "application": {"sha256": "application-bin"},
                    "proxy_kernel": "pk",
                },
            }
            full_runner.shared_runner.write_json_atomic(
                reduced_campaign_path, reduced_campaign
            )
            gate_path = root / "matrix-gate.json"
            gate = {
                "schema_version": 1,
                "status": "passed",
                "configuration_count": 51,
                "campaign_sha256": (
                    full_runner.endpoint_runner.sha256(reduced_campaign_path)
                ),
                "invariants": full_runner.shared_runner.PASSED_INVARIANTS,
            }
            full_runner.shared_runner.write_json_atomic(gate_path, gate)
            full_campaign = {
                "source_revisions": revisions,
                "matrix": {"sha256": "matrix"},
                "inputs": {
                    "axpike": {"sha256": "axpike-bin"},
                    "application": {"sha256": "application-bin"},
                    "proxy_kernel": "pk",
                },
            }

            prerequisite = full_runner.require_reduced_gate(
                gate_path, full_campaign
            )
            self.assertEqual(
                prerequisite["reduced_matrix_gate"]["sha256"],
                full_runner.endpoint_runner.sha256(gate_path),
            )

            changed = json.loads(json.dumps(full_campaign))
            changed["source_revisions"]["application"]["commit"] = "other"
            with self.assertRaisesRegex(
                ValueError, "different application source revisions"
            ):
                full_runner.require_reduced_gate(gate_path, changed)

    def test_full_main_forwards_scientific_identity_and_image_count(self) -> None:
        matrix = {"configurations": []}
        campaign = {
            "purpose": full_runner.PURPOSE,
            "scope": full_runner.SCOPE,
        }
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            arguments = [
                "run_uniform_n_full_matrix.py",
                "--matrix", str(root / "matrix.json"),
                "--axpike", str(root / "axpike"),
                "--application", str(root / "application"),
                "--data-directory", str(root / "data"),
                "--reduced-gate", str(root / "matrix-gate.json"),
                "--output-directory", str(root / "output"),
                "--max-new-runs", "0",
            ]
            with mock.patch.object(sys, "argv", arguments), mock.patch.object(
                full_runner.endpoint_runner, "require_file"
            ), mock.patch.object(
                full_runner, "require_fixture"
            ), mock.patch.object(
                full_runner.shared_runner,
                "read_json",
                return_value=matrix,
            ), mock.patch.object(
                full_runner.shared_runner,
                "validate_matrix",
                return_value=[],
            ), mock.patch.object(
                full_runner.shared_runner,
                "build_campaign",
                return_value=campaign,
            ) as build, mock.patch.object(
                full_runner,
                "require_reduced_gate",
                return_value={"reduced_matrix_gate": {}},
            ), mock.patch.object(
                full_runner.shared_runner,
                "initialize_or_validate_campaign",
                return_value=(root / "output/campaign.json", "hash"),
            ), mock.patch.object(
                full_runner.shared_runner,
                "run_pending_configurations",
                return_value=[],
            ) as run, mock.patch.object(
                full_runner.shared_runner, "finalize_campaign"
            ):
                self.assertEqual(full_runner.main(), 0)

            self.assertEqual(build.call_args.kwargs["image_count"], 10_000)
            self.assertEqual(build.call_args.kwargs["purpose"], campaign["purpose"])
            self.assertEqual(run.call_args.kwargs["image_count"], 10_000)


if __name__ == "__main__":
    unittest.main()
