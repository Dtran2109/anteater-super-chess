#include "board.h"
#include "moves.h"
#include <string.h>

static const ChessPiece EMPTY_SQUARE = { EMPTY, NONE, 0 };

ChessPiece makePiece(PieceType type, PieceColor color) {
    ChessPiece p;
    p.type = type;
    p.color = (type == EMPTY) ? NONE : color;
    p.MoveCount = 0;
    return p;
}

ChessPiece makeEmptyPiece(void) {
    return EMPTY_SQUARE;
}

PieceColor oppositeColor(PieceColor color) {
    if (color == WHITE) return BLACK;
    if (color == BLACK) return WHITE;
    return NONE;
}

int isPositionInBounds(Position pos) {
    return pos.row >= 0 && pos.row < BOARD_ROWS && pos.col >= 0 && pos.col < BOARD_COLS;
}

int isSquareEmpty(const GameState *state, Position pos) {
    return state && isPositionInBounds(pos) && state->squares[pos.row][pos.col].type == EMPTY;
}

ChessPiece getPieceAt(const GameState *state, Position pos) {
    if (!state || !isPositionInBounds(pos)) return EMPTY_SQUARE;
    return state->squares[pos.row][pos.col];
}

void setPieceAt(GameState *state, Position pos, ChessPiece piece) {
    if (!state || !isPositionInBounds(pos)) return;
    state->squares[pos.row][pos.col] = piece;
}

void clearGameState(GameState *state) {
    if (!state) return;
    for (int r = 0; r < BOARD_ROWS; ++r) {
        for (int c = 0; c < BOARD_COLS; ++c) state->squares[r][c] = EMPTY_SQUARE;
    }
    state->currentTurn = WHITE;
    state->whiteKingPos = (Position){-1, -1};
    state->blackKingPos = (Position){-1, -1};
    state->enPassantTarget = (Position){-1, -1};
    state->whiteCastleKingSide = 0;
    state->whiteCastleQueenSide = 0;
    state->blackCastleKingSide = 0;
    state->blackCastleQueenSide = 0;
    state->halfmoveClock = 0;
    state->fullmoveNumber = 1;
}

void initializeGameState(GameState *state) {
    int c;

    clearGameState(state);

    static const PieceType backRank[10] = {
        ROOK, KNIGHT, BISHOP, ANTEATER, QUEEN,
        KING, ANTEATER, BISHOP, KNIGHT, ROOK
    };

    /* Black starts at the top: rows 0 and 1 */
    for (c = 0; c < BOARD_COLS; ++c) {
        state->squares[0][c].type      = backRank[c];
        state->squares[0][c].color     = BLACK;
        state->squares[0][c].MoveCount = 0;

        state->squares[1][c].type      = PAWN;
        state->squares[1][c].color     = BLACK;
        state->squares[1][c].MoveCount = 0;

        /* White starts at the bottom: rows 6 and 7 */
        state->squares[6][c].type      = PAWN;
        state->squares[6][c].color     = WHITE;
        state->squares[6][c].MoveCount = 0;

        state->squares[7][c].type      = backRank[c];
        state->squares[7][c].color     = WHITE;
        state->squares[7][c].MoveCount = 0;
    }

    state->blackKingPos.row = 0;
    state->blackKingPos.col = 5;

    state->whiteKingPos.row = 7;
    state->whiteKingPos.col = 5;

    state->whiteCastleKingSide  = 1;
    state->whiteCastleQueenSide = 1;
    state->blackCastleKingSide  = 1;
    state->blackCastleQueenSide = 1;

    state->currentTurn = WHITE;
}
void copyGameState(GameState *dest, const GameState *src) {
    if (!dest || !src) return;
    memcpy(dest, src, sizeof(GameState));
}

static void fillRecord(const GameState *state, Move move, MoveRecord *rec) {
    if (!rec) return;
    memset(rec, 0, sizeof(*rec));
    rec->move = move;
    rec->movedBefore = getPieceAt(state, move.start);
    rec->captured = getPieceAt(state, move.dest);
    rec->prevEnPassantTarget = state->enPassantTarget;
    rec->prevWhiteCastleKingSide = state->whiteCastleKingSide;
    rec->prevWhiteCastleQueenSide = state->whiteCastleQueenSide;
    rec->prevBlackCastleKingSide = state->blackCastleKingSide;
    rec->prevBlackCastleQueenSide = state->blackCastleQueenSide;
    rec->prevWhiteKingPos = state->whiteKingPos;
    rec->prevBlackKingPos = state->blackKingPos;
    rec->prevHalfmoveClock = state->halfmoveClock;
    rec->prevFullmoveNumber = state->fullmoveNumber;
    rec->prevTurn = state->currentTurn;
}

static void removeCastlingRightsForPiece(GameState *state, ChessPiece piece, Position from, Position to) {
    if (piece.type == KING) {
        if (piece.color == WHITE) {
            state->whiteCastleKingSide = 0;
            state->whiteCastleQueenSide = 0;
        } else if (piece.color == BLACK) {
            state->blackCastleKingSide = 0;
            state->blackCastleQueenSide = 0;
        }
    }

    if (piece.type == ROOK) {
        if (from.row == 7 && from.col == 0) state->whiteCastleQueenSide = 0;
        if (from.row == 7 && from.col == BOARD_COLS - 1) state->whiteCastleKingSide = 0;
        if (from.row == 0 && from.col == 0) state->blackCastleQueenSide = 0;
        if (from.row == 0 && from.col == BOARD_COLS - 1) state->blackCastleKingSide = 0;
    }

    if (to.row == 7 && to.col == 0) state->whiteCastleQueenSide = 0;
    if (to.row == 7 && to.col == BOARD_COLS - 1) state->whiteCastleKingSide = 0;
    if (to.row == 0 && to.col == 0) state->blackCastleQueenSide = 0;
    if (to.row == 0 && to.col == BOARD_COLS - 1) state->blackCastleKingSide = 0;
}

static void applyMoveCore(GameState *state, Move move, MoveRecord *rec) {
    if (!state || !isPositionInBounds(move.start) || !isPositionInBounds(move.dest)) return;

    ChessPiece moving = getPieceAt(state, move.start);
    ChessPiece captured = getPieceAt(state, move.dest);
    if (moving.type == EMPTY) return;

    fillRecord(state, move, rec);

    Position antCaptured[MAX_CAPTURED_PAWNS];
    int antCount = 0;
    if (moving.type == ANTEATER && captured.type == PAWN && captured.color != moving.color) {
        antCount = collectAnteaterCapturePawns(state, move.start, move.dest, antCaptured, MAX_CAPTURED_PAWNS);
        if (rec) {
            rec->capturedPawnCount = antCount;
            for (int i = 0; i < antCount; ++i) rec->capturedPawns[i] = antCaptured[i];
        }
    }

    int wasEnPassant = (moving.type == PAWN && captured.type == EMPTY && move.start.col != move.dest.col &&
                        move.dest.row == state->enPassantTarget.row && move.dest.col == state->enPassantTarget.col);
    if (rec) rec->wasEnPassant = wasEnPassant;

    state->squares[move.start.row][move.start.col] = EMPTY_SQUARE;

    if (wasEnPassant) {
        Position pawnPos = { move.start.row, move.dest.col };
        if (rec) rec->captured = getPieceAt(state, pawnPos);
        state->squares[pawnPos.row][pawnPos.col] = EMPTY_SQUARE;
    }

    for (int i = 0; i < antCount; ++i) {
        state->squares[antCaptured[i].row][antCaptured[i].col] = EMPTY_SQUARE;
    }

    int wasCastle = (moving.type == KING && move.start.row == move.dest.row && (move.dest.col - move.start.col == 2 || move.dest.col - move.start.col == -2));
    if (wasCastle) {
        int row = move.start.row;
        int kingSide = move.dest.col > move.start.col;
        Position rookFrom = { row, kingSide ? BOARD_COLS - 1 : 0 };
        Position rookTo = { row, kingSide ? move.dest.col - 1 : move.dest.col + 1 };
        if (rec) {
            rec->wasCastleKing = kingSide;
            rec->wasCastleQueen = !kingSide;
            rec->castledRookBefore = getPieceAt(state, rookFrom);
        }
        state->squares[rookTo.row][rookTo.col] = state->squares[rookFrom.row][rookFrom.col];
        state->squares[rookTo.row][rookTo.col].MoveCount++;
        state->squares[rookFrom.row][rookFrom.col] = EMPTY_SQUARE;
    }

    moving.MoveCount++;
    if (moving.type == PAWN && move.dest.row == (moving.color == WHITE ? 0 : 7)) {
        moving.type = (move.promotionGoal == EMPTY) ? QUEEN : move.promotionGoal;
    }

    state->squares[move.dest.row][move.dest.col] = moving;
    if (moving.type == KING) {
        if (moving.color == WHITE) state->whiteKingPos = move.dest;
        else state->blackKingPos = move.dest;
    }

    removeCastlingRightsForPiece(state, moving, move.start, move.dest);

    state->enPassantTarget = (Position){-1, -1};
    if (moving.type == PAWN && (move.dest.row - move.start.row == 2 || move.start.row - move.dest.row == 2)) {
        state->enPassantTarget.row = (move.start.row + move.dest.row) / 2;
        state->enPassantTarget.col = move.start.col;
    }

    if (moving.type == PAWN || captured.type != EMPTY || antCount > 0 || wasEnPassant) state->halfmoveClock = 0;
    else state->halfmoveClock++;

    if (state->currentTurn == BLACK) state->fullmoveNumber++;
    state->currentTurn = oppositeColor(state->currentTurn);
}

void applyMove(GameState *state, Move move) {
    applyMoveCore(state, move, NULL);
}

void applyMoveRecord(GameState *state, Move move, MoveRecord *out) {
    applyMoveCore(state, move, out);
}

void undoMove(GameState *state, const MoveRecord *rec) {
    if (!state || !rec) return;
    Move move = rec->move;

    state->currentTurn = rec->prevTurn;
    state->enPassantTarget = rec->prevEnPassantTarget;
    state->whiteCastleKingSide = rec->prevWhiteCastleKingSide;
    state->whiteCastleQueenSide = rec->prevWhiteCastleQueenSide;
    state->blackCastleKingSide = rec->prevBlackCastleKingSide;
    state->blackCastleQueenSide = rec->prevBlackCastleQueenSide;
    state->whiteKingPos = rec->prevWhiteKingPos;
    state->blackKingPos = rec->prevBlackKingPos;
    state->halfmoveClock = rec->prevHalfmoveClock;
    state->fullmoveNumber = rec->prevFullmoveNumber;

    state->squares[move.start.row][move.start.col] = rec->movedBefore;
    state->squares[move.dest.row][move.dest.col] = rec->captured;

    if (rec->wasEnPassant) {
        Position pawnPos = { move.start.row, move.dest.col };
        state->squares[move.dest.row][move.dest.col] = EMPTY_SQUARE;
        state->squares[pawnPos.row][pawnPos.col] = rec->captured;
    }

    if (rec->capturedPawnCount > 0) {
        for (int i = 0; i < rec->capturedPawnCount; ++i) {
            state->squares[rec->capturedPawns[i].row][rec->capturedPawns[i].col] = makePiece(PAWN, oppositeColor(rec->movedBefore.color));
        }
        state->squares[move.dest.row][move.dest.col] = rec->captured;
    }

    if (rec->wasCastleKing || rec->wasCastleQueen) {
        int row = move.start.row;
        int kingSide = rec->wasCastleKing;
        Position rookFrom = { row, kingSide ? BOARD_COLS - 1 : 0 };
        Position rookTo = { row, kingSide ? move.dest.col - 1 : move.dest.col + 1 };
        state->squares[rookFrom.row][rookFrom.col] = rec->castledRookBefore;
        state->squares[rookTo.row][rookTo.col] = EMPTY_SQUARE;
    }
}
