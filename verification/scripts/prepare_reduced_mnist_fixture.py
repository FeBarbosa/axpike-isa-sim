#!/usr/bin/env python3
"""Prepare the smallest deterministic MNIST prefix covering all ten classes."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path
from typing import Any


IMAGE_MAGIC = 2051
LABEL_MAGIC = 2049
IMAGE_COUNT = 62
EXPECTED_CLASSES = tuple(range(10))
TRAIN_FILES = (
    "train-images-idx3-ubyte",
    "train-labels-idx1-ubyte",
)
TEST_IMAGE_FILE = "t10k-images-idx3-ubyte"
TEST_LABEL_FILE = "t10k-labels-idx1-ubyte"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as input_file:
        for chunk in iter(lambda: input_file.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def truncate_images(source: Path, destination: Path, count: int) -> None:
    with source.open("rb") as input_file:
        header = input_file.read(16)
        if len(header) != 16:
            raise ValueError(f"truncated IDX image header: {source}")
        magic, source_count, rows, columns = struct.unpack(">IIII", header)
        if magic != IMAGE_MAGIC:
            raise ValueError(f"unexpected IDX image magic in {source}: {magic}")
        if count > source_count:
            raise ValueError(
                f"requested {count} images from a {source_count}-image file"
            )
        payload_size = count * rows * columns
        payload = input_file.read(payload_size)
        if len(payload) != payload_size:
            raise ValueError(f"truncated IDX image payload: {source}")
    with destination.open("wb") as output_file:
        output_file.write(struct.pack(">IIII", magic, count, rows, columns))
        output_file.write(payload)


def truncate_labels(source: Path, destination: Path, count: int) -> bytes:
    with source.open("rb") as input_file:
        header = input_file.read(8)
        if len(header) != 8:
            raise ValueError(f"truncated IDX label header: {source}")
        magic, source_count = struct.unpack(">II", header)
        if magic != LABEL_MAGIC:
            raise ValueError(f"unexpected IDX label magic in {source}: {magic}")
        if count > source_count:
            raise ValueError(
                f"requested {count} labels from a {source_count}-label file"
            )
        payload = input_file.read(count)
        if len(payload) != count:
            raise ValueError(f"truncated IDX label payload: {source}")
    with destination.open("wb") as output_file:
        output_file.write(struct.pack(">II", magic, count))
        output_file.write(payload)
    return payload


def file_record(path: Path) -> dict[str, Any]:
    return {
        "path": str(path.resolve()),
        "size": path.stat().st_size,
        "sha256": sha256(path),
    }


def prepare_fixture(
    source_directory: Path,
    output_directory: Path,
    count: int = IMAGE_COUNT,
) -> dict[str, Any]:
    if output_directory.exists():
        raise ValueError(
            "output directory already exists; choose a new directory"
        )
    required_files = (
        *TRAIN_FILES,
        TEST_IMAGE_FILE,
        TEST_LABEL_FILE,
    )
    for filename in required_files:
        if not (source_directory / filename).is_file():
            raise ValueError(f"missing MNIST file: {source_directory / filename}")

    output_directory.mkdir(parents=True)
    for filename in TRAIN_FILES:
        (output_directory / filename).symlink_to(
            (source_directory / filename).resolve()
        )
    truncate_images(
        source_directory / TEST_IMAGE_FILE,
        output_directory / TEST_IMAGE_FILE,
        count,
    )
    labels = truncate_labels(
        source_directory / TEST_LABEL_FILE,
        output_directory / TEST_LABEL_FILE,
        count,
    )
    class_counts = {
        str(label): labels.count(label)
        for label in EXPECTED_CLASSES
    }
    represented_classes = [
        label for label in EXPECTED_CLASSES if class_counts[str(label)] > 0
    ]
    covers_all_classes = represented_classes == list(EXPECTED_CLASSES)
    minimal_prefix_covering_all_classes = (
        covers_all_classes
        and not set(EXPECTED_CLASSES).issubset(set(labels[:-1]))
    )

    return {
        "schema_version": 2,
        "purpose": "deterministic reduced MNIST fixture",
        "image_count": count,
        "selection": {
            "kind": "contiguous-test-prefix",
            "zero_based_last_source_index": count - 1,
            "expected_classes": list(EXPECTED_CLASSES),
            "represented_classes": represented_classes,
            "class_counts": class_counts,
            "covers_all_classes": covers_all_classes,
            "minimal_prefix_covering_all_classes":
                minimal_prefix_covering_all_classes,
        },
        "source_files": {
            filename: file_record(source_directory / filename)
            for filename in required_files
        },
        "fixture_files": {
            filename: file_record(output_directory / filename)
            for filename in required_files
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Create the deterministic 62-image MNIST IDX prefix that first "
            "covers all ten classes."
        )
    )
    parser.add_argument("--source-directory", type=Path, required=True)
    parser.add_argument("--output-directory", type=Path, required=True)
    parser.add_argument("--count", type=int, default=IMAGE_COUNT)
    args = parser.parse_args()
    if args.count <= 0:
        raise ValueError("count must be positive")

    manifest = prepare_fixture(
        args.source_directory.resolve(),
        args.output_directory,
        args.count,
    )
    if args.count == IMAGE_COUNT:
        selection = manifest["selection"]
        if (
            not selection["covers_all_classes"]
            or not selection["minimal_prefix_covering_all_classes"]
        ):
            raise ValueError(
                "the default 62-image fixture is not the minimal prefix "
                "covering all ten classes"
            )
    (args.output_directory / "fixture-manifest.json").write_text(
        json.dumps(
            manifest,
            indent=2,
            sort_keys=True,
            ensure_ascii=True,
        )
        + "\n",
        encoding="utf-8",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
