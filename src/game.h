#ifndef GAME_H
#define GAME_H

#include "types.h"

int isValidPosition(Position pos);
int positionsEqual(Position a, Position b);
int isSquareAttacked(const GameState *state, Position square, PieceColor bySide);
int isKingInCheck(const GameState *state, PieceColor side);
int isMoveLegal(const GameState *state, Move move);
void generateAllLegalMoves(const GameState *state, PieceColor side, MoveList *list);
int isCheckmate(const GameState *state, PieceColor side);
int isStalemate(const GameState *state, PieceColor side);
int isKingDead(const GameState *state, PieceColor side);
const char *getResultMessage(const GameState *state);
#endif
