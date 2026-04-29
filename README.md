---
description: Use when working on anteater chess - a chess variant with special rules for the anteater piece that captures multiple pawns
applyTo: "src/**"
---

# Anteater Chess Implementation Guide

## Project Overview

This is a modified chess game (8 rows × 10 columns) featuring a new piece called the **anteater** - an aggressive pawn-eating piece positioned between the king and bishops on each side.

## Board Configuration

- **Dimensions**: 8 rows × 10 columns (A-J, 1-8)
- **Pieces per side**: 2 anteaters (at D and G in starting position)
- **Standard setup**:
  - Row 8: `bR | bN | bB | bA | bQ | bK | bA | bB | bN | bR`
  - Row 7: `bP` (×10 black pawns)
  - Row 2: `wP` (×10 white pawns)
  - Row 1: `wR | wN | wB | wA | wQ | wK | wA | wB | wN | wR`

## Anteater Piece Rules

### Piece Notation
- `wA` = white anteater
- `bA` = black anteater

### Movement
- Moves **one step at a time** in any direction (like a king: horizontally, vertically, diagonally)
- **CANNOT capture** any other pieces besides pawns
- **NOT a threat** to the opposing king

### Captures (Ant Eating)
- Captures **ONLY pawns** (ants)
- **Unique ability**: Can capture **multiple pawns in a single move**
- When eating a pawn, the anteater also eats **all adjacent pawns** (horizontally, vertically, and diagonally connected)
- The capture path forms a continuous chain of adjacent pawns

### Example Moves
- Anteater at G3: Can move to 8 possible targets (all adjacent squares)
- Anteater at D6: Can move to C7 or D7, OR capture pawns on highlighted squares (E7, F7, G7, G6) in one move - leaving H7 untouched
- Multi-capture: Move from D6 to G6 by eating pawns at E7, F7, G7, and G6

## Code Structure

### Key Files
- `board.c/h` - Board state and piece management
- `moves.c/h` - Move validation and generation
- `cpu.c/h` - AI opponent logic
- `game.c/h` - Game state and turn management
- `gui.c/h` - Display and user interface
- `types.h` - Type definitions (piece types, board representation)

### Implementation Notes

**For Move Generation**:
- Anteater moves must check 8 directions from current position
- Capture logic must implement flood-fill or recursive collection of adjacent pawns
- Ensure captured pawns form a connected group

**For Game Logic**:
- Validate that anteaters only capture pawns, never other pieces
- Allow multiple pawn captures in one move when pawns are adjacent
- Board state must track captured pieces correctly

**For Testing**:
- Verify single-square moves work for all 8 directions
- Test multi-pawn captures with various adjacent pawn configurations
- Ensure anteater cannot capture non-pawn pieces
- Verify anteater is safely ignored in king threat calculations

## Build & Run

```bash
make debug     # Build with debugging symbols
make clean     # Clean build artifacts
```

## Types & Constants

Refer to `types.h` for piece type definitions and board constants.