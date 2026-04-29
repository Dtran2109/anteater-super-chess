#ifndef EVAL_H
#define EVAL_H

#include "types.h"

//precondition: Take in the game state and side color
//postcondition: Return the total evaluation score for that side
int evaluatePosition(const GameState *state, PieceColor side);

//precondition: Take in the game state and side color
//postcondition: Return the material score for that side
int evaluateMaterial(const GameState *state, PieceColor side);

//precondition: Take in the game state and side color
//postcondition: Return the mobility score for that side
int evaluateMobility(const GameState *state, PieceColor side);

//precondition: Take in the game state and side color
//postcondition: Return the pawn progress score for that side
int evaluatePawnProgress(const GameState *state, PieceColor side);

//precondition: Take in the game state and side color
//postcondition: Return the anteater activity score for that side
int evaluateAnteaterActivity(const GameState *state, PieceColor side);

//precondition: Take in the game state and side color
//postcondition: Return the king safety score for that side
int evaluateKingSafety(const GameState *state, PieceColor side);

/* Stage 4 feature upgrades */

//precondition: Take in the game state and side color
//postcondition: Return the hanging pieces score for that side
int evaluateHangingPieces(const GameState *state, PieceColor side);

//precondition: Take in the game state and side color
//postcondition: Return the king escape squares score for that side
int evaluateKingEscapeSquares(const GameState *state, PieceColor side);

//precondition: Take in the game state and side color
//postcondition: Return the anteater capture potential score for that side
int evaluateAnteaterCapturePotential(const GameState *state, PieceColor side);

//precondition: Take in the game state and side color
//postcondition: Return the passed pawns score for that side
int evaluatePassedPawns(const GameState *state, PieceColor side);

//precondition: Take in the game state and side color
//postcondition: Return the piece square table score for that side
int evaluatePieceSquareTables(const GameState *state, PieceColor side);

//static int getPieceSquareBonus(PieceType type, PieceColor color, int row, int col);

//precondition: Take in the piece square table scale percent
//postcondition: Set the piece square table scale percent
void setPstScalePercent(int percent);

//precondition: None
//postcondition: Return the current piece square table scale percent
int getPstScalePercent(void);

#endif