#!/usr/bin/env python3
"""Run moshare bdec candidate audit on selected public cases."""

import subprocess
import csv
import os
import re
import sys
import time

ABC = "./abc"

STUBBORN_CASES = [7, 8, 9, 15, 16, 17, 22, 23, 26]
COMPARISON_CASES = [11, 14, 25, 30]
ALL_CASES = list(range(1, 31))

BDEC_FLAGS = "-maxcut 6 -maxwin 64 -maxcand 1000 -stats"
ANSI_RE = re.compile(r"\x1b\[[0-9;]*m")
PRINT_STATS_RE = re.compile(
    r"i/o\s*=\s*(?P<pi>\d+)\s*/\s*(?P<po>\d+).*?"
    r"and\s*=\s*(?P<and>\d+).*?"
    r"lev\s*=\s*(?P<lev>\d+)"
)
NODES_LINE_RE = re.compile(
    r"nodes\s+(?P<before>\d+)\s*->\s*(?P<after>\d+)\s+"
    r"levels\s+(?P<lev_before>\d+)\s*->\s*(?P<lev_after>\d+)"
)

def run_case(case_num):
    case_dir = f"tc_public/tc_public_{case_num}"
    blif_in = f"{case_dir}/input.blif"
    blif_out = f"claude_run/moshare_bdec_tc{case_num:02d}.blif"
    os.makedirs("claude_run", exist_ok=True)

    cmd = (
        f'read_blif {blif_in}; strash; print_stats;'
        f' moshare -algo bdec {BDEC_FLAGS}; print_stats;'
        f' write_blif {blif_out}'
    )

    t0 = time.time()
    result = subprocess.run(
        [ABC, "-c", cmd],
        capture_output=True, text=True, timeout=120
    )
    elapsed = time.time() - t0

    output = result.stdout + result.stderr

    # Parse stats
    stats = {"case": f"tc_public_{case_num}", "runtime_s": f"{elapsed:.3f}", "cec": "SKIP"}

    print_stats_seen = []
    for raw_line in output.split("\n"):
        line = ANSI_RE.sub("", raw_line).strip()
        m_stats = PRINT_STATS_RE.search(line)
        if m_stats:
            print_stats_seen.append((int(m_stats.group("and")), int(m_stats.group("lev"))))
        if "windows tried" in line:
            try:
                stats["windows_tried"] = int(line.split("=")[-1].strip())
            except ValueError:
                pass
        if "multi-output windows" in line and "single" not in line:
            try:
                stats["windows_multi"] = int(line.split("=")[-1].strip())
            except ValueError:
                pass
        if "single-output skipped" in line:
            try:
                stats["windows_single"] = int(line.split("=")[-1].strip())
            except ValueError:
                pass
        if "small windows" in line:
            try:
                stats["windows_small"] = int(line.split("=")[-1].strip())
            except ValueError:
                pass
        if "truth failed windows" in line:
            try:
                stats["truth_failed"] = int(line.split("=")[-1].strip())
            except ValueError:
                pass
        if "truth outputs" in line:
            try:
                stats["truth_outputs"] = int(line.split("=")[-1].strip())
            except ValueError:
                pass
        if "divisors raw/unique/multi" in line:
            try:
                parts = line.split("=")[-1].strip().split("/")
                stats["div_raw"] = int(parts[0].strip())
                stats["div_unique"] = int(parts[1].strip())
                stats["div_multi"] = int(parts[2].strip())
            except ValueError:
                pass
        if "existing divisor matches" in line:
            try:
                stats["div_existing"] = int(line.split("=")[-1].strip())
            except ValueError:
                pass
        if "decomposable divisors" in line:
            try:
                stats["div_decomp"] = int(line.split("=")[-1].strip())
            except ValueError:
                pass
        if "replacements tried/accepted" in line:
            try:
                parts = line.split("=")[-1].strip().split("/")
                stats["repl_tried"] = int(parts[0].strip())
                stats["repl_accepted"] = int(parts[1].strip())
            except (IndexError, ValueError):
                pass
        if "reject_profit" in line:
            try:
                stats["reject_profit"] = int(line.split("=")[-1].strip())
            except ValueError:
                pass
        if "reject_level" in line:
            try:
                stats["reject_level"] = int(line.split("=")[-1].strip())
            except ValueError:
                pass
        if "best audit score" in line:
            try:
                stats["best_score"] = int(line.split("=")[-1].strip())
            except ValueError:
                pass
        if "candidate cap hits" in line:
            try:
                stats["cap_hits"] = int(line.split("=")[-1].strip())
            except ValueError:
                pass
        m_nodes = NODES_LINE_RE.search(line)
        if "moshare (algo bdec): nodes" in line and m_nodes:
            stats["and_before"] = int(m_nodes.group("before"))
            stats["and_after"] = int(m_nodes.group("after"))
            stats["lev_before"] = int(m_nodes.group("lev_before"))
            stats["lev_after"] = int(m_nodes.group("lev_after"))

    if print_stats_seen:
        stats.setdefault("and_before", print_stats_seen[0][0])
        stats.setdefault("lev_before", print_stats_seen[0][1])
        stats.setdefault("and_after", print_stats_seen[-1][0])
        stats.setdefault("lev_after", print_stats_seen[-1][1])

    # Run CEC
    cec_cmd = f"cec {blif_in} {blif_out}"
    cec_result = subprocess.run(
        [ABC, "-c", cec_cmd],
        capture_output=True, text=True, timeout=30
    )
    if "equivalent" in cec_result.stdout.lower():
        stats["cec"] = "PASS"
    elif "equivalent" in cec_result.stderr.lower():
        stats["cec"] = "PASS"
    else:
        stats["cec"] = "FAIL"

    return stats


def main():
    cases = sys.argv[1:] if len(sys.argv) > 1 else STUBBORN_CASES + COMPARISON_CASES

    fieldnames = [
        "case", "runtime_s", "cec",
        "and_before", "and_after", "lev_before", "lev_after",
        "windows_tried", "windows_multi", "windows_single", "windows_small",
        "truth_failed", "truth_outputs",
        "div_raw", "div_unique", "div_multi", "div_existing", "div_decomp",
        "repl_tried", "repl_accepted", "reject_profit", "reject_level",
        "best_score", "cap_hits"
    ]

    results = []
    for c in cases:
        case_num = int(c)
        print(f"Running tc_public_{case_num}...", file=sys.stderr)
        try:
            s = run_case(case_num)
            results.append(s)
            print(f"  cec={s['cec']} windows_multi={s.get('windows_multi','?')} best={s.get('best_score','?')}", file=sys.stderr)
        except Exception as e:
            print(f"  FAILED: {e}", file=sys.stderr)
            results.append({"case": f"tc_public_{case_num}", "runtime_s": "ERR", "cec": "ERR"})

    # Write CSV
    csv_path = "claude_doc/moshare_bdec_candidate_audit.csv"
    with open(csv_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(results)

    print(f"\nResults written to {csv_path}", file=sys.stderr)

    # Print summary
    n_pass = sum(1 for r in results if r.get("cec") == "PASS")
    n_multi = sum(1 for r in results if int(r.get("windows_multi", 0) or 0) > 0)
    n_improved = sum(
        1 for r in results
        if r.get("and_before") not in (None, "") and r.get("and_after") not in (None, "")
        and int(r["and_after"]) < int(r["and_before"])
    )
    print(f"CEC: {n_pass}/{len(results)} pass", file=sys.stderr)
    print(f"Multi-output windows found in {n_multi} cases", file=sys.stderr)
    print(f"AND-count improvements found in {n_improved} cases", file=sys.stderr)


if __name__ == "__main__":
    main()
