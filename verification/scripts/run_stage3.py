#!/usr/bin/env python3
from __future__ import annotations

import argparse
import concurrent.futures
import dataclasses
import json
import os
import subprocess
import sys
from pathlib import Path


ROOT_DIR = Path(__file__).resolve().parents[1]
# Keep all generated stage-3 files under the verification tree so the run is
# self-contained and easy to resume or inspect later.
GEN = ROOT_DIR / "out" / "fp16_input_generator"
CMP = ROOT_DIR / "out" / "fp16_file_compare"
OUT_DIR = ROOT_DIR / "out" / "stage3"
INPUT_DIR = OUT_DIR / "inputs"
LOG_DIR = OUT_DIR / "logs"
SUMMARY_DIR = OUT_DIR / "summaries"
EXECUTION_LOG = OUT_DIR / "execution.log"
PROGRESS_FILE = OUT_DIR / "progress.json"


@dataclasses.dataclass(frozen=True)
class Chunk:
    # An immutable chunk descriptor keeps the schedule easy to reason about and
    # safe to share across worker threads.
    op: str
    a_start: int
    a_end: int

    @property
    def a_start_hex(self) -> str:
        # The harness and generator both speak raw FP16 encodings in hex.
        return f"{self.a_start:04x}"

    @property
    def a_end_hex(self) -> str:
        return f"{self.a_end:04x}"

    @property
    def input_file(self) -> Path:
        return INPUT_DIR / f"{self.op}_a{self.a_start_hex}_a{self.a_end_hex}.hex"

    @property
    def log_file(self) -> Path:
        return LOG_DIR / f"{self.op}_a{self.a_start_hex}_a{self.a_end_hex}.log"

    @property
    def done_file(self) -> Path:
        return INPUT_DIR / f"{self.op}_a{self.a_start_hex}_a{self.a_end_hex}.done"


def parse_hex_u16(value: str) -> int:
    # Parse once at the edge, then keep the rest of the script working on ints.
    parsed = int(value, 16)
    if not 0 <= parsed <= 0xFFFF:
        raise ValueError(value)
    return parsed


def build_chunks(op: str, a_block_size: int) -> list[Chunk]:
    # Split the exhaustive sweep into fixed A-ranges; B always spans the full
    # FP16 encoding space in each chunk.
    chunks: list[Chunk] = []
    for a_start in range(0x0000, 0x10000, a_block_size):
        a_end = min(a_start + a_block_size - 1, 0xFFFF)
        chunks.append(Chunk(op=op, a_start=a_start, a_end=a_end))
    return chunks


def select_spread_chunks(chunks: list[Chunk], count: int) -> list[Chunk]:
    # Choose evenly spaced chunks so the sample covers the whole A interval
    # instead of only the prefix of the range.
    if count <= 0 or count >= len(chunks):
        return chunks
    if count == 1:
        return [chunks[0]]
    last = len(chunks) - 1
    picked: list[Chunk] = []
    seen: set[int] = set()
    for index in range(count):
        pos = round(index * last / (count - 1))
        if pos not in seen:
            picked.append(chunks[pos])
            seen.add(pos)
    return picked


def generate_input(chunk: Chunk, b_start: int, b_end: int) -> None:
    # A `.done` marker is the only resume signal for a completed chunk. If a
    # `.hex` file exists without `.done`, it may be a partial file left by an
    # interrupted generator, so regenerate it before comparing.
    if done_marker_is_valid(chunk.done_file):
        return
    if chunk.done_file.exists():
        chunk.done_file.unlink()
    if chunk.input_file.exists():
        chunk.input_file.unlink()
    cmd = [
        str(GEN),
        str(chunk.input_file),
        "--a-start",
        chunk.a_start_hex,
        "--a-end",
        chunk.a_end_hex,
        "--b-start",
        f"{b_start:04x}",
        "--b-end",
        f"{b_end:04x}",
    ]
    subprocess.run(cmd, check=True)


def parse_mismatch_count(output: str) -> int | None:
    for line in reversed(output.splitlines()):
        if line.startswith("operation="):
            parts = line.split()
            for part in parts:
                if part.startswith("mismatches="):
                    try:
                        return int(part.split("=", 1)[1])
                    except ValueError:
                        return None
    return None


def parse_done_marker(text: str) -> tuple[int, int | None]:
    status = 1
    mismatches: int | None = None
    for token in text.split():
        if token.startswith("status="):
            try:
                status = int(token.split("=", 1)[1])
            except ValueError:
                status = 1
        elif token.startswith("mismatches="):
            value = token.split("=", 1)[1]
            if value != "unknown":
                try:
                    mismatches = int(value)
                except ValueError:
                    mismatches = None
    return status, mismatches


def done_marker_is_valid(path: Path) -> bool:
    if not path.exists() or path.stat().st_size == 0:
        return False
    status, mismatches = parse_done_marker(path.read_text())
    return status == 0 or mismatches is not None


def compare_chunk(chunk: Chunk) -> tuple[Chunk, int, int | None, str]:
    # The `.done` marker is the lightest-weight resume signal for a completed
    # comparison.
    if done_marker_is_valid(chunk.done_file):
        prev_status, prev_mismatches = parse_done_marker(chunk.done_file.read_text())
        return chunk, prev_status, prev_mismatches, ""
    if chunk.done_file.exists():
        chunk.done_file.unlink()

    cmd = [str(CMP), chunk.op, str(chunk.input_file)]
    proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, check=False)
    output = proc.stdout or ""
    mismatch_count = parse_mismatch_count(output)

    if proc.returncode != 0:
        chunk.log_file.write_text(output, encoding="utf-8")
    elif chunk.log_file.exists():
        chunk.log_file.unlink()

    chunk.done_file.write_text(f"status={proc.returncode} mismatches={mismatch_count if mismatch_count is not None else 'unknown'}\n")
    return chunk, proc.returncode, mismatch_count, output


def process_chunk(chunk: Chunk, b_start: int, b_end: int, delete_inputs: bool) -> tuple[Chunk, int, int | None, str]:
    generate_input(chunk, b_start, b_end)
    result = compare_chunk(chunk)
    if delete_inputs and chunk.input_file.exists():
        chunk.input_file.unlink()
    return result


def load_progress() -> dict:
    # Progress is kept as JSON so the current state is human-readable and easy
    # to post-process later.
    if not PROGRESS_FILE.exists():
        return {}
    return json.loads(PROGRESS_FILE.read_text())


def save_progress(progress: dict) -> None:
    PROGRESS_FILE.write_text(json.dumps(progress, indent=2, sort_keys=True) + "\n")


def main() -> int:
    global OUT_DIR, INPUT_DIR, LOG_DIR, SUMMARY_DIR, EXECUTION_LOG, PROGRESS_FILE

    parser = argparse.ArgumentParser(description="Run the stage-3 FP16 exhaustive sweep in chunks.")
    parser.add_argument("--ops", default="add,sub,mul,div", help="Comma-separated list of operations")
    parser.add_argument("--a-block-size", default="0100", help="Chunk size for A in hex, default 0100")
    parser.add_argument("--b-start", default="0000", help="B range start in hex")
    parser.add_argument("--b-end", default="ffff", help="B range end in hex")
    parser.add_argument("--max-workers", type=int, default=1, help="Parallel chunk workers")
    parser.add_argument("--limit-op", help="Only run a single operation")
    parser.add_argument("--limit-chunks", type=int, default=0, help="Limit number of chunks per op")
    parser.add_argument(
        "--spread-chunks",
        type=int,
        default=0,
        help="Pick this many chunks evenly spread across the full A interval",
    )
    parser.add_argument("--out-dir", default=str(OUT_DIR), help="Directory for stage-3 outputs")
    parser.add_argument("--delete-inputs", action="store_true", help="Delete generated .hex chunks after comparison")
    parser.add_argument("--generate-only", action="store_true", help="Only generate chunk files")
    args = parser.parse_args()

    OUT_DIR = Path(args.out_dir)
    INPUT_DIR = OUT_DIR / "inputs"
    LOG_DIR = OUT_DIR / "logs"
    SUMMARY_DIR = OUT_DIR / "summaries"
    EXECUTION_LOG = OUT_DIR / "execution.log"
    PROGRESS_FILE = OUT_DIR / "progress.json"

    ops = [op.strip() for op in args.ops.split(",") if op.strip()]
    if args.limit_op:
        ops = [args.limit_op]

    a_block_size = parse_hex_u16(args.a_block_size)
    if a_block_size == 0:
        raise SystemExit("--a-block-size must be nonzero")
    b_start = parse_hex_u16(args.b_start)
    b_end = parse_hex_u16(args.b_end)
    if b_start > b_end:
        raise SystemExit("invalid B range")

    INPUT_DIR.mkdir(parents=True, exist_ok=True)
    LOG_DIR.mkdir(parents=True, exist_ok=True)
    SUMMARY_DIR.mkdir(parents=True, exist_ok=True)

    progress = load_progress()
    progress.setdefault("chunks", {})
    progress["b_start"] = f"{b_start:04x}"
    progress["b_end"] = f"{b_end:04x}"
    progress["a_block_size"] = f"{a_block_size:04x}"
    save_progress(progress)

    execution_log = EXECUTION_LOG.open("a", encoding="utf-8")
    execution_log.write(
        f"run ops={','.join(ops)} a_block_size={a_block_size:04x} b_start={b_start:04x} b_end={b_end:04x}\n"
    )
    execution_log.flush()

    total_generated = 0
    total_compared = 0
    total_mismatches = 0

    if args.limit_chunks and args.spread_chunks:
        raise SystemExit("--limit-chunks and --spread-chunks are mutually exclusive")

    for op in ops:
        chunks = build_chunks(op, a_block_size)
        if args.limit_chunks:
            chunks = chunks[: args.limit_chunks]
        elif args.spread_chunks:
            chunks = select_spread_chunks(chunks, args.spread_chunks)

        if args.generate_only:
            for chunk in chunks:
                generate_input(chunk, b_start, b_end)
                total_generated += 1
            continue

        # Parallelize across chunks only; each chunk is independent and keeps
        # the comparison logic simple.
        with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, args.max_workers)) as executor:
            futures = [
                executor.submit(process_chunk, chunk, b_start, b_end, args.delete_inputs)
                for chunk in chunks
            ]
            total_generated += len(chunks)
            for future in concurrent.futures.as_completed(futures):
                chunk, rc, mismatch_count, _ = future.result()
                total_compared += 1
                if rc != 0:
                    total_mismatches += 1
                progress["chunks"][f"{chunk.op}:{chunk.a_start_hex}-{chunk.a_end_hex}"] = {
                    "status": rc,
                    "mismatches": mismatch_count,
                    "input": str(chunk.input_file),
                    "log": str(chunk.log_file) if rc != 0 else "",
                }
                save_progress(progress)
                execution_log.write(
                    f"{chunk.op} {chunk.a_start_hex}..{chunk.a_end_hex} status={rc} mismatches={mismatch_count if mismatch_count is not None else 'unknown'}\n"
                )
                execution_log.flush()

        summary_file = SUMMARY_DIR / f"{op}.txt"
        with summary_file.open("w", encoding="utf-8") as fp:
            for chunk in chunks:
                status = progress["chunks"][f"{chunk.op}:{chunk.a_start_hex}-{chunk.a_end_hex}"]["status"]
                mismatches = progress["chunks"][f"{chunk.op}:{chunk.a_start_hex}-{chunk.a_end_hex}"].get("mismatches")
                fp.write(f"{op} chunk={chunk.a_start_hex}..{chunk.a_end_hex} status={status} mismatches={mismatches}\n")

    execution_log.write(
        f"complete generated={total_generated} compared={total_compared} mismatches={total_mismatches}\n"
    )
    execution_log.flush()
    execution_log.close()

    print(
        f"generated={total_generated} compared={total_compared} mismatches={total_mismatches}",
        flush=True,
    )
    return 0 if total_mismatches == 0 else 2


if __name__ == "__main__":
    raise SystemExit(main())
