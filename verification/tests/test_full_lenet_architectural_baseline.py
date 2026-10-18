from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


SCRIPTS = Path(__file__).resolve().parents[1] / "scripts"
sys.path.insert(0, str(SCRIPTS))

import run_full_lenet_architectural_baseline as baseline_runner


class FullLenetArchitecturalBaselineTest(unittest.TestCase):
    def test_configuration_is_distinct_from_uniform_n(self) -> None:
        self.assertEqual(
            baseline_runner.CONFIGURATION,
            {
                "id": "original-fp32-fp64",
                "label": "Architectural FP32/FP64 baseline",
                "n": None,
                "transprecision_mode": "disabled",
                "protected_bits": None,
                "cli_argument": "--transprecision-mode=disabled",
            },
        )

    def test_campaign_records_inputs_and_disabled_mode(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            axpike = root / "axpike"
            application = root / "app"
            data = root / "data"
            axpike.write_bytes(b"axpike")
            application.write_bytes(b"application")
            data.mkdir()
            (data / "fixture-manifest.json").write_text(
                "{}\n", encoding="utf-8"
            )
            for filename in baseline_runner.endpoint_runner.MNIST_FILES:
                (data / filename).write_bytes(filename.encode("ascii"))

            with mock.patch.object(
                baseline_runner.endpoint_runner,
                "git_provenance",
                return_value={"commit": "revision", "dirty": False},
            ), mock.patch.object(
                baseline_runner.shared_runner,
                "execution_environment",
                return_value={"host": "test"},
            ):
                campaign = baseline_runner.build_campaign(
                    axpike=axpike,
                    application=application,
                    data_directory=data,
                    proxy_kernel="pk",
                )

            self.assertEqual(campaign["image_count"], 10_000)
            self.assertEqual(
                campaign["configurations"],
                [{
                    "id": "original-fp32-fp64",
                    "transprecision_mode": "disabled",
                    "protected_bits": None,
                    "cli_argument": "--transprecision-mode=disabled",
                }],
            )
            self.assertNotIn("matrix", campaign)
            self.assertNotIn("reduced_gate", campaign)

    def test_main_forwards_full_image_count_and_supports_dry_run(self) -> None:
        campaign = {
            "purpose": baseline_runner.PURPOSE,
            "scope": baseline_runner.SCOPE,
        }
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            arguments = [
                "run_full_lenet_architectural_baseline.py",
                "--axpike", str(root / "axpike"),
                "--application", str(root / "application"),
                "--data-directory", str(root / "data"),
                "--output-directory", str(root / "output"),
                "--max-new-runs", "0",
            ]
            with mock.patch.object(sys, "argv", arguments), mock.patch.object(
                baseline_runner.endpoint_runner, "require_file"
            ), mock.patch.object(
                baseline_runner.full_runner, "require_fixture"
            ), mock.patch.object(
                baseline_runner, "build_campaign", return_value=campaign
            ), mock.patch.object(
                baseline_runner.shared_runner,
                "initialize_or_validate_campaign",
                return_value=(root / "output/campaign.json", "hash"),
            ), mock.patch.object(
                baseline_runner.shared_runner,
                "run_pending_configurations",
                return_value=[],
            ) as run, mock.patch.object(
                baseline_runner, "finalize_baseline"
            ) as finalize:
                self.assertEqual(baseline_runner.main(), 0)

            self.assertEqual(run.call_args.kwargs["image_count"], 10_000)
            self.assertEqual(run.call_args.kwargs["max_new_runs"], 0)
            self.assertEqual(
                run.call_args.kwargs["configurations"],
                [baseline_runner.CONFIGURATION],
            )
            finalize.assert_called_once()


if __name__ == "__main__":
    unittest.main()
