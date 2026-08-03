from __future__ import annotations

import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


SCRIPTS = Path(__file__).resolve().parents[1] / "scripts"
sys.path.insert(0, str(SCRIPTS))


def load_script(name: str):
    path = SCRIPTS / f"{name}.py"
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec is not None
    assert spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


matrix_generator = load_script("generate_transprecision_experiment_matrix")
matrix_runner = load_script("run_uniform_n_reduced_matrix")


class UniformNReducedMatrixTest(unittest.TestCase):
    def configuration(self, n: int) -> dict[str, object]:
        return matrix_generator.build_manifest()["configurations"][n]

    def test_matrix_contract_accepts_all_points_and_rejects_gaps(self) -> None:
        manifest = matrix_generator.build_manifest()
        configurations = matrix_runner.validate_matrix(manifest)
        self.assertEqual(len(configurations), 51)
        self.assertEqual(
            [configuration["n"] for configuration in configurations],
            list(range(51)),
        )

        incomplete = json.loads(json.dumps(manifest))
        incomplete["configurations"].pop(25)
        incomplete["dynamic_configuration_count"] = 50
        incomplete["planned_new_execution_count"] = 50
        with self.assertRaisesRegex(ValueError, "51 configurations"):
            matrix_runner.validate_matrix(incomplete)

    def test_attempt_directories_preserve_failed_or_interrupted_work(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            configuration_directory = Path(temporary_directory) / "n-00"
            first = matrix_runner.next_attempt_directory(
                configuration_directory
            )
            (first / "partial.log").write_text("interrupted", encoding="utf-8")
            second = matrix_runner.next_attempt_directory(
                configuration_directory
            )
            self.assertEqual(first.name, "attempt-0001")
            self.assertEqual(second.name, "attempt-0002")
            self.assertTrue((first / "partial.log").is_file())

    def test_campaign_identity_allows_exact_resume_only(self) -> None:
        campaign = {
            "schema_version": matrix_runner.CAMPAIGN_SCHEMA_VERSION,
            "purpose": matrix_runner.PURPOSE,
            "input": {"sha256": "abc"},
        }
        with tempfile.TemporaryDirectory() as temporary_directory:
            output = Path(temporary_directory) / "campaign"
            first_path, first_hash = (
                matrix_runner.initialize_or_validate_campaign(output, campaign)
            )
            second_path, second_hash = (
                matrix_runner.initialize_or_validate_campaign(output, campaign)
            )
            self.assertEqual(first_path, second_path)
            self.assertEqual(first_hash, second_hash)

            changed = json.loads(json.dumps(campaign))
            changed["input"]["sha256"] = "changed"
            with self.assertRaisesRegex(ValueError, "identity differs"):
                matrix_runner.initialize_or_validate_campaign(output, changed)

    def make_completion(
        self,
        output: Path,
        configuration: dict[str, object],
        campaign_hash: str,
    ) -> None:
        configuration_directory = (
            output / "runs" / f"n-{configuration['n']:02d}"
        )
        attempt_directory = configuration_directory / "attempt-0001"
        run_directory = attempt_directory / str(configuration["id"])
        run_directory.mkdir(parents=True)
        record_path = run_directory / "run.json"
        record_path.write_text("{}\n", encoding="utf-8")
        run_entry = {
            "id": configuration["id"],
            "record": str(record_path.relative_to(output)),
            "sha256": matrix_runner.endpoint_runner.sha256(record_path),
        }
        gate = {
            "schema_version": 1,
            "status": "passed",
            "campaign_sha256": campaign_hash,
            "invariants": matrix_runner.PASSED_INVARIANTS,
        }
        gate_path = attempt_directory / "gate.json"
        matrix_runner.write_json_atomic(gate_path, gate)
        completion = {
            "schema_version": 1,
            "status": "passed",
            "campaign_sha256": campaign_hash,
            "configuration": {
                "n": configuration["n"],
                "id": configuration["id"],
                "protected_bits": configuration["protected_bits"],
            },
            "run": run_entry,
            "gate": {
                "path": str(gate_path.relative_to(output)),
                "sha256": matrix_runner.endpoint_runner.sha256(gate_path),
            },
        }
        matrix_runner.write_json_atomic(
            configuration_directory / "completion.json", completion
        )

    def test_completion_requires_valid_gate_and_matching_configuration(
        self,
    ) -> None:
        configuration = self.configuration(8)
        campaign_hash = "campaign-hash"
        with tempfile.TemporaryDirectory() as temporary_directory:
            output = Path(temporary_directory)
            self.make_completion(output, configuration, campaign_hash)

            def extract_run(_root, run_entry):
                return {
                    "id": run_entry["id"],
                    "n": configuration["n"],
                    "protected_bits": configuration["protected_bits"],
                }

            entry = matrix_runner.validate_completion(
                output_directory=output,
                configuration=configuration,
                campaign_sha256=campaign_hash,
                extract_run=extract_run,
            )
            self.assertEqual(entry["id"], configuration["id"])

            gate_path = output / "runs/n-08/attempt-0001/gate.json"
            gate_path.write_text("{}\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "gate hash mismatch"):
                matrix_runner.validate_completion(
                    output_directory=output,
                    configuration=configuration,
                    campaign_sha256=campaign_hash,
                    extract_run=extract_run,
                )

    def test_failed_run_is_preserved_and_resume_uses_new_attempt(self) -> None:
        configuration = self.configuration(0)
        campaign_hash = "campaign-hash"
        with tempfile.TemporaryDirectory() as temporary_directory:
            output = Path(temporary_directory)

            with mock.patch.object(
                matrix_runner.endpoint_runner,
                "run_configuration",
                side_effect=RuntimeError("simulated interruption"),
            ):
                with self.assertRaisesRegex(RuntimeError, "interruption"):
                    matrix_runner.run_pending_configurations(
                        output_directory=output,
                        campaign_sha256=campaign_hash,
                        configurations=[configuration],
                        axpike=Path("axpike"),
                        proxy_kernel="pk",
                        application=Path("application"),
                        data_directory=Path("data"),
                        max_new_runs=None,
                    )
            first_attempt = output / "runs/n-00/attempt-0001"
            self.assertTrue((first_attempt / "failure.json").is_file())
            self.assertEqual(
                json.loads((output / "progress.json").read_text())["status"],
                "failed",
            )

            def successful_run(**kwargs):
                run_directory = (
                    kwargs["output_directory"]
                    / kwargs["configuration"].identifier
                )
                run_directory.mkdir()
                record_path = run_directory / "run.json"
                record_path.write_text("{}\n", encoding="utf-8")
                return {
                    "id": kwargs["configuration"].identifier,
                    "record": str(
                        record_path.relative_to(kwargs["output_directory"])
                    ),
                    "sha256": matrix_runner.endpoint_runner.sha256(record_path),
                }

            def extracted(_root, run_entry):
                return {
                    "id": run_entry["id"],
                    "n": configuration["n"],
                    "protected_bits": configuration["protected_bits"],
                }

            with mock.patch.object(
                matrix_runner.endpoint_runner,
                "run_configuration",
                side_effect=successful_run,
            ) as execute, mock.patch.object(
                matrix_runner.summarizer,
                "extract_run",
                side_effect=extracted,
            ):
                entries = matrix_runner.run_pending_configurations(
                    output_directory=output,
                    campaign_sha256=campaign_hash,
                    configurations=[configuration],
                    axpike=Path("axpike"),
                    proxy_kernel="pk",
                    application=Path("application"),
                    data_directory=Path("data"),
                    max_new_runs=None,
                )
                self.assertEqual(len(entries), 1)
                self.assertEqual(execute.call_count, 1)

                resumed = matrix_runner.run_pending_configurations(
                    output_directory=output,
                    campaign_sha256=campaign_hash,
                    configurations=[configuration],
                    axpike=Path("axpike"),
                    proxy_kernel="pk",
                    application=Path("application"),
                    data_directory=Path("data"),
                    max_new_runs=None,
                )
                self.assertEqual(len(resumed), 1)
                self.assertEqual(execute.call_count, 1)
            self.assertTrue((output / "runs/n-00/attempt-0002").is_dir())

    def test_uniform_summary_requires_complete_ordered_matrix(self) -> None:
        configurations = matrix_generator.build_manifest()["configurations"]
        with tempfile.TemporaryDirectory() as temporary_directory:
            manifest_path = Path(temporary_directory) / "run-manifest.json"
            manifest = {
                "schema_version": 4,
                "purpose": matrix_runner.PURPOSE,
                "scope": "validation",
                "image_count": 62,
                "mode": matrix_runner.endpoint_runner.EXPERIMENT_MODE,
                "type_order": list(matrix_runner.endpoint_runner.TYPE_ORDER),
                "runs": [
                    {"id": configuration["id"]}
                    for configuration in configurations
                ],
            }
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")

            extracted_runs = [
                {
                    "id": configuration["id"],
                    "n": configuration["n"],
                    "mode": matrix_runner.endpoint_runner.EXPERIMENT_MODE,
                    "protected_bits": configuration["protected_bits"],
                    "outcome": {
                        "processed": 62,
                        "correct": 62,
                        "errors": 0,
                    },
                }
                for configuration in configurations
            ]
            with mock.patch.object(
                matrix_runner.summarizer,
                "extract_run",
                side_effect=extracted_runs,
            ):
                summary = matrix_runner.summarizer.build_summary(manifest_path)
            self.assertEqual(summary["schema_version"], 4)
            self.assertEqual(len(summary["runs"]), 51)

            manifest["runs"].pop()
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            with mock.patch.object(
                matrix_runner.summarizer,
                "extract_run",
                side_effect=extracted_runs[:-1],
            ):
                with self.assertRaisesRegex(ValueError, "n=0..50"):
                    matrix_runner.summarizer.build_summary(manifest_path)


if __name__ == "__main__":
    unittest.main()
