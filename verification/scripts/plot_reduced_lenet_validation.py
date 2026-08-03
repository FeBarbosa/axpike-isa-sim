#!/usr/bin/env python3
"""Render a dependency-free SVG overview of reduced LeNet validation."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
from pathlib import Path
from typing import Any
from xml.sax.saxutils import escape


WIDTH = 1200
HEIGHT = 440
TYPE_ORDER = ("E5M2", "FP16", "FP32", "FP64")
TYPE_COLORS = {
    "E5M2": "#202020",
    "FP16": "#686868",
    "FP32": "#aaaaaa",
    "FP64": "#e4e4e4",
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def text(
    x: float,
    y: float,
    value: str,
    *,
    size: int = 15,
    anchor: str = "middle",
    weight: str = "normal",
    fill: str = "#111111",
    transform: str = "",
) -> str:
    transform_attribute = (
        f' transform="{transform}"' if transform else ""
    )
    return (
        f'<text x="{x:.1f}" y="{y:.1f}" text-anchor="{anchor}" '
        f'font-family="DejaVu Sans, sans-serif" font-size="{size}" '
        f'font-weight="{weight}" fill="{fill}"{transform_attribute}>'
        f"{escape(value)}</text>"
    )


def rect(
    x: float,
    y: float,
    width: float,
    height: float,
    fill: str,
    *,
    stroke: str = "#111111",
) -> str:
    return (
        f'<rect x="{x:.1f}" y="{y:.1f}" width="{width:.1f}" '
        f'height="{height:.1f}" fill="{fill}" stroke="{stroke}" '
        'stroke-width="1"/>'
    )


def line(x1: float, y1: float, x2: float, y2: float) -> str:
    return (
        f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" '
        f'y2="{y2:.1f}" stroke="#111111" stroke-width="1"/>'
    )


def panel_accuracy(runs: list[dict[str, Any]]) -> list[str]:
    elements = [text(205, 42, "(a) Reduced accuracy", weight="bold")]
    baseline_y = 350
    plot_height = 245
    elements.extend(
        [
            line(55, baseline_y, 365, baseline_y),
            line(55, baseline_y - plot_height, 55, baseline_y),
            text(45, baseline_y + 5, "0", anchor="end", size=12),
            text(45, baseline_y - plot_height + 5, "100", anchor="end", size=12),
            text(
                18,
                228,
                "Accuracy (%)",
                size=13,
                transform="rotate(-90 18 228)",
            ),
        ]
    )
    for index, run in enumerate(runs):
        percentage = 100.0 * run["accuracy"]
        bar_height = plot_height * run["accuracy"]
        x = 105 + index * 145
        elements.extend(
            [
                rect(
                    x,
                    baseline_y - bar_height,
                    80,
                    bar_height,
                    "#777777" if index == 0 else "#d0d0d0",
                ),
                text(
                    x + 40,
                    baseline_y - bar_height - 10,
                    f"{run['outcome']['correct']}/{run['outcome']['processed']}"
                    f" ({percentage:.1f}%)",
                    size=12,
                ),
                text(
                    x + 40,
                    baseline_y + 25,
                    "Full protection"
                    if run["id"] == "full-protection"
                    else "No protection",
                    size=12,
                ),
            ]
        )
    return elements


def panel_effective_types(runs: list[dict[str, Any]]) -> list[str]:
    offset = 400
    elements = [
        text(offset + 200, 42, "(b) Effective-type distribution", weight="bold")
    ]
    baseline_y = 350
    plot_height = 245
    elements.extend(
        [
            line(offset + 55, baseline_y, offset + 365, baseline_y),
            line(
                offset + 55,
                baseline_y - plot_height,
                offset + 55,
                baseline_y,
            ),
            text(offset + 45, baseline_y + 5, "0", anchor="end", size=12),
            text(
                offset + 45,
                baseline_y - plot_height + 5,
                "100",
                anchor="end",
                size=12,
            ),
        ]
    )
    for index, run in enumerate(runs):
        x = offset + 105 + index * 145
        total = run["effective_type_observations"]
        current_y = baseline_y
        for effective_type in TYPE_ORDER:
            share = run["effective_types"][effective_type] / total
            segment_height = plot_height * share
            current_y -= segment_height
            if segment_height > 0.25:
                elements.append(
                    rect(
                        x,
                        current_y,
                        80,
                        segment_height,
                        TYPE_COLORS[effective_type],
                    )
                )
            if segment_height >= 20:
                elements.append(
                    text(
                        x + 40,
                        current_y + segment_height / 2 + 5,
                        f"{100 * share:.1f}%",
                        size=11,
                        fill=(
                            "#ffffff"
                            if effective_type in ("E5M2", "FP16")
                            else "#111111"
                        ),
                    )
                )
        elements.append(
            text(
                x + 40,
                baseline_y + 25,
                "Full protection"
                if run["id"] == "full-protection"
                else "No protection",
                size=12,
            )
        )
    legend_x = offset + 75
    for index, effective_type in enumerate(TYPE_ORDER):
        x = legend_x + index * 75
        elements.extend(
            [
                rect(x, 388, 14, 14, TYPE_COLORS[effective_type]),
                text(
                    x + 19,
                    400,
                    effective_type,
                    anchor="start",
                    size=10,
                ),
            ]
        )
    return elements


def panel_nan_accounting(full_protection: dict[str, Any]) -> list[str]:
    offset = 800
    elements = [
        text(
            offset + 200,
            42,
            "(c) Full-protection NaN accounting",
            weight="bold",
        )
    ]
    diagnostics = full_protection["diagnostics"]
    values = (
        ("External\ncarrier", diagnostics["external_nan_total"]),
        (
            "Effective\nboxed FP32",
            diagnostics["fp64_load_nan_boxed_fp32_effective_total"],
        ),
        ("Operation\nNaN", diagnostics["operation_nan_total"]),
    )
    baseline_y = 350
    plot_height = 245
    maximum = max(value for _, value in values) or 1
    elements.extend(
        [
            line(offset + 55, baseline_y, offset + 375, baseline_y),
            line(
                offset + 55,
                baseline_y - plot_height,
                offset + 55,
                baseline_y,
            ),
            text(offset + 45, baseline_y + 5, "0", anchor="end", size=12),
            text(
                offset + 45,
                baseline_y - plot_height + 5,
                str(maximum),
                anchor="end",
                size=12,
            ),
            text(
                offset + 18,
                228,
                "Events",
                size=13,
                transform=f"rotate(-90 {offset + 18} 228)",
            ),
        ]
    )
    for index, (label, value) in enumerate(values):
        bar_height = plot_height * value / maximum
        x = offset + 85 + index * 95
        elements.extend(
            [
                rect(
                    x,
                    baseline_y - bar_height,
                    55,
                    bar_height,
                    ("#555555", "#999999", "#dedede")[index],
                ),
                text(
                    x + 27.5,
                    baseline_y - bar_height - 10,
                    str(value),
                    size=12,
                ),
            ]
        )
        label_lines = label.split("\n")
        for line_index, label_line in enumerate(label_lines):
            elements.append(
                text(
                    x + 27.5,
                    baseline_y + 20 + 14 * line_index,
                    label_line,
                    size=10,
                )
            )
    return elements


def render_svg(summary: dict[str, Any]) -> str:
    runs = summary["runs"]
    if [run["id"] for run in runs] != [
        "full-protection",
        "no-protection",
    ]:
        raise ValueError(
            "summary must contain full-protection and no-protection runs"
        )
    elements = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{WIDTH}" '
        f'height="{HEIGHT}" viewBox="0 0 {WIDTH} {HEIGHT}">',
        '<rect width="100%" height="100%" fill="#ffffff"/>',
        *panel_accuracy(runs),
        *panel_effective_types(runs),
        *panel_nan_accounting(runs[0]),
        text(
            WIDTH / 2,
            432,
            (
                f"Deterministic {summary['image_count']}-image prefix; "
                "validation-only evidence"
            ),
            size=11,
        ),
        "</svg>",
    ]
    return "\n".join(elements) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Render reduced LeNet validation example plots."
    )
    parser.add_argument("--summary", type=Path, required=True)
    parser.add_argument("--output-directory", type=Path, required=True)
    parser.add_argument(
        "--pdf",
        action="store_true",
        help="Also convert the deterministic SVG source to PDF with Inkscape.",
    )
    args = parser.parse_args()

    if args.output_directory.exists():
        raise ValueError(
            "output directory already exists; choose a new directory"
        )
    args.output_directory.mkdir(parents=True)
    summary_reference = str(args.summary)
    summary_path = args.summary.resolve()
    summary = json.loads(summary_path.read_text(encoding="utf-8"))
    svg_path = args.output_directory / "reduced_validation_overview.svg"
    svg_path.write_text(render_svg(summary), encoding="utf-8")

    artifacts = {
        svg_path.name: sha256(svg_path),
    }
    if args.pdf:
        pdf_path = args.output_directory / "reduced_validation_overview.pdf"
        subprocess.run(
            [
                "inkscape",
                str(svg_path),
                "--export-type=pdf",
                f"--export-filename={pdf_path}",
            ],
            check=True,
            env=os.environ | {"SOURCE_DATE_EPOCH": "0"},
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        artifacts[pdf_path.name] = sha256(pdf_path)

    manifest_path = args.output_directory / "figures-manifest.json"
    manifest_path.write_text(
        json.dumps(
            {
                "schema_version": 1,
                "scope": summary["scope"],
                "source_summary": {
                    "path": summary_reference,
                    "sha256": sha256(summary_path),
                },
                "artifacts": artifacts,
            },
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
