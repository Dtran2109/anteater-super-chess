#ifndef CPU_H
#define CPU_H

#include "types.h"
#include "game.h"

typedef struct
{
    Move bestMove;
    PieceColor side;
    int requestedDepth;
    int completedDepth;
    int score;
    unsigned long nodes;
    unsigned long long totalNodes;
    double timeMs;
    double averageTimeMs;
    unsigned long nps;
    unsigned long ttHits;
    unsigned long ttCutoffs;
    unsigned long long totalTtHits;
    unsigned long long totalTtCutoffs;
} CpuSearchStats;

//precondition: Take in the game state and CPU side color
//postcondition: Return a random legal move for the CPU side
Move chooseRandomMove(const GameState *state, PieceColor cpuSide);

//precondition: Take in the game state and CPU side color
//postcondition: Return a greedy legal move for the CPU side
Move chooseGreedyMove(const GameState *state, PieceColor cpuSide);

//precondition: Take in the game state and CPU side color
//postcondition: Return the best move found by the CPU search
Move chooseBestMove(const GameState *state, PieceColor cpuSide);

//precondition: Take in the game state, CPU side color, and search depth
//postcondition: Return the best move found by the CPU search at the requested depth
Move chooseBestMoveDepth(const GameState *state, PieceColor cpuSide, int depth);

//precondition: Take in a positive millisecond limit
//postcondition: Set the CPU search time limit used for deep searches
void setCpuSearchTimeLimitMs(int milliseconds);

//precondition: Take in a preset name: fast, balanced, or strong
//postcondition: Apply the requested CPU search preset
void setCpuSearchPreset(const char *presetName);

//precondition: Take in a positive megabyte target
//postcondition: Resize and clear the CPU transposition table; return actual MB allocated, or 0 on failure
unsigned long setCpuTranspositionTableSizeMb(int megabytes);

//precondition: None
//postcondition: Return the currently allocated CPU transposition table size in MB
unsigned long getCpuTranspositionTableSizeMb(void);

//precondition: None
//postcondition: Return the active CPU search preset name
const char *getCpuSearchPresetName(void);

//precondition: Take in an output pointer
//postcondition: Fill output with the most recent CPU search statistics
void getLastCpuSearchStats(CpuSearchStats *stats);

//precondition: Take in the piece type
//postcondition: Return the value of that piece type
int getPieceValue(PieceType type);

#endif
