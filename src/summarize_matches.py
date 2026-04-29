import csv
from collections import defaultdict

filename = "combined_games.csv"

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
        plies = int(row["plies"])
        reason = row["end_reason"]

        s = stats[matchup]
        s["games"] += 1
        s["plies"] += plies
        s["end_reasons"][reason] += 1

        if winner == "WHITE":
            s["white_wins"] += 1
            s["engine_wins"][white_engine] += 1
            s["engine_score"][white_engine] += 1.0
            s["engine_score"][black_engine] += 0.0
        elif winner == "BLACK":
            s["black_wins"] += 1
            s["engine_wins"][black_engine] += 1
            s["engine_score"][black_engine] += 1.0
            s["engine_score"][white_engine] += 0.0
        else:
            s["draws"] += 1
            s["engine_score"][white_engine] += 0.5
            s["engine_score"][black_engine] += 0.5

for matchup, s in stats.items():
    print()
    print(matchup)
    print("-" * len(matchup))
    print(f"Games: {s['games']}")
    print(f"Draws: {s['draws']}")
    print(f"White wins: {s['white_wins']}")
    print(f"Black wins: {s['black_wins']}")
    print(f"Average plies: {s['plies'] / s['games']:.2f}")

    print("Engine wins:")
    for engine, wins in sorted(s["engine_wins"].items()):
        print(f"  {engine}: {wins}")

    print("Engine scores:")
    for engine, score in sorted(s["engine_score"].items()):
        print(f"  {engine}: {score:.1f} / {s['games']}")

    print("End reasons:")
    for reason, count in sorted(s["end_reasons"].items()):
        print(f"  {reason}: {count}")
