#!/usr/bin/env python3
"""
train_eval_residual_cuda.py

Material-anchored / residual trainer for Anteater Chess.

Instead of learning the whole evaluator from scratch, this script keeps the
existing handcrafted score as the base:

    base_score = sum(eval_* features)

and trains only a correction term:

    final_score = base_score + residual_correction

The generated C header writes weights for residual_correction. In eval.c, use:

    return score + (int)(EVAL_RESIDUAL_SCALE * learned);

where score is your handcrafted base score and learned is computed from the
EVAL_WEIGHT_* macros.
"""

import argparse
import csv
import math
import sys
from typing import Dict, List, Tuple

FEATURES = [
    "eval_material",
    "eval_mobility",
    "eval_pawn_progress",
    "eval_anteater_activity",
    "eval_king_safety",
    "eval_hanging_pieces",
    "eval_king_escape",
    "eval_anteater_capture_potential",
    "eval_passed_pawns",
    "eval_pawn_structure",
]

# By default, material stays in the base evaluator only.
# The residual model learns from the other features.
DEFAULT_RESIDUAL_FEATURES = [
    "eval_mobility",
    "eval_pawn_progress",
    "eval_anteater_activity",
    "eval_king_safety",
    "eval_hanging_pieces",
    "eval_king_escape",
    "eval_anteater_capture_potential",
    "eval_passed_pawns",
    "eval_pawn_structure",
]

HEADER_GUARD = "GENERATED_EVAL_WEIGHTS_H"


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Train material-anchored residual eval weights with PyTorch/CUDA.")
    p.add_argument("csv_file", help="Input CSV from selfplay_data")
    p.add_argument("output_header", help="Output generated_eval_weights.h")
    p.add_argument("--device", default="auto", choices=["auto", "cuda", "cpu"], help="Training device")
    p.add_argument("--epochs", type=int, default=1500, help="Training epochs")
    p.add_argument("--lr", type=float, default=0.03, help="Adam learning rate")
    p.add_argument("--test-ratio", type=float, default=0.20, help="Test split ratio")
    p.add_argument("--seed", type=int, default=23, help="Random seed")
    p.add_argument("--batch-size", type=int, default=0, help="Mini-batch size. 0 = full batch")
    p.add_argument("--temperature", type=float, default=900.0,
                   help="Centipawn-to-logit temperature. Larger = softer labels. Default 900")
    p.add_argument("--correction-l2", type=float, default=0.015,
                   help="Penalty on residual correction size. Higher = safer/smaller residual")
    p.add_argument("--include-material-residual", action="store_true",
                   help="Allow the residual model to also use eval_material. Off by default")
    p.add_argument("--residual-scale", type=float, default=1.0,
                   help="C-side multiplier written as EVAL_RESIDUAL_SCALE. Default 1.0")
    return p.parse_args()


def require_torch():
    try:
        import torch
        return torch
    except ImportError:
        print("ERROR: PyTorch is not installed.", file=sys.stderr)
        print("Check: python3 -c \"import torch; print(torch.cuda.is_available())\"", file=sys.stderr)
        sys.exit(1)


def sign_of_result(value: str) -> int:
    try:
        x = float(value)
    except Exception:
        return 0
    if x > 0:
        return 1
    if x < 0:
        return -1
    return 0


def load_csv(path: str) -> Tuple[List[List[float]], List[int], Dict[str, int]]:
    xs: List[List[float]] = []
    ys: List[int] = []
    stats = {
        "loaded": 0,
        "skipped_draw_unknown": 0,
        "bad_rows": 0,
        "side_white": 0,
        "side_black": 0,
        "side_won": 0,
        "side_lost": 0,
    }

    with open(path, "r", newline="") as f:
        reader = csv.DictReader(f)
        required = FEATURES + ["final_result_for_side"]
        missing = [name for name in required if name not in (reader.fieldnames or [])]
        if missing:
            raise ValueError(f"CSV is missing required columns: {missing}")

        for row in reader:
            result_sign = sign_of_result(row.get("final_result_for_side", "0"))
            if result_sign == 0:
                stats["skipped_draw_unknown"] += 1
                continue
            try:
                x = [float(row[name]) for name in FEATURES]
            except Exception:
                stats["bad_rows"] += 1
                continue

            side = row.get("side_to_move", "")
            if side == "WHITE":
                stats["side_white"] += 1
            elif side == "BLACK":
                stats["side_black"] += 1

            if result_sign > 0:
                y = 1
                stats["side_won"] += 1
            else:
                y = 0
                stats["side_lost"] += 1

            xs.append(x)
            ys.append(y)
            stats["loaded"] += 1

    return xs, ys, stats


def sign_accuracy(values: List[float], ys: List[int]) -> Tuple[float, int, int]:
    correct = 0
    total = 0
    zero = 0
    for v, y in zip(values, ys):
        sy = 1 if y == 1 else -1
        if v == 0:
            zero += 1
            continue
        if (v > 0 and sy > 0) or (v < 0 and sy < 0):
            correct += 1
        total += 1
    return (100.0 * correct / total if total else 0.0, correct, total)


def basic_stats(xs: List[List[float]], ys: List[int]) -> None:
    n = len(xs)
    if n == 0:
        return
    signed_y = [1 if y == 1 else -1 for y in ys]

    material_vals = [x[FEATURES.index("eval_material")] for x in xs]
    base_vals = [sum(x) for x in xs]
    mat_acc, mat_correct, mat_total = sign_accuracy(material_vals, ys)
    base_acc, base_correct, base_total = sign_accuracy(base_vals, ys)

    print(f"Material-only sign accuracy: {mat_acc:.2f}% ({mat_correct}/{mat_total})")
    print(f"Base-score sign accuracy:    {base_acc:.2f}% ({base_correct}/{base_total})")

    print("Feature/result correlations:")
    for j, name in enumerate(FEATURES):
        vals = [x[j] for x in xs]
        mean_x = sum(vals) / n
        mean_y = sum(signed_y) / n
        cov = sum((vals[i] - mean_x) * (signed_y[i] - mean_y) for i in range(n))
        var_x = sum((v - mean_x) ** 2 for v in vals)
        var_y = sum((v - mean_y) ** 2 for v in signed_y)
        corr = cov / math.sqrt(var_x * var_y) if var_x > 0 and var_y > 0 else 0.0
        print(f"  {name:<34} {corr:+.4f}")


def choose_device(torch, requested: str):
    if requested == "cpu":
        return torch.device("cpu")
    if requested == "cuda":
        if not torch.cuda.is_available():
            print("WARNING: --device cuda requested, but CUDA is not available. Falling back to CPU.")
            return torch.device("cpu")
        return torch.device("cuda")
    return torch.device("cuda" if torch.cuda.is_available() else "cpu")


def accuracy_from_scores(torch, scores_cp, y) -> float:
    logits = scores_cp >= 0
    preds = logits.float()
    return preds.eq(y).float().mean().item() * 100.0


def train_residual(torch, xs: List[List[float]], ys: List[int], args: argparse.Namespace):
    torch.manual_seed(args.seed)
    device = choose_device(torch, args.device)

    residual_names = list(FEATURES if args.include_material_residual else DEFAULT_RESIDUAL_FEATURES)
    residual_indices = [FEATURES.index(name) for name in residual_names]

    X_all = torch.tensor(xs, dtype=torch.float32)
    y_all = torch.tensor(ys, dtype=torch.float32).view(-1, 1)
    base_all = X_all.sum(dim=1, keepdim=True)  # handcrafted base score in centipawns-ish units
    X_resid_all = X_all[:, residual_indices]

    perm = torch.randperm(X_all.shape[0])
    X_resid_all = X_resid_all[perm]
    base_all = base_all[perm]
    y_all = y_all[perm]

    test_count = max(1, int(X_all.shape[0] * args.test_ratio))
    train_count = X_all.shape[0] - test_count

    X_train_raw = X_resid_all[:train_count]
    X_test_raw = X_resid_all[train_count:]
    base_train = base_all[:train_count]
    base_test = base_all[train_count:]
    y_train = y_all[:train_count]
    y_test = y_all[train_count:]

    mean = X_train_raw.mean(dim=0)
    std = X_train_raw.std(dim=0)
    std = torch.where(std < 1e-6, torch.ones_like(std), std)

    X_train = ((X_train_raw - mean) / std).to(device)
    X_test = ((X_test_raw - mean) / std).to(device)
    base_train = base_train.to(device)
    base_test = base_test.to(device)
    y_train = y_train.to(device)
    y_test = y_test.to(device)

    model = torch.nn.Linear(len(residual_names), 1).to(device)
    torch.nn.init.zeros_(model.weight)
    torch.nn.init.zeros_(model.bias)

    opt = torch.optim.Adam(model.parameters(), lr=args.lr)
    bce = torch.nn.BCEWithLogitsLoss()

    print(f"Residual features: {residual_names}")
    print(f"Device: {device}")
    if device.type == "cuda":
        print(f"CUDA device name: {torch.cuda.get_device_name(0)}")

    with torch.no_grad():
        base_train_acc = accuracy_from_scores(torch, base_train, y_train)
        base_test_acc = accuracy_from_scores(torch, base_test, y_test)
    print(f"Base-only split accuracy: train={base_train_acc:.2f}%, test={base_test_acc:.2f}%")

    batch_size = args.batch_size
    full_batch = batch_size <= 0 or batch_size >= train_count

    for epoch in range(1, args.epochs + 1):
        model.train()
        if full_batch:
            opt.zero_grad(set_to_none=True)
            correction_cp = model(X_train)
            logits = (base_train + correction_cp) / args.temperature
            loss = bce(logits, y_train)
            # Keep residual as a correction, not a replacement brain.
            loss = loss + args.correction_l2 * torch.mean((correction_cp / args.temperature) ** 2)
            loss.backward()
            opt.step()
        else:
            order = torch.randperm(train_count, device=device)
            total_loss = 0.0
            batches = 0
            for start in range(0, train_count, batch_size):
                idx = order[start:start + batch_size]
                xb = X_train[idx]
                bb = base_train[idx]
                yb = y_train[idx]
                opt.zero_grad(set_to_none=True)
                correction_cp = model(xb)
                logits = (bb + correction_cp) / args.temperature
                loss = bce(logits, yb)
                loss = loss + args.correction_l2 * torch.mean((correction_cp / args.temperature) ** 2)
                loss.backward()
                opt.step()
                total_loss += loss.item()
                batches += 1
            loss = torch.tensor(total_loss / max(1, batches), device=device)

        if epoch == 1 or epoch == args.epochs or epoch % max(1, args.epochs // 4) == 0:
            model.eval()
            with torch.no_grad():
                train_score = base_train + model(X_train)
                test_score = base_test + model(X_test)
                train_acc = accuracy_from_scores(torch, train_score, y_train)
                test_acc = accuracy_from_scores(torch, test_score, y_test)
                avg_abs_corr = torch.mean(torch.abs(model(X_train))).item()
            print(f"epoch {epoch:5d}/{args.epochs}: loss={loss.item():.6f}, "
                  f"train_acc={train_acc:.2f}%, test_acc={test_acc:.2f}%, "
                  f"avg_abs_correction={avg_abs_corr:.1f}")

    model.eval()
    with torch.no_grad():
        train_score = base_train + model(X_train)
        test_score = base_test + model(X_test)
        train_acc = accuracy_from_scores(torch, train_score, y_train)
        test_acc = accuracy_from_scores(torch, test_score, y_test)
        avg_abs_corr = torch.mean(torch.abs(model(X_train))).item()

    # Convert normalized residual weights back to raw C feature scale.
    w_norm = model.weight.detach().cpu().view(-1)
    b_norm = model.bias.detach().cpu().item()
    mean_cpu = mean.cpu()
    std_cpu = std.cpu()

    w_resid_raw = w_norm / std_cpu
    b_raw = b_norm - torch.sum(w_norm * mean_cpu / std_cpu).item()

    weights = {name: 0.0 for name in FEATURES}
    for i, name in enumerate(residual_names):
        weights[name] = float(w_resid_raw[i].item())

    bias = float(b_raw)
    return bias, weights, train_acc, test_acc, base_train_acc, base_test_acc, avg_abs_corr, str(device), residual_names


def write_header(path: str, bias: float, weights: Dict[str, float], args: argparse.Namespace,
                 train_acc: float, test_acc: float, base_train_acc: float, base_test_acc: float,
                 avg_abs_corr: float, device_name: str, residual_names: List[str]) -> None:
    with open(path, "w") as f:
        f.write("/* Auto-generated by train_eval_residual_cuda.py. Do not edit by hand. */\n")
        f.write("/* Material-anchored residual correction model. */\n")
        f.write(f"/* Base-only train/test accuracy: {base_train_acc:.2f}% / {base_test_acc:.2f}% */\n")
        f.write(f"/* Residual train/test accuracy:  {train_acc:.2f}% / {test_acc:.2f}% */\n")
        f.write(f"/* Average absolute residual correction: {avg_abs_corr:.2f} */\n")
        f.write(f"/* Residual features: {', '.join(residual_names)} */\n")
        f.write(f"/* Training device: {device_name} */\n")
        f.write(f"#ifndef {HEADER_GUARD}\n")
        f.write(f"#define {HEADER_GUARD}\n\n")
        f.write(f"#define EVAL_RESIDUAL_SCALE {args.residual_scale:.12f}\n")
        f.write(f"#define EVAL_BIAS {bias:.12f}\n")
        f.write(f"#define EVAL_WEIGHT_MATERIAL {weights['eval_material']:.12f}\n")
        f.write(f"#define EVAL_WEIGHT_MOBILITY {weights['eval_mobility']:.12f}\n")
        f.write(f"#define EVAL_WEIGHT_PAWN_PROGRESS {weights['eval_pawn_progress']:.12f}\n")
        f.write(f"#define EVAL_WEIGHT_ANTEATER_ACTIVITY {weights['eval_anteater_activity']:.12f}\n")
        f.write(f"#define EVAL_WEIGHT_KING_SAFETY {weights['eval_king_safety']:.12f}\n")
        f.write(f"#define EVAL_WEIGHT_HANGING_PIECES {weights['eval_hanging_pieces']:.12f}\n")
        f.write(f"#define EVAL_WEIGHT_KING_ESCAPE {weights['eval_king_escape']:.12f}\n")
        f.write(f"#define EVAL_WEIGHT_ANTEATER_CAPTURE_POTENTIAL {weights['eval_anteater_capture_potential']:.12f}\n")
        f.write(f"#define EVAL_WEIGHT_PASSED_PAWNS {weights['eval_passed_pawns']:.12f}\n")
        f.write(f"#define EVAL_WEIGHT_PAWN_STRUCTURE {weights['eval_pawn_structure']:.12f}\n\n")
        f.write(f"#endif /* {HEADER_GUARD} */\n")


def main() -> int:
    args = parse_args()
    torch = require_torch()

    xs, ys, stats = load_csv(args.csv_file)
    print(f"Loaded labeled samples: {stats['loaded']}")
    print(f"Skipped draw/unknown samples: {stats['skipped_draw_unknown']}")
    print(f"Bad rows skipped: {stats['bad_rows']}")
    print(f"Side counts: {{'WHITE': {stats['side_white']}, 'BLACK': {stats['side_black']}}}")
    print(f"Result counts: {{'side_to_move_won': {stats['side_won']}, 'side_to_move_lost': {stats['side_lost']}}}")

    if not xs:
        print("ERROR: no labeled samples found.", file=sys.stderr)
        return 1

    basic_stats(xs, ys)

    bias, weights, train_acc, test_acc, base_train_acc, base_test_acc, avg_abs_corr, device_name, residual_names = train_residual(torch, xs, ys, args)

    print("Final accuracy:")
    print(f"  Base train accuracy:     {base_train_acc:.2f}%")
    print(f"  Base test accuracy:      {base_test_acc:.2f}%")
    print(f"  Residual train accuracy: {train_acc:.2f}%")
    print(f"  Residual test accuracy:  {test_acc:.2f}%")
    print(f"  Avg abs correction:      {avg_abs_corr:.2f}")
    print("Raw C residual weights:")
    print(f"  EVAL_RESIDUAL_SCALE = {args.residual_scale:.12f}")
    print(f"  bias = {bias:.12f}")
    for name in FEATURES:
        print(f"  {name} = {weights[name]:.12f}")

    write_header(args.output_header, bias, weights, args, train_acc, test_acc, base_train_acc, base_test_acc, avg_abs_corr, device_name, residual_names)
    print(f"Wrote: {args.output_header}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
