#!/usr/bin/env python3
"""Freeze the complete standard MNIST files for the SSCAD campaign."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path
from typing import Any


IMAGE_MAGIC = 2051
LABEL_MAGIC = 2049
ROWS = 28
COLUMNS = 28
TRAIN_COUNT = 60_000
TEST_COUNT = 10_000
FILES = {
    "train-images-idx3-ubyte": ("images", TRAIN_COUNT),
    "train-labels-idx1-ubyte": ("labels", TRAIN_COUNT),
    "t10k-images-idx3-ubyte": ("images", TEST_COUNT),
    "t10k-labels-idx1-ubyte": ("labels", TEST_COUNT),
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as input_file:
        for chunk in iter(lambda: input_file.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def validate_idx(path: Path, kind: str, expected_count: int) -> None:
    with path.open("rb") as input_file:
        if kind == "images":
            header = input_file.read(16)
            if len(header) != 16:
                raise ValueError(f"truncated IDX image header: {path}")
            magic, count, rows, columns = struct.unpack(">IIII", header)
            if (magic, count, rows, columns) != (
                IMAGE_MAGIC,
                expected_count,
                ROWS,
                COLUMNS,
            ):
                raise ValueError(f"unexpected IDX image header: {path}")
            expected_size = 16 + count * rows * columns
        else:
            header = input_file.read(8)
            if len(header) != 8:
                raise ValueError(f"truncated IDX label header: {path}")
            magic, count = struct.unpack(">II", header)
            if (magic, count) != (LABEL_MAGIC, expected_count):
                raise ValueError(f"unexpected IDX label header: {path}")
            expected_size = 8 + count
    if path.stat().st_size != expected_size:
        raise ValueError(
            f"unexpected IDX file size for {path}: "
            f"{path.stat().st_size}, expected {expected_size}"
        )


def file_record(path: Path) -> dict[str, Any]:
    return {
        "path": str(path.resolve()),
        "size": path.stat().st_size,
        "sha256": sha256(path),
    }


def prepare_fixture(
    source_directory: Path,
    output_directory: Path,
) -> dict[str, Any]:
    if output_directory.exists():
        raise ValueError(
            "output directory already exists; choose a new directory"
        )
    for filename, (kind, count) in FILES.items():
        source = source_directory / filename
        if not source.is_file():
            raise ValueError(f"missing MNIST file: {source}")
        validate_idx(source, kind, count)

    output_directory.mkdir(parents=True)
    for filename in FILES:
        (output_directory / filename).symlink_to(
            (source_directory / filename).resolve()
        )

    return {
        "schema_version": 1,
        "purpose": "complete standard MNIST fixture",
        "test_image_count": TEST_COUNT,
        "train_image_count": TRAIN_COUNT,
        "source_files": {
            filename: file_record(source_directory / filename)
            for filename in FILES
        },
        "fixture_files": {
            filename: file_record(output_directory / filename)
            for filename in FILES
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Validate and freeze the complete standard MNIST IDX files."
        )
    )
    parser.add_argument("--source-directory", type=Path, required=True)
    parser.add_argument("--output-directory", type=Path, required=True)
    args = parser.parse_args()

    manifest = prepare_fixture(
        args.source_directory.resolve(),
        args.output_directory,
    )
    (args.output_directory / "fixture-manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True, ensure_ascii=True)
        + "\n",
        encoding="utf-8",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
