#ifndef CPU_H
#define CPU_H

#include "types.h"
#include "game.h"

//precondition: Take in the game state and CPU side color
//postcondition: Return a random legal move for the CPU side
Move chooseRandomMove(const GameState *state, PieceColor cpuSide);

//precondition: Take in the game state and CPU side color
//postcondition: Return a greedy legal move for the CPU side
Move chooseGreedyMove(const GameState *state, PieceColor cpuSide);

//precondition: Take in the game state and CPU side color
//postcondition: Return the best move found by the CPU search
Move chooseBestMove(const GameState *state, PieceColor cpuSide);

//precondition: Take in the piece type
//postcondition: Return the value of that piece type
int getPieceValue(PieceType type);

#endif