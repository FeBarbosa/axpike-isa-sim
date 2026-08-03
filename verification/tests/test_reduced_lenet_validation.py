from __future__ import annotations

import csv
import hashlib
import importlib.util
import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPTS = Path(__file__).resolve().parents[1] / "scripts"


def load_script(name: str):
    path = SCRIPTS / f"{name}.py"
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec is not None
    assert spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


runner = load_script("run_reduced_lenet_validation")
summarizer = load_script("summarize_reduced_lenet_validation")
plotter = load_script("plot_reduced_lenet_validation")
fixture = load_script("prepare_reduced_mnist_fixture")


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


class ReducedLenetValidationTest(unittest.TestCase):
    def test_fixture_preserves_first_idx_records_and_rewrites_counts(
        self,
    ) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            source = root / "source"
            source.mkdir()
            (source / fixture.TEST_IMAGE_FILE).write_bytes(
                struct.pack(">IIII", fixture.IMAGE_MAGIC, 3, 2, 2)
                + bytes(range(12))
            )
            (source / fixture.TEST_LABEL_FILE).write_bytes(
                struct.pack(">II", fixture.LABEL_MAGIC, 3)
                + bytes((7, 8, 9))
            )
            for filename in fixture.TRAIN_FILES:
                (source / filename).write_bytes(b"training")

            output = root / "fixture"
            manifest = fixture.prepare_fixture(source, output, count=2)
            self.assertEqual(manifest["image_count"], 2)
            image_bytes = (output / fixture.TEST_IMAGE_FILE).read_bytes()
            label_bytes = (output / fixture.TEST_LABEL_FILE).read_bytes()
            self.assertEqual(
                struct.unpack(">IIII", image_bytes[:16]),
                (fixture.IMAGE_MAGIC, 2, 2, 2),
            )
            self.assertEqual(image_bytes[16:], bytes(range(8)))
            self.assertEqual(
                struct.unpack(">II", label_bytes[:8]),
                (fixture.LABEL_MAGIC, 2),
            )
            self.assertEqual(label_bytes[8:], bytes((7, 8)))
            self.assertEqual(manifest["schema_version"], 2)
            self.assertEqual(
                manifest["selection"]["represented_classes"],
                [7, 8],
            )
            self.assertFalse(
                manifest["selection"]["covers_all_classes"]
            )
            self.assertTrue((output / fixture.TRAIN_FILES[0]).is_symlink())

    def test_fixture_records_minimal_prefix_covering_all_classes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            source = root / "source"
            source.mkdir()
            labels = bytes((*range(9), 0, 9))
            (source / fixture.TEST_IMAGE_FILE).write_bytes(
                struct.pack(">IIII", fixture.IMAGE_MAGIC, len(labels), 1, 1)
                + bytes(range(len(labels)))
            )
            (source / fixture.TEST_LABEL_FILE).write_bytes(
                struct.pack(">II", fixture.LABEL_MAGIC, len(labels))
                + labels
            )
            for filename in fixture.TRAIN_FILES:
                (source / filename).write_bytes(b"training")

            manifest = fixture.prepare_fixture(
                source,
                root / "fixture",
                count=len(labels),
            )
            selection = manifest["selection"]
            self.assertTrue(selection["covers_all_classes"])
            self.assertTrue(
                selection["minimal_prefix_covering_all_classes"]
            )
            self.assertEqual(
                selection["represented_classes"],
                list(range(10)),
            )
            self.assertEqual(
                selection["zero_based_last_source_index"],
                len(labels) - 1,
            )

    def write_transprecision_csv(
        self,
        path: Path,
        protected_bits: tuple[int, ...],
        *,
        external_nan: int = 123,
        effective_boxed: int = 58,
        unclassified: int = 0,
        policy_version: int = summarizer.POLICY_VERSION,
        invalid_promotions: int = 0,
    ) -> None:
        rows = [
            (
                "policy_version",
                "",
                "",
                "",
                summarizer.POLICY_NAME,
                "",
                policy_version,
            )
        ]
        for name, value in zip(runner.TYPE_ORDER, protected_bits):
            rows.append(
                (
                    "policy_protected_bits",
                    "",
                    "",
                    "",
                    name.upper(),
                    "",
                    value,
                )
            )
        rows.extend(
            [
                (
                    "effective_type_by_instruction",
                    "fadd_s",
                    "",
                    "",
                    "E5M2",
                    "",
                    10,
                ),
                ("effective_type_by_instruction", "fadd_s", "", "", "FP16", "", 20),
                ("effective_type_by_instruction", "fadd_s", "", "", "FP32", "", 60),
                ("effective_type_by_instruction", "fadd_d", "", "", "FP64", "", 10),
                ("operand_unclassified_total", "", "", "", "", "", 0),
                (
                    "result_tag_reduction_to_zero_total",
                    "",
                    "",
                    "",
                    "",
                    "",
                    2,
                ),
                (
                    "external_write_masked_to_zero_total",
                    "",
                    "",
                    "",
                    "",
                    "",
                    1,
                ),
                (
                    "invalid_result_promotion_total",
                    "",
                    "",
                    "",
                    "",
                    "",
                    invalid_promotions,
                ),
                ("lazy_reclassification_total", "", "", "", "", "", 3),
                ("unclassified_fallback_total", "", "", "", "", "", 0),
                (
                    "fp64_load_nan_boxed_fp32_effective_total",
                    "",
                    "",
                    "",
                    "",
                    "",
                    effective_boxed,
                ),
                (
                    "external_write_class_total",
                    "",
                    "",
                    "",
                    "",
                    "NAN",
                    external_nan,
                ),
                (
                    "operation_result_class_total",
                    "",
                    "",
                    "",
                    "",
                    "FINITE",
                    100,
                ),
                (
                    "operation_result_class_total",
                    "",
                    "",
                    "",
                    "",
                    "NAN",
                    0,
                ),
                (
                    "operand_promotion_from_to",
                    "",
                    "E5M2",
                    "FP32",
                    "",
                    "",
                    5,
                ),
                (
                    "external_write_masked_from_to",
                    "",
                    "FP32",
                    "E5M2",
                    "",
                    "",
                    2,
                ),
            ]
        )
        if unclassified:
            rows.append(
                (
                    "effective_type_by_instruction",
                    "fadd_s",
                    "",
                    "",
                    "UNCLASSIFIED",
                    "",
                    unclassified,
                )
            )
        supported_types = ("E5M2", "FP16", "FP32", "FP64")
        for source_index, source in enumerate(supported_types):
            for destination in supported_types[:source_index]:
                pair = f"{source.lower()}-{destination.lower()}"
                total = 11 if pair == "fp32-e5m2" else 0
                changed = 7 if pair == "fp32-e5m2" else 0
                for category, value in (
                    ("result_tag_reduction_total_from_to", total),
                    ("result_tag_reduction_changed_from_to", changed),
                ):
                    rows.append(
                        (
                            category,
                            "",
                            source,
                            destination,
                            "",
                            "",
                            value,
                        )
                    )
        for carrier, effective in summarizer.QUANTIZATION_PAIRS:
            pair = f"{carrier.lower()}-{effective.lower()}"
            total = 100 if pair == "fp32-fp16" else 0
            changed = 25 if pair == "fp32-fp16" else 0
            to_zero = 2 if pair == "fp32-fp16" else 0
            overflow = 1 if pair == "fp32-fp16" else 0
            underflow = 2 if pair == "fp32-fp16" else 0
            for category, value in (
                ("result_quantization_total_from_to", total),
                ("result_quantization_changed_from_to", changed),
                ("result_quantization_to_zero_from_to", to_zero),
                ("result_quantization_overflow_from_to", overflow),
                ("result_quantization_underflow_from_to", underflow),
            ):
                rows.append(
                    (
                        category,
                        "",
                        carrier,
                        effective,
                        "",
                        "",
                        value,
                    )
                )
        with path.open("w", newline="", encoding="utf-8") as output:
            writer = csv.writer(output)
            writer.writerow(
                (
                    "Category",
                    "Instruction",
                    "From",
                    "To",
                    "Type",
                    "Class",
                    "Value",
                )
            )
            writer.writerows(rows)

    def create_fixture(self, root: Path) -> Path:
        run_entries = []
        for identifier, label, bits, correct in (
            (
                "full-protection",
                "Full-protection policy",
                runner.FULL_PROTECTION_BITS,
                62,
            ),
            (
                "no-protection",
                "No-protection endpoint",
                runner.NO_PROTECTION_BITS,
                11,
            ),
        ):
            run_directory = root / identifier
            run_directory.mkdir()
            csv_path = run_directory / "AxPIKE_transprecision_test.csv"
            self.write_transprecision_csv(csv_path, bits)
            record = {
                "schema_version": 3,
                "id": identifier,
                "label": label,
                "image_count": 62,
                "mode": "direct-logits",
                "protected_bits": runner.protected_bits_mapping(bits),
                "command": [],
                "outcome": {
                    "processed": 62,
                    "correct": correct,
                    "errors": 62 - correct,
                },
                "artifacts": {
                    "transprecision": {
                        "path": csv_path.name,
                        "sha256": sha256(csv_path),
                    }
                },
            }
            record_path = run_directory / "run.json"
            record_path.write_text(
                json.dumps(record, sort_keys=True),
                encoding="utf-8",
            )
            run_entries.append(
                {
                    "id": identifier,
                    "record": str(record_path.relative_to(root)),
                    "sha256": sha256(record_path),
                }
            )
        manifest_path = root / "run-manifest.json"
        manifest_path.write_text(
            json.dumps(
                {
                    "schema_version": 3,
                    "purpose": "reduced deterministic LeNet validation",
                    "scope": "implementation and simulator-model validation only",
                    "image_count": 62,
                    "mode": "direct-logits",
                    "type_order": list(runner.TYPE_ORDER),
                    "runs": run_entries,
                },
                sort_keys=True,
            ),
            encoding="utf-8",
        )
        return manifest_path

    def test_runner_outcome_parser_and_policy_argument(self) -> None:
        self.assertEqual(
            runner.parse_outcome("processed: 62\ncorrect: 61\nerrors: 1\n"),
            {"processed": 62, "correct": 61, "errors": 1},
        )
        self.assertEqual(
            runner.policy_argument(runner.NO_PROTECTION_BITS),
            "--transprecision-type-protected-bits="
            "fp64:0,fp32:0,fp16:0,e5m2:0",
        )

    def test_summary_extracts_metrics_and_passes_invariants(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            summary = summarizer.build_summary(self.create_fixture(root))
            self.assertEqual(summary["runs"][0]["accuracy"], 1.0)
            self.assertEqual(summary["runs"][1]["outcome"]["correct"], 11)
            self.assertEqual(
                summary["runs"][0]["diagnostics"][
                    "fp64_load_nan_boxed_fp32_effective_total"
                ],
                58,
            )
            self.assertEqual(
                summary["runs"][0]["transitions"][
                    "result_tag_reduction_changed_from_to"
                ],
                7,
            )
            self.assertEqual(
                summary["runs"][0]["quantization"][
                    "result_quantization_changed_from_to"
                ]["fp32-fp16"],
                25,
            )
            self.assertTrue(
                all(value == "passed" for value in summary["invariants"].values())
            )

    def test_summary_rejects_effective_boxed_count_above_carriers(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            manifest_path = self.create_fixture(root)
            exact_record_path = root / "full-protection" / "run.json"
            exact_record = json.loads(
                exact_record_path.read_text(encoding="utf-8")
            )
            csv_path = root / "full-protection" / exact_record[
                "artifacts"]["transprecision"]["path"]
            self.write_transprecision_csv(
                csv_path,
                runner.FULL_PROTECTION_BITS,
                external_nan=10,
                effective_boxed=11,
            )
            exact_record["artifacts"]["transprecision"]["sha256"] = sha256(
                csv_path
            )
            exact_record_path.write_text(
                json.dumps(exact_record, sort_keys=True),
                encoding="utf-8",
            )
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            manifest["runs"][0]["sha256"] = sha256(exact_record_path)
            manifest_path.write_text(
                json.dumps(manifest, sort_keys=True),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "exceeds external NaNs"):
                summarizer.build_summary(manifest_path)

    def test_summary_rejects_any_hashed_artifact_mismatch(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            manifest_path = self.create_fixture(root)
            record_path = root / "full-protection" / "run.json"
            record = json.loads(record_path.read_text(encoding="utf-8"))
            log_path = root / "full-protection" / "application.log"
            log_path.write_text("original\n", encoding="utf-8")
            record["artifacts"]["application_log"] = {
                "path": log_path.name,
                "sha256": sha256(log_path),
            }
            record_path.write_text(
                json.dumps(record, sort_keys=True), encoding="utf-8"
            )
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            manifest["runs"][0]["sha256"] = sha256(record_path)
            manifest_path.write_text(
                json.dumps(manifest, sort_keys=True), encoding="utf-8"
            )
            log_path.write_text("tampered\n", encoding="utf-8")
            with self.assertRaisesRegex(
                ValueError, "application_log artifact hash mismatch"
            ):
                summarizer.build_summary(manifest_path)

    def test_uniform_run_requires_policy_vector_in_command(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            manifest_path = self.create_fixture(root)
            record_path = root / "full-protection" / "run.json"
            record = json.loads(record_path.read_text(encoding="utf-8"))
            record["n"] = 50
            record["command"] = ["axpike", "pk", "application"]
            record_path.write_text(
                json.dumps(record, sort_keys=True), encoding="utf-8"
            )
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            manifest["runs"][0]["sha256"] = sha256(record_path)
            manifest_path.write_text(
                json.dumps(manifest, sort_keys=True), encoding="utf-8"
            )
            with self.assertRaisesRegex(
                ValueError, "command does not contain its policy vector"
            ):
                summarizer.build_summary(manifest_path)

    def test_summary_rejects_old_policy_and_invalid_promotions(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            manifest_path = self.create_fixture(root)
            exact_record_path = root / "full-protection" / "run.json"
            exact_record = json.loads(
                exact_record_path.read_text(encoding="utf-8")
            )
            csv_path = root / "full-protection" / exact_record[
                "artifacts"]["transprecision"]["path"]
            self.write_transprecision_csv(
                csv_path,
                runner.FULL_PROTECTION_BITS,
                policy_version=3,
            )
            exact_record["artifacts"]["transprecision"]["sha256"] = sha256(
                csv_path
            )
            exact_record_path.write_text(
                json.dumps(exact_record, sort_keys=True),
                encoding="utf-8",
            )
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            manifest["runs"][0]["sha256"] = sha256(exact_record_path)
            manifest_path.write_text(
                json.dumps(manifest, sort_keys=True),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "unsupported.*version"):
                summarizer.build_summary(manifest_path)

            self.write_transprecision_csv(
                csv_path,
                runner.FULL_PROTECTION_BITS,
                invalid_promotions=1,
            )
            exact_record["artifacts"]["transprecision"]["sha256"] = sha256(
                csv_path
            )
            exact_record_path.write_text(
                json.dumps(exact_record, sort_keys=True),
                encoding="utf-8",
            )
            manifest["runs"][0]["sha256"] = sha256(exact_record_path)
            manifest_path.write_text(
                json.dumps(manifest, sort_keys=True),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(
                ValueError, "invalid result promotions"
            ):
                summarizer.build_summary(manifest_path)

    def test_svg_is_deterministic_and_marks_validation_scope(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            summary = summarizer.build_summary(self.create_fixture(root))
            first = plotter.render_svg(summary)
            second = plotter.render_svg(summary)
            self.assertEqual(first, second)
            self.assertIn("validation-only evidence", first)
            self.assertIn(">Effective</text>", first)
            self.assertIn(">boxed FP32</text>", first)
            self.assertIn("11/62 (17.7%)", first)


if __name__ == "__main__":
    unittest.main()
