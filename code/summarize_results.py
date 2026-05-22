#!/usr/bin/env python3
import argparse
import csv
from pathlib import Path


K_VALUES = [100, 200, 300, 400, 500]
S_VALUES = [10, 20, 30, 40, 50]
DEFAULT_K = 100
DEFAULT_S = 10


def parse_float(value):
    if value in ("", "-", "NA", None):
        return None
    try:
        return float(value)
    except ValueError:
        return None


def fmt(value):
    if value is None:
        return ""
    return f"{value:.6g}"


def add_row(rows, algorithm, dataset, k, seed_num, saved, time_seconds, approx_ratio=None, status="OK"):
    rows.append({
        "algorithm": algorithm,
        "dataset": dataset,
        "k": int(k),
        "seed_num": int(seed_num),
        "saved_nodes": saved,
        "time_seconds": time_seconds,
        "approx_ratio": approx_ratio,
        "status": status,
    })


def read_ag_results(results_dir):
    rows = []
    path = results_dir / "ag_gr_results.txt"
    if not path.exists():
        return rows

    with path.open("r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#") or line.startswith("=") or line.startswith("Format") or line == "---":
                continue
            parts = line.split("\t")
            if len(parts) < 9:
                continue
            algorithm, dataset, k, seed_num = parts[0], parts[1], parts[2], parts[3]
            saved = parse_float(parts[6])
            time_seconds = parse_float(parts[7])
            status = parts[8]
            if saved is None and status.startswith("SKIP"):
                saved = None
            add_row(rows, algorithm, dataset, k, seed_num, saved, time_seconds, status=status)
    return rows


def read_imin_results(results_dir):
    rows = []
    for path in results_dir.glob("res_*"):
        with path.open("r", encoding="utf-8", errors="ignore") as f:
            reader = csv.DictReader(f, delimiter="\t")
            if not reader.fieldnames or "algorithm" not in reader.fieldnames:
                continue
            for record in reader:
                result_type = record.get("result_type", "")
                algorithm = record.get("algorithm", "SandIMIN")
                if result_type:
                    algorithm = f"{algorithm}-{result_type}"
                add_row(
                    rows,
                    algorithm,
                    record.get("dataset", ""),
                    record.get("k", 0),
                    record.get("|S|", 0),
                    parse_float(record.get("saved_nodes")),
                    parse_float(record.get("time_seconds")),
                    parse_float(record.get("approx_ratio")),
                    record.get("status", "OK"),
                )
    return rows


def pivot(rows, fixed_field, fixed_value, varying_field, varying_values, metric):
    algorithms = []
    seen = set()
    for row in rows:
        if row[fixed_field] != fixed_value:
            continue
        key = row["algorithm"]
        if key not in seen:
            seen.add(key)
            algorithms.append(key)

    output = []
    for algorithm in algorithms:
        line = {"algorithm": algorithm}
        for value in varying_values:
            matches = [
                row for row in rows
                if row["algorithm"] == algorithm
                and row[fixed_field] == fixed_value
                and row[varying_field] == value
            ]
            line[str(value)] = fmt(matches[-1][metric]) if matches else ""
        output.append(line)
    return output


def write_pivot(path, rows, value_headers, first_header="Thuật toán"):
    with path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow([first_header] + value_headers)
        for row in rows:
            writer.writerow([row["algorithm"]] + [row.get(h, "") for h in value_headers])


def write_approx(path, rows):
    with path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["algorithm", "k", "|S|", "approx_ratio"])
        for row in rows:
            if row["approx_ratio"] is not None:
                writer.writerow([row["algorithm"], row["k"], row["seed_num"], fmt(row["approx_ratio"])])


def main():
    parser = argparse.ArgumentParser(description="Generate table-ready CSV files from experiment results.")
    parser.add_argument("--results", default="../results", help="Path to results directory")
    args = parser.parse_args()

    results_dir = Path(args.results).resolve()
    rows = read_ag_results(results_dir) + read_imin_results(results_dir)

    k_headers = [f"k={k}" for k in K_VALUES]
    s_headers = [f"|S|={s}" for s in S_VALUES]

    saved_by_k = pivot(rows, "seed_num", DEFAULT_S, "k", K_VALUES, "saved_nodes")
    time_by_k = pivot(rows, "seed_num", DEFAULT_S, "k", K_VALUES, "time_seconds")
    saved_by_s = pivot(rows, "k", DEFAULT_K, "seed_num", S_VALUES, "saved_nodes")
    time_by_s = pivot(rows, "k", DEFAULT_K, "seed_num", S_VALUES, "time_seconds")

    # Rename dict keys from raw numeric values to display headers.
    def display(rows_in, values, headers):
        converted = []
        for row in rows_in:
            item = {"algorithm": row["algorithm"]}
            for value, header in zip(values, headers):
                item[header] = row.get(str(value), "")
            converted.append(item)
        return converted

    write_pivot(results_dir / "table_k_saved.csv", display(saved_by_k, K_VALUES, k_headers), k_headers)
    write_pivot(results_dir / "table_k_time.csv", display(time_by_k, K_VALUES, k_headers), k_headers)
    write_pivot(results_dir / "table_s_saved.csv", display(saved_by_s, S_VALUES, s_headers), s_headers)
    write_pivot(results_dir / "table_s_time.csv", display(time_by_s, S_VALUES, s_headers), s_headers)
    write_approx(results_dir / "table_approx_ratio.csv", rows)

    print(f"Generated table CSV files in {results_dir}")
    print("  table_k_saved.csv")
    print("  table_k_time.csv")
    print("  table_s_saved.csv")
    print("  table_s_time.csv")
    print("  table_approx_ratio.csv")


if __name__ == "__main__":
    main()
