// Name: Abdulwahab Aldeyyeain
// Course: EECS 22
// Assignment: Chess Game Project
// Description: Implementation of game logic including move legality,
// check detection, and game rules.

#include "game.h"
#include "board.h"
#include "moves.h"
#include <stdlib.h>

//precondition: Take in an integer
//postcondition: Return the absolute value of the integer
static int absInt(int x)
{
    if (x < 0)
    {
        return -x;
    }

    return x;
}

//precondition: Take in the pos structure
//postcondition: Return integer if the position is valid
int isValidPosition(Position pos)
{
    return isPositionInBounds(pos);
}

//precondition: Take in two position structures
//postcondition: Return integer if both positions are the same
int positionsEqual(Position a, Position b)
{
    if (a.row != b.row)
    {
        return 0;
    }

    if (a.col != b.col)
    {
        return 0;
    }

    return 1;
}

//precondition: Take in the game state, start position, and end position
//postcondition: Return integer if the path between the two positions is clear
static int isPathClear(const GameState *state, Position from, Position to)
{
    int dr;
    int dc;
    int sr;
    int sc;
    Position cur;

    dr = to.row - from.row;
    dc = to.col - from.col;

    sr = 0;
    sc = 0;

    if (dr > 0)
    {
        sr = 1;
    }

    if (dr < 0)
    {
        sr = -1;
    }

    if (dc > 0)
    {
        sc = 1;
    }

    if (dc < 0)
    {
        sc = -1;
    }

    cur.row = from.row + sr;
    cur.col = from.col + sc;

    while (!positionsEqual(cur, to))
    {
        if (!isPositionInBounds(cur))
        {
            return 0;
        }

        if (!isSquareEmpty(state, cur))
        {
            return 0;
        }

        cur.row = cur.row + sr;
        cur.col = cur.col + sc;
    }

    return 1;
}

//precondition: Take in the side color
//postcondition: Return the pawn moving direction for that side
static int pawnDirection(PieceColor side)
{
    if (side == WHITE)
    {
        return -1;
    }

    return 1;
}

//precondition: Take in the game state and side color
//postcondition: Return the king position of that side
static Position getKingPosition(const GameState *state, PieceColor side)
{
    if (side == WHITE)
    {
        return state->whiteKingPos;
    }

    return state->blackKingPos;
}

//precondition: Take in the game state, target square, and attacking side
//postcondition: Return integer if the square is attacked by that side
int isSquareAttacked(const GameState *state, Position square, PieceColor bySide)
{
    int r;
    int c;

    if (state == NULL)
    {
        return 0;
    }

    if (!isPositionInBounds(square))
    {
        return 0;
    }

    if (bySide != WHITE)
    {
        if (bySide != BLACK)
        {
            return 0;
        }
    }

    for (r = 0; r < BOARD_ROWS; r++)
    {
        for (c = 0; c < BOARD_COLS; c++)
        {
            Position from;
            ChessPiece piece;
            int dr;
            int dc;
            int adr;
            int adc;

            from.row = r;
            from.col = c;

            piece = getPieceAt(state, from);

            if (piece.type == EMPTY)
            {
                continue;
            }

            if (piece.color != bySide)
            {
                continue;
            }

            dr = square.row - from.row;
            dc = square.col - from.col;
            adr = absInt(dr);
            adc = absInt(dc);

            if (piece.type == PAWN)
            {
                if (dr == pawnDirection(bySide))
                {
                    if (dc == -1)
                    {
                        return 1;
                    }

                    if (dc == 1)
                    {
                        return 1;
                    }
                }
            }

            if (piece.type == KNIGHT)
            {
                if (adr == 2)
                {
                    if (adc == 1)
                    {
                        return 1;
                    }
                }

                if (adr == 1)
                {
                    if (adc == 2)
                    {
                        return 1;
                    }
                }
            }

            if (piece.type == KING)
            {
                if (adr <= 1)
                {
                    if (adc <= 1)
                    {
                        if (adr != 0 || adc != 0)
                        {
                            return 1;
                        }
                    }
                }
            }

            if (piece.type == ROOK)
            {
                if (dr == 0 || dc == 0)
                {
                    if (isPathClear(state, from, square))
                    {
                        return 1;
                    }
                }
            }

            if (piece.type == BISHOP)
            {
                if (adr == adc)
                {
                    if (isPathClear(state, from, square))
                    {
                        return 1;
                    }
                }
            }

            if (piece.type == QUEEN)
            {
                if (dr == 0 || dc == 0)
                {
                    if (isPathClear(state, from, square))
                    {
                        return 1;
                    }
                }

                if (adr == adc)
                {
                    if (isPathClear(state, from, square))
                    {
                        return 1;
                    }
                }
            }

            /*
             * Anteaters do not attack or check kings in this variant.
             */
        }
    }

    return 0;
}

//precondition: Take in the game state and side color
//postcondition: Return integer if the king is in check
int isKingInCheck(const GameState *state, PieceColor side)
{
    Position king;

    king = getKingPosition(state, side);

    if (!isPositionInBounds(king))
    {
        return 1;
    }

    return isSquareAttacked(state, king, oppositeColor(side));
}

//precondition: Take in the piece type
//postcondition: Return integer if the piece type is allowed for promotion
static int isPromotionPiece(PieceType type)
{
    if (type == QUEEN)
    {
        return 1;
    }

    if (type == ROOK)
    {
        return 1;
    }

    if (type == BISHOP)
    {
        return 1;
    }

    if (type == KNIGHT)
    {
        return 1;
    }

    if (type == ANTEATER)
    {
        return 1;
    }

    return 0;
}

//precondition: Take in the game state, move, and pawn piece
//postcondition: Return integer if the pawn movement pattern is legal
static int isPawnMovePatternLegal(const GameState *state, Move move, ChessPiece piece)
{
    int dir;
    int startRow;
    int promotionRow;
    int dr;
    int dc;
    ChessPiece dest;

    dir = pawnDirection(piece.color);

    if (piece.color == WHITE)
    {
        startRow = 6;
        promotionRow = 0;
    }
    else
    {
        startRow = 1;
        promotionRow = 7;
    }

    dr = move.dest.row - move.start.row;
    dc = move.dest.col - move.start.col;

    dest = getPieceAt(state, move.dest);

    if (move.dest.row == promotionRow)
    {
        if (!isPromotionPiece(move.promotionGoal))
        {
            return 0;
        }
    }
    else
    {
        if (move.promotionGoal != EMPTY)
        {
            return 0;
        }
    }

    if (dc == 0)
    {
        if (dr == dir)
        {
            if (dest.type == EMPTY)
            {
                return 1;
            }
        }
    }

    if (dc == 0)
    {
        if (dr == 2 * dir)
        {
            if (move.start.row == startRow)
            {
                Position mid;

                mid.row = move.start.row + dir;
                mid.col = move.start.col;

                if (dest.type == EMPTY)
                {
                    if (isSquareEmpty(state, mid))
                    {
                        return 1;
                    }
                }
            }
        }
    }

    if (absInt(dc) == 1)
    {
        if (dr == dir)
        {
            if (dest.type != EMPTY)
            {
                if (dest.color == oppositeColor(piece.color))
                {
                    return 1;
                }
            }

            if (dest.type == EMPTY)
            {
                if (state->enPassantTarget.row == move.dest.row)
                {
                    if (state->enPassantTarget.col == move.dest.col)
                    {
                        Position pawnPos;
                        ChessPiece pawn;

                        pawnPos.row = move.start.row;
                        pawnPos.col = move.dest.col;

                        pawn = getPieceAt(state, pawnPos);

                        if (pawn.type == PAWN)
                        {
                            if (pawn.color == oppositeColor(piece.color))
                            {
                                return 1;
                            }
                        }
                    }
                }
            }
        }
    }

    return 0;
}

//precondition: Take in the game state and move
//postcondition: Return integer if the rook movement pattern is legal
static int isRookMovePatternLegal(const GameState *state, Move move)
{
    if (move.start.row == move.dest.row)
    {
        if (isPathClear(state, move.start, move.dest))
        {
            return 1;
        }
    }

    if (move.start.col == move.dest.col)
    {
        if (isPathClear(state, move.start, move.dest))
        {
            return 1;
        }
    }

    return 0;
}

//precondition: Take in the game state and move
//postcondition: Return integer if the bishop movement pattern is legal
static int isBishopMovePatternLegal(const GameState *state, Move move)
{
    int dr;
    int dc;

    dr = absInt(move.dest.row - move.start.row);
    dc = absInt(move.dest.col - move.start.col);

    if (dr == dc)
    {
        if (isPathClear(state, move.start, move.dest))
        {
            return 1;
        }
    }

    return 0;
}

//precondition: Take in the move
//postcondition: Return integer if the knight movement pattern is legal
static int isKnightMovePatternLegal(Move move)
{
    int dr;
    int dc;

    dr = absInt(move.dest.row - move.start.row);
    dc = absInt(move.dest.col - move.start.col);

    if (dr == 2)
    {
        if (dc == 1)
        {
            return 1;
        }
    }

    if (dr == 1)
    {
        if (dc == 2)
        {
            return 1;
        }
    }

    return 0;
}

//precondition: Take in the game state and move
//postcondition: Return integer if the queen movement pattern is legal
static int isQueenMovePatternLegal(const GameState *state, Move move)
{
    if (isRookMovePatternLegal(state, move))
    {
        return 1;
    }

    if (isBishopMovePatternLegal(state, move))
    {
        return 1;
    }

    return 0;
}

//precondition: Take in the game state, move, and king piece
//postcondition: Return integer if castling is legal
static int isKingCastleLegal(const GameState *state, Move move, ChessPiece king)
{
    int delta;
    int kingSide;
    int row;
    int rookCol;
    ChessPiece rook;
    int step;
    int col;
    int i;

    if (king.MoveCount != 0)
    {
        return 0;
    }

    if (move.start.row != move.dest.row)
    {
        return 0;
    }

    delta = move.dest.col - move.start.col;

    if (delta != 2)
    {
        if (delta != -2)
        {
            return 0;
        }
    }

    if (isKingInCheck(state, king.color))
    {
        return 0;
    }

    if (delta > 0)
    {
        kingSide = 1;
    }
    else
    {
        kingSide = 0;
    }

    row = move.start.row;

    if (kingSide)
    {
        rookCol = BOARD_COLS - 1;
    }
    else
    {
        rookCol = 0;
    }

    rook = state->squares[row][rookCol];

    if (rook.type != ROOK)
    {
        return 0;
    }

    if (rook.color != king.color)
    {
        return 0;
    }

    if (rook.MoveCount != 0)
    {
        return 0;
    }

    if (king.color == WHITE)
    {
        if (kingSide)
        {
            if (!state->whiteCastleKingSide)
            {
                return 0;
            }
        }
        else
        {
            if (!state->whiteCastleQueenSide)
            {
                return 0;
            }
        }
    }
    else
    {
        if (kingSide)
        {
            if (!state->blackCastleKingSide)
            {
                return 0;
            }
        }
        else
        {
            if (!state->blackCastleQueenSide)
            {
                return 0;
            }
        }
    }

    if (kingSide)
    {
        step = 1;
    }
    else
    {
        step = -1;
    }

    col = move.start.col + step;

    while (col != rookCol)
    {
        Position p;

        p.row = row;
        p.col = col;

        if (!isSquareEmpty(state, p))
        {
            return 0;
        }

        col = col + step;
    }

    for (i = 1; i <= 2; i++)
    {
        Position p;

        p.row = row;
        p.col = move.start.col + i * step;

        if (isSquareAttacked(state, p, oppositeColor(king.color)))
        {
            return 0;
        }
    }

    return 1;
}

//precondition: Take in the game state, move, and king piece
//postcondition: Return integer if the king movement pattern is legal
static int isKingMovePatternLegal(const GameState *state, Move move, ChessPiece king)
{
    int dr;
    int dc;

    dr = absInt(move.dest.row - move.start.row);
    dc = absInt(move.dest.col - move.start.col);

    if (dr <= 1)
    {
        if (dc <= 1)
        {
            if (dr != 0 || dc != 0)
            {
                return 1;
            }
        }
    }

    return isKingCastleLegal(state, move, king);
}

//precondition: Take in the game state, move, and anteater piece
//postcondition: Return integer if the anteater movement pattern is legal
static int isAnteaterMovePatternLegal(const GameState *state, Move move, ChessPiece ant)
{
    ChessPiece dest;
    int dr;
    int dc;
    Position captured[MAX_CAPTURED_PAWNS];
    int count;
    int ar;
    int ac;
    int i;

    dest = getPieceAt(state, move.dest);
    dr = absInt(move.dest.row - move.start.row);
    dc = absInt(move.dest.col - move.start.col);

    if (dest.type == EMPTY)
    {
        if (dr <= 1)
        {
            if (dc <= 1)
            {
                if (dr != 0 || dc != 0)
                {
                    return 1;
                }
            }
        }

        return 0;
    }

    if (dest.type != PAWN)
    {
        return 0;
    }

    if (dest.color == ant.color)
    {
        return 0;
    }

    count = 0;

    for (ar = -1; ar <= 1; ar++)
    {
        for (ac = -1; ac <= 1; ac++)
        {
            Position adjacent;
            ChessPiece first;

            if (ar == 0)
            {
                if (ac == 0)
                {
                    continue;
                }
            }

            adjacent.row = move.start.row + ar;
            adjacent.col = move.start.col + ac;

            if (!isPositionInBounds(adjacent))
            {
                continue;
            }

            first = getPieceAt(state, adjacent);

            if (first.type == PAWN)
            {
                if (first.color != ant.color)
                {
                    count = collectAnteaterCapturePawns(state,
                                                        move.start,
                                                        adjacent,
                                                        captured,
                                                        MAX_CAPTURED_PAWNS);

                    for (i = 0; i < count; i++)
                    {
                        if (positionsEqual(captured[i], move.dest))
                        {
                            return 1;
                        }
                    }
                }
            }
        }
    }

    return 0;
}

//precondition: Take in the game state and move
//postcondition: Return integer if the move is legal
int isMoveLegal(const GameState *state, Move move)
{
    ChessPiece piece;
    ChessPiece dest;
    int patternLegal;
    GameState copy;

    if (state == NULL)
    {
        return 0;
    }

    if (!isPositionInBounds(move.start))
    {
        return 0;
    }

    if (!isPositionInBounds(move.dest))
    {
        return 0;
    }

    if (positionsEqual(move.start, move.dest))
    {
        return 0;
    }

    piece = getPieceAt(state, move.start);
    dest = getPieceAt(state, move.dest);

    if (piece.type == EMPTY)
    {
        return 0;
    }

    if (piece.color != state->currentTurn)
    {
        return 0;
    }

    if (dest.type != EMPTY)
    {
        if (dest.color == piece.color)
        {
            return 0;
        }
    }

    /*
     * Do not allow physical king captures.
     * A game should end by checkmate or stalemate, not by deleting a king.
     */
    if (dest.type == KING)
    {
        return 0;
    }

    patternLegal = 0;

    if (piece.type == PAWN)
    {
        patternLegal = isPawnMovePatternLegal(state, move, piece);
    }
    else if (piece.type == ROOK)
    {
        patternLegal = isRookMovePatternLegal(state, move);
    }
    else if (piece.type == BISHOP)
    {
        patternLegal = isBishopMovePatternLegal(state, move);
    }
    else if (piece.type == KNIGHT)
    {
        patternLegal = isKnightMovePatternLegal(move);
    }
    else if (piece.type == QUEEN)
    {
        patternLegal = isQueenMovePatternLegal(state, move);
    }
    else if (piece.type == KING)
    {
        patternLegal = isKingMovePatternLegal(state, move, piece);
    }
    else if (piece.type == ANTEATER)
    {
        patternLegal = isAnteaterMovePatternLegal(state, move, piece);
    }
    else
    {
        patternLegal = 0;
    }

    if (!patternLegal)
    {
        return 0;
    }

    copyGameState(&copy, state);
    applyMove(&copy, move);

    if (isKingInCheck(&copy, piece.color))
    {
        return 0;
    }

    return 1;
}

//precondition: Take in the game state, move, and side color
//postcondition: Return integer if the move does not leave the king in check
static int isMoveLegalFast(GameState *state, Move move, PieceColor side)
{
    MoveRecord record;
    int legal;

    applyMoveRecord(state, move, &record);
    legal = !isKingInCheck(state, side);
    undoMove(state, &record);

    return legal;
}

//precondition: Take in the game state, side color, and move list
//postcondition: Fill the move list with all legal moves for the side
void generateAllLegalMoves(const GameState *state, PieceColor side, MoveList *list)
{
    GameState temp;
    MoveList pseudo;
    int i;

    clearMoveList(list);

    if (state == NULL)
    {
        return;
    }

    if (side != WHITE)
    {
        if (side != BLACK)
        {
            return;
        }
    }

    copyGameState(&temp, state);
    temp.currentTurn = side;

    generateAllPseudoLegalMoves(&temp, side, &pseudo);

    for (i = 0; i < pseudo.count; i++)
    {
        ChessPiece moving = getPieceAt(&temp, pseudo.moves[i].start);
        int isCastleAttempt = moving.type == KING &&
                              pseudo.moves[i].start.row == pseudo.moves[i].dest.row &&
                              (pseudo.moves[i].dest.col - pseudo.moves[i].start.col == 2 ||
                               pseudo.moves[i].dest.col - pseudo.moves[i].start.col == -2);

        if ((isCastleAttempt && isMoveLegal(&temp, pseudo.moves[i])) ||
            (!isCastleAttempt && isMoveLegalFast(&temp, pseudo.moves[i], side)))
        {
            addMove(list,
                    pseudo.moves[i].start,
                    pseudo.moves[i].dest,
                    pseudo.moves[i].promotionGoal);
        }
    }
}

//precondition: Take in the game state and side color
//postcondition: Return integer if the side is in checkmate
int isCheckmate(const GameState *state, PieceColor side)
{
    MoveList moves;

    if (!isKingInCheck(state, side))
    {
        return 0;
    }

    generateAllLegalMoves(state, side, &moves);

    if (moves.count == 0)
    {
        return 1;
    }

    return 0;
}

//precondition: Take in the game state and side color
//postcondition: Return integer if the side is in stalemate
int isStalemate(const GameState *state, PieceColor side)
{
    MoveList moves;

    if (isKingInCheck(state, side))
    {
        return 0;
    }

    generateAllLegalMoves(state, side, &moves);

    if (moves.count == 0)
    {
        return 1;
    }

    return 0;
}

//precondition: Take in the game state and side color
//postcondition: Return integer if the king is gone
int isKingDead(const GameState *state, PieceColor side)
{
    Position p;
    ChessPiece k;

    p = getKingPosition(state, side);

    if (!isPositionInBounds(p))
    {
        return 1;
    }

    k = getPieceAt(state, p);

    if (k.type != KING)
    {
        return 1;
    }

    if (k.color != side)
    {
        return 1;
    }

    return 0;
}

//precondition: Take in the game state
//postcondition: Return the game result message
const char *getResultMessage(const GameState *state)
{
    if (isKingDead(state, WHITE))
    {
        return "Black wins. White king is gone.";
    }

    if (isKingDead(state, BLACK))
    {
        return "White wins. Black king is gone.";
    }

    if (isCheckmate(state, WHITE))
    {
        return "Checkmate. Black wins.";
    }

    if (isCheckmate(state, BLACK))
    {
        return "Checkmate. White wins.";
    }

    if (isStalemate(state, state->currentTurn))
    {
        return "Stalemate. Draw.";
    }

    return NULL;
}
