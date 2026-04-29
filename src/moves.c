#include "moves.h"
#include "board.h"
#include <string.h>

void clearMoveList(MoveList *list) {
    if (list) list->count = 0;
}

void addMove(MoveList *list, Position from, Position to, PieceType promotionType) {
    if (!list || list->count >= MAX_MOVES) return;
    list->moves[list->count].start = from;
    list->moves[list->count].dest = to;
    list->moves[list->count].promotionGoal = promotionType;
    list->count++;
}

static void generateSlidingMoves(const GameState *state, Position from, MoveList *list, const int directions[][2], int count) {
    ChessPiece piece = getPieceAt(state, from);
    for (int i = 0; i < count; ++i) {
        Position to = { from.row + directions[i][0], from.col + directions[i][1] };
        while (isPositionInBounds(to)) {
            ChessPiece target = getPieceAt(state, to);
            if (target.type == EMPTY) {
                addMove(list, from, to, EMPTY);
            } else {
                if (target.color != piece.color && target.type != KING) addMove(list, from, to, EMPTY);
                break;
            }
            to.row += directions[i][0];
            to.col += directions[i][1];
        }
    }
}

void generatePawnMoves(const GameState *state, Position from, MoveList *list) {
    ChessPiece piece = getPieceAt(state, from);
    if (piece.type != PAWN) return;

    int dir = (piece.color == WHITE) ? -1 : 1;
    int startRow = (piece.color == WHITE) ? 6 : 1;
    int promotionRow = (piece.color == WHITE) ? 0 : 7;
    Position one = { from.row + dir, from.col };

    if (isPositionInBounds(one) && isSquareEmpty(state, one)) {
        if (one.row == promotionRow) {
            addMove(list, from, one, QUEEN);
            addMove(list, from, one, ROOK);
            addMove(list, from, one, BISHOP);
            addMove(list, from, one, KNIGHT);
            addMove(list, from, one, ANTEATER);
        } else {
            addMove(list, from, one, EMPTY);
        }

        Position two = { from.row + 2 * dir, from.col };
        if (from.row == startRow && isPositionInBounds(two) && isSquareEmpty(state, two)) {
            addMove(list, from, two, EMPTY);
        }
    }

    for (int dc = -1; dc <= 1; dc += 2) {
        Position to = { from.row + dir, from.col + dc };
        if (!isPositionInBounds(to)) continue;
        ChessPiece target = getPieceAt(state, to);
        int isEnPassant = (state->enPassantTarget.row == to.row && state->enPassantTarget.col == to.col);
        if (((target.type != EMPTY && target.color == oppositeColor(piece.color) && target.type != KING)) || isEnPassant) {
            if (to.row == promotionRow) {
                addMove(list, from, to, QUEEN);
                addMove(list, from, to, ROOK);
                addMove(list, from, to, BISHOP);
                addMove(list, from, to, KNIGHT);
                addMove(list, from, to, ANTEATER);
            } else {
                addMove(list, from, to, EMPTY);
            }
        }
    }
}

void generateRookMoves(const GameState *state, Position from, MoveList *list) {
    static const int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
    if (getPieceAt(state, from).type == ROOK) generateSlidingMoves(state, from, list, dirs, 4);
}

void generateBishopMoves(const GameState *state, Position from, MoveList *list) {
    static const int dirs[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
    if (getPieceAt(state, from).type == BISHOP) generateSlidingMoves(state, from, list, dirs, 4);
}

void generateQueenMoves(const GameState *state, Position from, MoveList *list) {
    static const int dirs[8][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
    if (getPieceAt(state, from).type == QUEEN) generateSlidingMoves(state, from, list, dirs, 8);
}

void generateKnightMoves(const GameState *state, Position from, MoveList *list) {
    ChessPiece piece = getPieceAt(state, from);
    if (piece.type != KNIGHT) return;
    static const int jumps[8][2] = {{2,1},{2,-1},{-2,1},{-2,-1},{1,2},{1,-2},{-1,2},{-1,-2}};
    for (int i = 0; i < 8; ++i) {
        Position to = { from.row + jumps[i][0], from.col + jumps[i][1] };
        if (!isPositionInBounds(to)) continue;
        ChessPiece target = getPieceAt(state, to);
        if (target.type == EMPTY || (target.color != piece.color && target.type != KING)) addMove(list, from, to, EMPTY);
    }
}

void generateKingMoves(const GameState *state, Position from, MoveList *list) {
    ChessPiece piece = getPieceAt(state, from);
    if (piece.type != KING) return;
    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            if (dr == 0 && dc == 0) continue;
            Position to = { from.row + dr, from.col + dc };
            if (!isPositionInBounds(to)) continue;
            ChessPiece target = getPieceAt(state, to);
            if (target.type == EMPTY || (target.color != piece.color && target.type != KING)) addMove(list, from, to, EMPTY);
        }
    }
    if (piece.MoveCount == 0) {
        Position kingSide = { from.row, from.col + 2 };
        Position queenSide = { from.row, from.col - 2 };
        if (isPositionInBounds(kingSide)) addMove(list, from, kingSide, EMPTY);
        if (isPositionInBounds(queenSide)) addMove(list, from, queenSide, EMPTY);
    }
}

int collectAnteaterCapturePawns(const GameState *state, Position from, Position dest, Position captured[], int maxCaptured) {
    ChessPiece ant = getPieceAt(state, from);
    ChessPiece first = getPieceAt(state, dest);
    if (ant.type != ANTEATER || first.type != PAWN || first.color == ant.color || first.color == NONE) return 0;

    int visited[BOARD_ROWS][BOARD_COLS];
    memset(visited, 0, sizeof(visited));
    Position queue[BOARD_ROWS * BOARD_COLS];
    int head = 0, tail = 0, count = 0;

    queue[tail++] = dest;
    visited[dest.row][dest.col] = 1;

    while (head < tail) {
        Position cur = queue[head++];
        if (count < maxCaptured) captured[count] = cur;
        count++;

        for (int dr = -1; dr <= 1; ++dr) {
            for (int dc = -1; dc <= 1; ++dc) {
                if (dr == 0 && dc == 0) continue;
                Position n = { cur.row + dr, cur.col + dc };
                if (!isPositionInBounds(n) || visited[n.row][n.col]) continue;
                ChessPiece p = getPieceAt(state, n);
                if (p.type == PAWN && p.color == first.color) {
                    visited[n.row][n.col] = 1;
                    queue[tail++] = n;
                }
            }
        }
    }
    return (count > maxCaptured) ? maxCaptured : count;
}

void generateAntEaterMoves(const GameState *state, Position from, MoveList *list) {
    ChessPiece piece = getPieceAt(state, from);
    if (piece.type != ANTEATER) return;

    Position component[MAX_CAPTURED_PAWNS];
    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            if (dr == 0 && dc == 0) continue;
            Position to = { from.row + dr, from.col + dc };
            if (!isPositionInBounds(to)) continue;
            ChessPiece target = getPieceAt(state, to);
            if (target.type == EMPTY) {
                addMove(list, from, to, EMPTY);
            } else if (target.type == PAWN && target.color != piece.color) {
                int n = collectAnteaterCapturePawns(state, from, to, component, MAX_CAPTURED_PAWNS);
                for (int i = 0; i < n; ++i) {
                    int duplicate = 0;
                    for (int j = 0; j < list->count; ++j) {
                        if (list->moves[j].start.row == from.row &&
                            list->moves[j].start.col == from.col &&
                            list->moves[j].dest.row == component[i].row &&
                            list->moves[j].dest.col == component[i].col) {
                            duplicate = 1;
                            break;
                        }
                    }
                    if (!duplicate) addMove(list, from, component[i], EMPTY);
                }
            }
        }
    }
}

void generatePieceMoves(const GameState *state, Position from, MoveList *list) {
    clearMoveList(list);
    switch (getPieceAt(state, from).type) {
        case PAWN: generatePawnMoves(state, from, list); break;
        case ROOK: generateRookMoves(state, from, list); break;
        case BISHOP: generateBishopMoves(state, from, list); break;
        case KNIGHT: generateKnightMoves(state, from, list); break;
        case QUEEN: generateQueenMoves(state, from, list); break;
        case KING: generateKingMoves(state, from, list); break;
        case ANTEATER: generateAntEaterMoves(state, from, list); break;
        default: break;
    }
}

static int moveAlreadyInList(const MoveList *list, Move move) {
    for (int i = 0; i < list->count; ++i) {
        Move m = list->moves[i];
        if (m.start.row == move.start.row && m.start.col == move.start.col &&
            m.dest.row == move.dest.row && m.dest.col == move.dest.col &&
            m.promotionGoal == move.promotionGoal) return 1;
    }
    return 0;
}

void generateAllPseudoLegalMoves(const GameState *state, PieceColor side, MoveList *list) {
    clearMoveList(list);
    for (int r = 0; r < BOARD_ROWS; ++r) {
        for (int c = 0; c < BOARD_COLS; ++c) {
            Position from = { r, c };
            ChessPiece p = getPieceAt(state, from);
            if (p.type == EMPTY || p.color != side) continue;
            MoveList temp;
            generatePieceMoves(state, from, &temp);
            for (int i = 0; i < temp.count; ++i) {
                if (!moveAlreadyInList(list, temp.moves[i])) {
                    addMove(list, temp.moves[i].start, temp.moves[i].dest, temp.moves[i].promotionGoal);
                }
            }
        }
    }
}
