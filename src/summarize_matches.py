import argparse
import csv
from collections import defaultdict


def to_int(value, default=0):
    try:
        if value == "":
            return default
        return int(float(value))
    except (TypeError, ValueError):
        return default


def to_float(value, default=0.0):
    try:
        if value == "":
            return default
        return float(value)
    except (TypeError, ValueError):
        return default


def summarize_games(filename):
    stats = defaultdict(lambda: {
        "games": 0,
        "draws": 0,
        "white_wins": 0,
        "black_wins": 0,
        "plies": 0,
        "engine_wins": defaultdict(int),
        "engine_score": defaultdict(float),
        "end_reasons": defaultdict(int),
    })

    with open(filename, newline="") as f:
        reader = csv.DictReader(f)

        for row in reader:
            matchup = row["matchup"]
            white_engine = row["white_engine"]
            black_engine = row["black_engine"]
            winner = row["winner"]
            plies = to_int(row["plies"])
            reason = row["end_reason"]

            s = stats[matchup]
            s["games"] += 1
            s["plies"] += plies
            s["end_reasons"][reason] += 1

            if winner == "WHITE":
                s["white_wins"] += 1
                s["engine_wins"][white_engine] += 1
                s["engine_score"][white_engine] += 1.0
            elif winner == "BLACK":
                s["black_wins"] += 1
                s["engine_wins"][black_engine] += 1
                s["engine_score"][black_engine] += 1.0
            else:
                s["draws"] += 1
                s["engine_score"][white_engine] += 0.5
                s["engine_score"][black_engine] += 0.5

    print("\nGAME RESULTS")
    print("============")
    for matchup, s in stats.items():
        games = max(s["games"], 1)

        print()
        print(matchup)
        print("-" * len(matchup))
        print(f"Games: {s['games']}")
        print(f"Draws: {s['draws']}")
        print(f"White wins: {s['white_wins']}")
        print(f"Black wins: {s['black_wins']}")
        print(f"Average plies: {s['plies'] / games:.2f}")

        print("Engine scores:")
        for engine, score in sorted(s["engine_score"].items()):
            print(f"  {engine}: {score:.1f} / {s['games']}")

        print("End reasons:")
        for reason, count in sorted(s["end_reasons"].items()):
            print(f"  {reason}: {count}")


def summarize_moves(filename):
    stats = defaultdict(lambda: {
        "moves": 0,
        "depth": 0,
        "requested_depth": 0,
        "nodes": 0,
        "time_ms": 0.0,
        "nps": 0,
        "tt_hits": 0,
        "tt_cutoffs": 0,
        "full_depth": 0,
    })

    with open(filename, newline="") as f:
        reader = csv.DictReader(f)

        for row in reader:
            if to_int(row.get("random_opening", "0")):
                continue

            key = (row.get("engine", ""), row.get("side_to_move", ""))
            depth = to_int(row.get("search_depth", "0"))
            requested_depth = to_int(row.get("search_requested_depth", "0"))
            nodes = to_int(row.get("search_nodes", "0"))
            time_ms = to_float(row.get("search_time_ms", "0"))
            nps = to_int(row.get("search_nps", "0"))

            s = stats[key]
            s["moves"] += 1
            s["depth"] += depth
            s["requested_depth"] += requested_depth
            s["nodes"] += nodes
            s["time_ms"] += time_ms
            s["nps"] += nps
            s["tt_hits"] += to_int(row.get("search_tt_hits", "0"))
            s["tt_cutoffs"] += to_int(row.get("search_tt_cutoffs", "0"))

            if requested_depth > 0 and depth >= requested_depth:
                s["full_depth"] += 1

    print("\nSEARCH STATS")
    print("============")
    for (engine, side), s in sorted(stats.items()):
        moves = max(s["moves"], 1)
        full_depth_pct = 100.0 * s["full_depth"] / moves

        print()
        print(f"{engine} {side}")
        print("-" * (len(engine) + len(side) + 1))
        print(f"Moves: {s['moves']}")
        print(f"Average depth: {s['depth'] / moves:.2f} / {s['requested_depth'] / moves:.2f}")
        print(f"Full-depth rate: {full_depth_pct:.1f}%")
        print(f"Average nodes: {s['nodes'] / moves:.0f}")
        print(f"Average time: {s['time_ms'] / moves:.2f} ms")
        print(f"Average NPS: {s['nps'] / moves:.0f}")
        print(f"Average TT hits: {s['tt_hits'] / moves:.0f}")
        print(f"Average TT cutoffs: {s['tt_cutoffs'] / moves:.0f}")


def main():
    parser = argparse.ArgumentParser(description="Summarize engine match CSV files.")
    parser.add_argument("--games", default="combined_games.csv", help="Game CSV path")
    parser.add_argument("--moves", default=None, help="Move CSV path with search stats")
    parser.add_argument("--skip-games", action="store_true", help="Only summarize moves")
    args = parser.parse_args()

    if not args.skip_games:
        summarize_games(args.games)

    if args.moves is not None:
        summarize_moves(args.moves)


if __name__ == "__main__":
    main()
