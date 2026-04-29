#ifndef MOVES_H
#define MOVES_H

#include "types.h"

void clearMoveList(MoveList *list);
void addMove(MoveList *list, Position from, Position to, PieceType promotionType);
void generatePawnMoves(const GameState *state, Position from, MoveList *list);
void generateRookMoves(const GameState *state, Position from, MoveList *list);
void generateBishopMoves(const GameState *state, Position from, MoveList *list);
void generateKnightMoves(const GameState *state, Position from, MoveList *list);
void generateQueenMoves(const GameState *state, Position from, MoveList *list);
void generateKingMoves(const GameState *state, Position from, MoveList *list);
void generateAntEaterMoves(const GameState *state, Position from, MoveList *list);
void generatePieceMoves(const GameState *state, Position from, MoveList *list);
void generateAllPseudoLegalMoves(const GameState *state, PieceColor side, MoveList *list);
int collectAnteaterCapturePawns(const GameState *state, Position from, Position dest, Position captured[], int maxCaptured);

#endif
