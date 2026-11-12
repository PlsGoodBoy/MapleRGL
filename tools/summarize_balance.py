#!/usr/bin/env python3
"""Summarize actual simulate_balance JSON runs, with explicit censoring.

python tools/summarize_balance.py docs/simulation_combat_runs.json
Outputs sibling *_summary.json, *_runs.csv and *_skills.csv. No dependencies.
"""
import argparse
import csv
import hashlib
import json
import math
from collections import defaultdict
from pathlib import Path
from statistics import mean, median


def average(rows, field):
    return mean(float(r[field]) for r in rows) if rows else None


def wilson(wins, n):
    """Descriptive binomial interval only; shared-seed samples are correlated."""
    if not n:
        return [None, None]
    z = 1.96
    p = wins / n
    denominator = 1 + z * z / n
    center = (p + z * z / (2 * n)) / denominator
    half = z * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n)) / denominator
    return [max(0, center - half), min(1, center + half)]


def summarize(rows):
    wins = sum(r["outcome"] == "victory" for r in rows)
    spawned = [r for r in rows if r["boss_spawned"]]
    defeated = [r for r in rows if r["boss_defeated"]]
    return {
        "runs": len(rows),
        "wins": wins,
        "win_rate": wins / len(rows),
        "descriptive_wilson_95": wilson(wins, len(rows)),
        "survival_seconds_mean": average(rows, "survival_seconds"),
        "survival_seconds_median": median(r["survival_seconds"] for r in rows),
        "survival_seconds_min": min(r["survival_seconds"] for r in rows),
        "survival_seconds_max": max(r["survival_seconds"] for r in rows),
        "level_mean": average(rows, "level"),
        "level_min": min(r["level"] for r in rows),
        "level_max": max(r["level"] for r in rows),
        "kills_mean": average(rows, "kills"),
        "damage_taken_mean": average(rows, "damage_taken"),
        "altar_health_spent_mean": average(rows, "altar_health_spent"),
        "xp_acquired_mean": average(rows, "xp_acquired"),
        "events_started_mean": average(rows, "events_started"),
        "events_succeeded_mean": average(rows, "events_succeeded"),
        "events_failed_mean": average(rows, "events_failed"),
        "events_active_at_end": sum(bool(r["event_active_at_end"]) for r in rows),
        "boss_spawned_runs": len(spawned),
        "boss_defeated_runs": len(defeated),
        "boss_defeat_rate_given_spawn": len(defeated) / len(spawned) if spawned else None,
        "boss_fight_seconds_mean_for_kills": mean(r["boss_defeat_time"] - r["boss_spawn_time"] for r in defeated) if defeated else None,
        "boss_damage_taken_mean_given_spawn": average(spawned, "boss_damage_taken"),
        "survive_without_boss_kill_wins": sum(r["outcome"] == "victory" and not r["boss_defeated"] for r in rows),
        "xp_by_minute_all_runs_mean": [mean(r["xp_by_minute"][i] for r in rows) for i in range(10)],
        "minute_observations": [
            {
                "minute": i + 1,
                "runs_reaching_minute": sum(r["minute_exposure_seconds"][i] > 0 for r in rows),
                "runs_completing_minute": sum(r["minute_exposure_seconds"][i] >= 59.5 for r in rows),
                "total_exposure_seconds": sum(r["minute_exposure_seconds"][i] for r in rows),
                "xp_per_60_alive_seconds": (
                    sum(r["xp_by_minute"][i] for r in rows) * 60 /
                    sum(r["minute_exposure_seconds"][i] for r in rows)
                    if sum(r["minute_exposure_seconds"][i] for r in rows) else None
                ),
                "level_mean_for_survivors": mean(
                    r["level_by_minute"][i] for r in rows if r["minute_exposure_seconds"][i] >= 59.5
                ) if any(r["minute_exposure_seconds"][i] >= 59.5 for r in rows) else None,
            } for i in range(10)
        ],
        "wall_ms_mean": average(rows, "wall_ms"),
        "peak_enemies_max": max(r["peak_enemies"] for r in rows),
        "evolved_runs": sum(any(s["evolved"] for s in r["skills"]) for r in rows),
    }


def group(rows, keys):
    groups = defaultdict(list)
    for row in rows:
        groups["/".join(str(row[k]) for k in keys)].append(row)
    return {key: summarize(value) for key, value in sorted(groups.items())}


def validate(report):
    rows = report.get("runs", [])
    if not report.get("complete"):
        raise ValueError("Batch is incomplete; no completed-batch summary will be generated")
    if not rows:
        raise ValueError("No runs")
    seen = set()
    expected = report.get("expected_runs", report["runs_per_class"] * 3)
    if len(rows) != expected:
        raise ValueError(f"Expected {expected} runs, got {len(rows)}")
    for row in rows:
        identity = tuple(row[k] for k in ("class", "seed", "movement", "upgrade_policy"))
        if identity in seen:
            raise ValueError(f"Duplicate run: {identity}")
        seen.add(identity)
        if row["outcome"] not in ("victory", "defeat"):
            raise ValueError(f"Non-terminal outcome: {identity}")
        if row["outcome"] == "victory" and row["survival_seconds"] < 599.9 and not row["boss_defeated"]:
            raise ValueError(f"Unexplained early victory: {identity}")
        if row["outcome"] == "defeat" and row["hp"] > 0:
            raise ValueError(f"Defeat with positive HP: {identity}")
        if abs(sum(row["xp_by_minute"]) - row["xp_acquired"]) > .1:
            raise ValueError(f"XP accounting mismatch: {identity}")
        if abs(sum(row["minute_exposure_seconds"]) - row["survival_seconds"]) > .1:
            raise ValueError(f"Timeline accounting mismatch: {identity}")
        if row["events_succeeded"] + row["events_failed"] > row["events_started"]:
            raise ValueError(f"Event accounting mismatch: {identity}")
        for skill in row["skills"]:
            if skill["damage"] < 0 or (not skill["level"] and skill["damage"] > 0):
                raise ValueError(f"Skill damage without ownership: {identity}")
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("--prefix", type=Path, help="Output file prefix; defaults to input minus _runs.json")
    args = parser.parse_args()
    raw = args.input.read_bytes()
    report = json.loads(raw.decode("utf-8-sig"))
    rows = validate(report)
    stem = args.input.stem.removesuffix("_runs")
    prefix = args.prefix or args.input.with_name(stem)
    summary = {
        "schema_version": 1,
        "source": args.input.name,
        "source_sha256": hashlib.sha256(raw).hexdigest(),
        "source_complete": True,
        "sample_count": len(rows),
        "seed_count": report["seed_count"],
        "first_seed": report["first_seed"],
        "fixed_simulation_dt_seconds": report["simulation_dt_seconds"],
        "decision_frequency_hz": report["decision_frequency_hz"],
        "limitations": report["limitations"] + [
            "Wilson intervals are descriptive only, not independent-sample confidence guarantees.",
            "Skill damage shares mix acquisition time, upgrade level, survivorship and policy selection; do not rank skill balance from shares alone.",
            "Wall time includes controller and diagnostics, excludes graphics/audio, and is not rendered FPS.",
        ],
        "overall": summarize(rows),
        "by_class": group(rows, ["class"]),
        "by_class_and_upgrade": group(rows, ["class", "upgrade_policy"]),
        "by_class_and_movement": group(rows, ["class", "movement"]),
        "by_policy": group(rows, ["class", "movement", "upgrade_policy"]),
        "skills": [],
        "event_types": [],
    }
    event_types = defaultdict(lambda: {"started": 0, "succeeded": 0, "failed": 0})
    for row in rows:
        for event in row.get("event_types", []):
            counts = event_types[(event["id"], event["name"])]
            for field in counts:
                counts[field] += event[field]
    summary["event_types"] = [{"id": key[0], "name": key[1], **value} for key, value in sorted(event_types.items())]
    skills = defaultdict(list)
    class_damage = defaultdict(float)
    for row in rows:
        class_damage[row["class"]] += sum(s["damage"] for s in row["skills"]) + row["other_damage"]
        for skill in row["skills"]:
            skills[(row["class"], skill["id"], skill["name"])].append(skill)
    for (cls, skill_id, name), values in sorted(skills.items()):
        damage = sum(s["damage"] for s in values)
        exposure = sum(s["owned_seconds"] for s in values)
        owners = [s for s in values if s["level"]]
        summary["skills"].append({
            "class": cls, "skill_id": skill_id, "name": name,
            "runs": len(values), "owned_runs": len(owners),
            "evolved_runs": sum(s["evolved"] for s in values),
            "final_level_mean_given_owned": mean(s["level"] for s in owners) if owners else None,
            "actual_damage_total": damage,
            "class_damage_share": damage / class_damage[cls] if class_damage[cls] else 0,
            "owned_seconds_total": exposure,
            "actual_damage_per_owned_second": damage / exposure if exposure else None,
        })
    output = Path(str(prefix) + "_summary.json")
    output.write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    scalar_fields = [k for k, v in rows[0].items() if not isinstance(v, (list, dict))]
    flat_fields = scalar_fields + [f"xp_minute_{i+1}" for i in range(10)] + [f"alive_seconds_minute_{i+1}" for i in range(10)]
    csv_path = Path(str(prefix) + "_runs.csv")
    with csv_path.open("w", newline="", encoding="utf-8-sig") as file:
        writer = csv.DictWriter(file, fieldnames=flat_fields)
        writer.writeheader()
        for row in rows:
            flat = {k: row[k] for k in scalar_fields}
            flat.update({f"xp_minute_{i+1}": row["xp_by_minute"][i] for i in range(10)})
            flat.update({f"alive_seconds_minute_{i+1}": row["minute_exposure_seconds"][i] for i in range(10)})
            writer.writerow(flat)
    with Path(str(prefix) + "_skills.csv").open("w", newline="", encoding="utf-8-sig") as file:
        writer = csv.DictWriter(file, fieldnames=list(summary["skills"][0]))
        writer.writeheader()
        writer.writerows(summary["skills"])
    print(f"Validated {len(rows)} actual runs. Wrote {output}, {csv_path} and skill CSV.")
    for cls, data in summary["by_class"].items():
        print(f"{cls}: {data['wins']}/{data['runs']} wins, survival {data['survival_seconds_mean']:.1f}s, "
              f"level {data['level_mean']:.1f}, kills {data['kills_mean']:.1f}, "
              f"boss {data['boss_defeated_runs']}/{data['boss_spawned_runs']} killed/spawned")


if __name__ == "__main__":
    main()


