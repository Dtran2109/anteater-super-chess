#ifndef BOARD_H
#define BOARD_H

#include "types.h"

typedef struct {
    Move move;
    ChessPiece movedBefore;
    ChessPiece captured;
    ChessPiece castledRookBefore;
    Position capturedPawns[MAX_CAPTURED_PAWNS];
    int capturedPawnCount;
    int wasEnPassant;
    int wasCastleKing;
    int wasCastleQueen;
    Position prevEnPassantTarget;
    int prevWhiteCastleKingSide;
    int prevWhiteCastleQueenSide;
    int prevBlackCastleKingSide;
    int prevBlackCastleQueenSide;
    Position prevWhiteKingPos;
    Position prevBlackKingPos;
    int prevHalfmoveClock;
    int prevFullmoveNumber;
    PieceColor prevTurn;
} MoveRecord;

ChessPiece makePiece(PieceType type, PieceColor color);
ChessPiece makeEmptyPiece(void);
int isPositionInBounds(Position pos);
int isSquareEmpty(const GameState *state, Position pos);
ChessPiece getPieceAt(const GameState *state, Position pos);
void setPieceAt(GameState *state, Position pos, ChessPiece piece);
void clearGameState(GameState *state);
void initializeGameState(GameState *state);
void copyGameState(GameState *dest, const GameState *src);
PieceColor oppositeColor(PieceColor color);
void applyMove(GameState *state, Move move);
void applyMoveRecord(GameState *state, Move move, MoveRecord *out);
void undoMove(GameState *state, const MoveRecord *rec);

#endif
