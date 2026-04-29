#include <stdlib.h>
#include <limits.h>

#include "cpu.h"
#include "game.h"
#include "board.h"
#include "eval.h"
#include "types.h"

#define CPU_SEARCH_DEPTH 3
#define CPU_MATE_SCORE 10000000

//precondition: None
//postcondition: Return an invalid move
static Move makeInvalidMove(void)
{
    Move move;

    move.start.row = -1;
    move.start.col = -1;
    move.dest.row = -1;
    move.dest.col = -1;
    move.promotionGoal = EMPTY;

    return move;
}

//precondition: Take in the side color
//postcondition: Return the opposite side color
static PieceColor oppositeColorLocal(PieceColor side)
{
    if (side == WHITE) return BLACK;
    if (side == BLACK) return WHITE;
    return NONE;
}

//precondition: Take in a move
//postcondition: Return integer if the move is valid
static int moveIsValid(Move move)
{
    return move.start.row >= 0 &&
           move.start.col >= 0 &&
           move.dest.row >= 0 &&
           move.dest.col >= 0;
}

//precondition: Take in the piece type
//postcondition: Return the value of that piece type
int getPieceValue(PieceType type)
{
    switch (type)
    {
        case PAWN:     return 100;
        case KNIGHT:   return 320;
        case BISHOP:   return 330;
        case ROOK:     return 500;
        case QUEEN:    return 900;
        case KING:     return 20000;
        case ANTEATER: return 700;
        case EMPTY:
        default:       return 0;
    }
}

//precondition: Take in the game state and CPU side color
//postcondition: Return a random legal move for the CPU side
Move chooseRandomMove(const GameState *state, PieceColor cpuSide)
{
    MoveList legalMoves;
    int randomIndex;

    generateAllLegalMoves(state, cpuSide, &legalMoves);

    if (legalMoves.count == 0)
    {
        return makeInvalidMove();
    }

    randomIndex = rand() % legalMoves.count;
    return legalMoves.moves[randomIndex];
}

//precondition: Take in the game state and CPU side color
//postcondition: Return a greedy legal move for the CPU side
Move chooseGreedyMove(const GameState *state, PieceColor cpuSide)
{
    MoveList legalMoves;
    Move bestMove;
    int bestScore;
    int i;

    generateAllLegalMoves(state, cpuSide, &legalMoves);

    if (legalMoves.count == 0)
    {
        return makeInvalidMove();
    }

    bestMove = legalMoves.moves[0];
    bestScore = INT_MIN;

    for (i = 0; i < legalMoves.count; i++)
    {
        Move currentMove = legalMoves.moves[i];
        ChessPiece targetPiece = state->squares[currentMove.dest.row][currentMove.dest.col];
        int score = 0;

        if (targetPiece.type != EMPTY && targetPiece.color == oppositeColorLocal(cpuSide))
        {
            score += getPieceValue(targetPiece.type);
        }

        if (currentMove.promotionGoal != EMPTY)
        {
            score += getPieceValue(currentMove.promotionGoal);
        }

        if (score > bestScore)
        {
            bestScore = score;
            bestMove = currentMove;
        }
    }

    return bestMove;
}

//precondition: Take in the game state, search depth, alpha, beta, side to move, and maximizing side
//postcondition: Return the alpha-beta evaluation score
static int alphaBeta(GameState *state,
                     int depth,
                     int alpha,
                     int beta,
                     PieceColor sideToMove,
                     PieceColor maximizingSide)
{
    MoveList legalMoves;
    int i;

    if (depth <= 0)
    {
        return evaluatePosition(state, maximizingSide);
    }

    generateAllLegalMoves(state, sideToMove, &legalMoves);

    if (legalMoves.count == 0)
    {
        if (isKingInCheck(state, sideToMove))
        {
            if (sideToMove == maximizingSide)
            {
                return -CPU_MATE_SCORE - depth;
            }
            else
            {
                return CPU_MATE_SCORE + depth;
            }
        }

        return 0;
    }

    if (sideToMove == maximizingSide)
    {
        int bestScore = INT_MIN;

        for (i = 0; i < legalMoves.count; i++)
        {
            GameState copy = *state;
            Move move = legalMoves.moves[i];
            int score;

            if (!isMoveLegal(&copy, move))
            {
                continue;
            }

            applyMove(&copy, move);

            score = alphaBeta(&copy,
                              depth - 1,
                              alpha,
                              beta,
                              oppositeColorLocal(sideToMove),
                              maximizingSide);

            if (score > bestScore)
            {
                bestScore = score;
            }

            if (score > alpha)
            {
                alpha = score;
            }

            if (beta <= alpha)
            {
                break;
            }
        }

        return bestScore;
    }
    else
    {
        int bestScore = INT_MAX;

        for (i = 0; i < legalMoves.count; i++)
        {
            GameState copy = *state;
            Move move = legalMoves.moves[i];
            int score;

            if (!isMoveLegal(&copy, move))
            {
                continue;
            }

            applyMove(&copy, move);

            score = alphaBeta(&copy,
                              depth - 1,
                              alpha,
                              beta,
                              oppositeColorLocal(sideToMove),
                              maximizingSide);

            if (score < bestScore)
            {
                bestScore = score;
            }

            if (score < beta)
            {
                beta = score;
            }

            if (beta <= alpha)
            {
                break;
            }
        }

        return bestScore;
    }
}

//precondition: Take in the game state and CPU side color
//postcondition: Return the best move found by alpha-beta search
Move chooseBestMove(const GameState *state, PieceColor cpuSide)
{
    MoveList legalMoves;
    Move bestMove;
    int bestScore;
    int i;

    generateAllLegalMoves(state, cpuSide, &legalMoves);

    if (legalMoves.count == 0)
    {
        return makeInvalidMove();
    }

    bestMove = legalMoves.moves[0];
    bestScore = INT_MIN;

    for (i = 0; i < legalMoves.count; i++)
    {
        GameState copy = *state;
        Move move = legalMoves.moves[i];
        int score;

        if (!isMoveLegal(&copy, move))
        {
            continue;
        }

        applyMove(&copy, move);

        score = alphaBeta(&copy,
                          CPU_SEARCH_DEPTH - 1,
                          INT_MIN + 1,
                          INT_MAX,
                          oppositeColorLocal(cpuSide),
                          cpuSide);

        if (score > bestScore)
        {
            bestScore = score;
            bestMove = move;
        }
        else if (score == bestScore && (rand() % 4) == 0)
        {
            bestMove = move;
        }
    }

    if (!moveIsValid(bestMove))
    {
        return makeInvalidMove();
    }

    return bestMove;
}