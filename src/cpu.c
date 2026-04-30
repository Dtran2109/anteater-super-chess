#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
#include <time.h>
#include <string.h>

#include "cpu.h"
#include "game.h"
#include "board.h"
#include "eval.h"
#include "types.h"

#define CPU_SEARCH_DEPTH 10
#define CPU_QUIESCENCE_DEPTH 1
#define CPU_MATE_SCORE 10000000
#define CPU_INF 1000000000
#define CPU_DEFAULT_TT_ENTRIES (1u << 19)
#define CPU_MIN_TT_ENTRIES (1u << 10)
#define CPU_DEEP_SEARCH_TIME_MS 3000
#define CPU_STOP_CHECK_INTERVAL 2048u
#define CPU_MAX_SEARCH_PLY 128
#define CPU_ASPIRATION_WINDOW 75
#define CPU_HISTORY_MAX 1000000
#define CPU_LMR_MIN_DEPTH 2
#define CPU_LMR_MOVE_INDEX 1
#define CPU_LMR_DEEP_MIN_DEPTH 4
#define CPU_LMR_DEEP_MOVE_INDEX 4
#define CPU_NULL_MOVE_MIN_DEPTH 3
#define CPU_NULL_MOVE_REDUCTION 4
#define CPU_FUTILITY_MAX_DEPTH 1
#define CPU_FUTILITY_MARGIN 180
#define CPU_ROOT_LIMIT_DEPTH 6
#define CPU_ROOT_MOVE_LIMIT 2

typedef struct
{
    const char *name;
    int quiescenceDepth;
    int lmrMinDepth;
    int lmrMoveIndex;
    int lmrDeepMinDepth;
    int lmrDeepMoveIndex;
    int researchReducedMoves;
    int nullMoveMinDepth;
    int nullMoveReduction;
    int futilityMaxDepth;
    int futilityMargin;
    int rootLimitDepth;
    int rootMoveLimit;
} CpuSearchPreset;

enum
{
    TT_EXACT = 0,
    TT_LOWER = 1,
    TT_UPPER = 2
};

typedef struct
{
    uint64_t key;
    Move bestMove;
    int score;
    int depth;
    unsigned char flag;
    unsigned char valid;
    unsigned char generation;
} TranspositionEntry;

static const CpuSearchPreset CPU_PRESET_FAST = {
    "fast",
    CPU_QUIESCENCE_DEPTH,
    CPU_LMR_MIN_DEPTH,
    CPU_LMR_MOVE_INDEX,
    CPU_LMR_DEEP_MIN_DEPTH,
    CPU_LMR_DEEP_MOVE_INDEX,
    0,
    CPU_NULL_MOVE_MIN_DEPTH,
    CPU_NULL_MOVE_REDUCTION,
    CPU_FUTILITY_MAX_DEPTH,
    CPU_FUTILITY_MARGIN,
    CPU_ROOT_LIMIT_DEPTH,
    CPU_ROOT_MOVE_LIMIT
};

static const CpuSearchPreset CPU_PRESET_BALANCED = {
    "balanced",
    2,
    3,
    3,
    5,
    8,
    0,
    3,
    3,
    1,
    180,
    6,
    8
};

static const CpuSearchPreset CPU_PRESET_STRONG = {
    "strong",
    4,
    3,
    4,
    5,
    10,
    0,
    4,
    2,
    0,
    0,
    99,
    MAX_MOVES
};

static const CpuSearchPreset CPU_PRESET_SHARP = {
    "sharp",
    3,
    3,
    4,
    5,
    10,
    0,
    4,
    2,
    0,
    0,
    99,
    MAX_MOVES
};

static const CpuSearchPreset CPU_PRESET_STORM = {
    "storm",
    4,
    3,
    4,
    5,
    10,
    1,
    4,
    2,
    0,
    0,
    99,
    MAX_MOVES
};

static TranspositionEntry *transpositionTable = NULL;
static size_t transpositionTableSize = 0u;
static size_t transpositionTableMask = 0u;
static int transpositionTableUsesMask = 0;
static const CpuSearchPreset *activePreset = &CPU_PRESET_FAST;
static unsigned char currentTtGeneration = 1;
static clock_t searchStartClock;
static int searchTimeLimitMs;
static int searchUseTimeLimit;
static int searchStopped;
static int cpuSearchTimeLimitMs = CPU_DEEP_SEARCH_TIME_MS;
static unsigned long searchNodeCount;
static unsigned long searchTtHits;
static unsigned long searchTtCutoffs;
static unsigned long long totalSearchNodeCount;
static unsigned long long totalSearchTtHits;
static unsigned long long totalSearchTtCutoffs;
static unsigned long long totalSearchCount;
static double totalSearchTimeMs;
static CpuSearchStats lastSearchStats;
static Move killerMoves[CPU_MAX_SEARCH_PLY][2];
static int historyScores[2][BOARD_ROWS * BOARD_COLS][BOARD_ROWS * BOARD_COLS];

static int allocateTranspositionTable(size_t entryCount)
{
    TranspositionEntry *newTable;

    if (entryCount < CPU_MIN_TT_ENTRIES)
    {
        entryCount = CPU_MIN_TT_ENTRIES;
    }

    if (entryCount == transpositionTableSize && transpositionTable != NULL)
    {
        memset(transpositionTable, 0, transpositionTableSize * sizeof(*transpositionTable));
        return 1;
    }

    newTable = (TranspositionEntry *)calloc(entryCount, sizeof(*newTable));
    if (newTable == NULL)
    {
        return 0;
    }

    free(transpositionTable);
    transpositionTable = newTable;
    transpositionTableSize = entryCount;
    transpositionTableMask = entryCount - 1u;
    transpositionTableUsesMask = (entryCount & (entryCount - 1u)) == 0u;
    currentTtGeneration = 1;

    return 1;
}

static int ensureTranspositionTable(void)
{
    if (transpositionTable != NULL && transpositionTableSize > 0u)
    {
        return 1;
    }

    return allocateTranspositionTable(CPU_DEFAULT_TT_ENTRIES);
}

static size_t transpositionIndex(uint64_t key)
{
    if (transpositionTableUsesMask)
    {
        return (size_t)key & transpositionTableMask;
    }

    return (size_t)(key % (uint64_t)transpositionTableSize);
}

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

//precondition: Take in two moves
//postcondition: Return integer if both moves represent the same move
static int movesAreEqual(Move a, Move b)
{
    return a.start.row == b.start.row &&
           a.start.col == b.start.col &&
           a.dest.row == b.dest.row &&
           a.dest.col == b.dest.col &&
           a.promotionGoal == b.promotionGoal;
}

//precondition: Take in a side color
//postcondition: Return the history-table side index
static int sideToHistoryIndex(PieceColor side)
{
    if (side == BLACK)
    {
        return 1;
    }

    return 0;
}

//precondition: Take in a board position
//postcondition: Return a compact square index
static int squareIndex(Position pos)
{
    return pos.row * BOARD_COLS + pos.col;
}

//precondition: Take in search ply and move
//postcondition: Remember a quiet cutoff move for future move ordering
static void rememberKillerMove(int ply, Move move)
{
    if (ply < 0 || ply >= CPU_MAX_SEARCH_PLY)
    {
        return;
    }

    if (movesAreEqual(killerMoves[ply][0], move))
    {
        return;
    }

    killerMoves[ply][1] = killerMoves[ply][0];
    killerMoves[ply][0] = move;
}

//precondition: None
//postcondition: Clear search-local killer move ordering hints
static void clearKillerMoves(void)
{
    Move invalid = makeInvalidMove();
    int ply;

    for (ply = 0; ply < CPU_MAX_SEARCH_PLY; ply++)
    {
        killerMoves[ply][0] = invalid;
        killerMoves[ply][1] = invalid;
    }
}

//precondition: Take in search side, move, and depth
//postcondition: Increase quiet move priority for future move ordering
static void addHistoryScore(PieceColor side, Move move, int depth)
{
    int sideIndex;
    int from;
    int to;
    int bonus;

    if (!moveIsValid(move))
    {
        return;
    }

    sideIndex = sideToHistoryIndex(side);
    from = squareIndex(move.start);
    to = squareIndex(move.dest);
    bonus = depth * depth;

    historyScores[sideIndex][from][to] += bonus;

    if (historyScores[sideIndex][from][to] > CPU_HISTORY_MAX)
    {
        historyScores[sideIndex][from][to] /= 2;
    }
}

//precondition: Take in the game state and side color
//postcondition: Return integer if the side has material where null-move pruning is safer
static int sideHasNonPawnMaterial(const GameState *state, PieceColor side)
{
    int row;
    int col;

    for (row = 0; row < BOARD_ROWS; row++)
    {
        for (col = 0; col < BOARD_COLS; col++)
        {
            ChessPiece piece = state->squares[row][col];

            if (piece.color == side &&
                piece.type != EMPTY &&
                piece.type != KING &&
                piece.type != PAWN)
            {
                return 1;
            }
        }
    }

    return 0;
}

//precondition: Take in a search ply
//postcondition: Return a mate score adjusted to prefer faster wins and slower losses
static int matedScore(int ply)
{
    if (ply < 0)
    {
        ply = 0;
    }

    return -CPU_MATE_SCORE + ply;
}

//precondition: None
//postcondition: Return integer if the current search should stop
static int searchShouldStop(void)
{
    searchNodeCount++;

    if (!searchUseTimeLimit || searchStopped)
    {
        return searchStopped;
    }

    if ((searchNodeCount & (CPU_STOP_CHECK_INTERVAL - 1u)) == 0u)
    {
        clock_t now = clock();
        long elapsedMs = (long)((now - searchStartClock) * 1000 / CLOCKS_PER_SEC);

        if (elapsedMs >= searchTimeLimitMs)
        {
            searchStopped = 1;
        }
    }

    return searchStopped;
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

//precondition: Take in a value
//postcondition: Return a mixed 64-bit value for position hashing
static uint64_t mix64(uint64_t x)
{
    x += UINT64_C(0x9e3779b97f4a7c15);
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    return x ^ (x >> 31);
}

//precondition: Take in the game state and side to move
//postcondition: Return a hash key for the searchable game state
static uint64_t hashPosition(const GameState *state, PieceColor sideToMove)
{
    uint64_t key = UINT64_C(0xcbf29ce484222325);
    int row;
    int col;

    for (row = 0; row < BOARD_ROWS; row++)
    {
        for (col = 0; col < BOARD_COLS; col++)
        {
            ChessPiece piece = state->squares[row][col];

            if (piece.type != EMPTY)
            {
                uint64_t pieceKey = (uint64_t)(row * BOARD_COLS + col + 1);
                pieceKey ^= (uint64_t)piece.type << 12;
                pieceKey ^= (uint64_t)piece.color << 20;
                pieceKey ^= (uint64_t)(piece.MoveCount & 7) << 28;
                key ^= mix64(pieceKey);
            }
        }
    }

    key ^= mix64((uint64_t)sideToMove << 1);
    key ^= mix64((uint64_t)(state->whiteCastleKingSide != 0) << 3);
    key ^= mix64((uint64_t)(state->whiteCastleQueenSide != 0) << 4);
    key ^= mix64((uint64_t)(state->blackCastleKingSide != 0) << 5);
    key ^= mix64((uint64_t)(state->blackCastleQueenSide != 0) << 6);

    if (state->enPassantTarget.row >= 0 && state->enPassantTarget.col >= 0)
    {
        key ^= mix64((uint64_t)(state->enPassantTarget.row * BOARD_COLS +
                                state->enPassantTarget.col + 1) << 8);
    }

    return key;
}

//precondition: Take in a move and board state
//postcondition: Return integer if the move captures material or promotes a pawn
static int moveIsNoisy(const GameState *state, Move move, PieceColor side)
{
    ChessPiece target;
    ChessPiece moving;

    if (move.promotionGoal != EMPTY)
    {
        return 1;
    }

    target = state->squares[move.dest.row][move.dest.col];
    if (target.type != EMPTY && target.color == oppositeColorLocal(side))
    {
        return 1;
    }

    moving = state->squares[move.start.row][move.start.col];
    if (moving.type == PAWN &&
        target.type == EMPTY &&
        move.start.col != move.dest.col &&
        state->enPassantTarget.row == move.dest.row &&
        state->enPassantTarget.col == move.dest.col)
    {
        return 1;
    }

    return 0;
}

//precondition: Take in the game state, move, side, and preferred hash move
//postcondition: Return a higher score for moves that should be searched first
static int scoreMoveForOrdering(const GameState *state,
                                Move move,
                                PieceColor side,
                                Move hashMove,
                                int ply)
{
    ChessPiece moving;
    ChessPiece target;
    int score = 0;
    int centerDistance;
    int noisyMove;

    if (moveIsValid(hashMove) && movesAreEqual(move, hashMove))
    {
        return 200000000;
    }

    moving = state->squares[move.start.row][move.start.col];
    target = state->squares[move.dest.row][move.dest.col];
    noisyMove = moveIsNoisy(state, move, side);

    if (target.type != EMPTY && target.color == oppositeColorLocal(side))
    {
        score += 100000 + getPieceValue(target.type) * 10;
        score -= getPieceValue(moving.type);
    }

    if (moving.type == PAWN &&
        target.type == EMPTY &&
        move.start.col != move.dest.col &&
        state->enPassantTarget.row == move.dest.row &&
        state->enPassantTarget.col == move.dest.col)
    {
        score += 100000 + getPieceValue(PAWN) * 10;
    }

    if (move.promotionGoal != EMPTY)
    {
        score += 80000 + getPieceValue(move.promotionGoal);
    }

    if (!noisyMove && ply >= 0 && ply < CPU_MAX_SEARCH_PLY)
    {
        if (movesAreEqual(killerMoves[ply][0], move))
        {
            score += 70000;
        }
        else if (movesAreEqual(killerMoves[ply][1], move))
        {
            score += 69000;
        }
    }

    if (!noisyMove)
    {
        score += historyScores[sideToHistoryIndex(side)]
                              [squareIndex(move.start)]
                              [squareIndex(move.dest)] / 16;
    }

    centerDistance = abs(move.dest.row - 3) + abs(move.dest.row - 4);
    centerDistance += abs(move.dest.col - 4) + abs(move.dest.col - 5);
    score += 20 - centerDistance;

    return score;
}

//precondition: Take in a move list and board state
//postcondition: Sort the moves so alpha-beta finds strong moves early
static void orderMoves(const GameState *state,
                       MoveList *list,
                       PieceColor side,
                       Move hashMove,
                       int ply)
{
    int scores[MAX_MOVES];
    int i;

    for (i = 0; i < list->count; i++)
    {
        scores[i] = scoreMoveForOrdering(state,
                                         list->moves[i],
                                         side,
                                         hashMove,
                                         ply);
    }

    for (i = 1; i < list->count; i++)
    {
        Move move = list->moves[i];
        int score = scores[i];
        int j = i - 1;

        while (j >= 0 && scores[j] < score)
        {
            list->moves[j + 1] = list->moves[j];
            scores[j + 1] = scores[j];
            j--;
        }

        list->moves[j + 1] = move;
        scores[j + 1] = score;
    }
}

//precondition: Take in a position key, depth, alpha, beta, and output pointers
//postcondition: Return integer if the transposition table produced a usable score
static int probeTransposition(uint64_t key,
                              int depth,
                              int *alpha,
                              int beta,
                              int *score,
                              Move *bestMove)
{
    TranspositionEntry *entry;

    if (!ensureTranspositionTable())
    {
        return 0;
    }

    entry = &transpositionTable[transpositionIndex(key)];

    if (!entry->valid || entry->key != key)
    {
        return 0;
    }

    searchTtHits++;

    if (bestMove != NULL)
    {
        *bestMove = entry->bestMove;
    }

    entry->generation = currentTtGeneration;

    if (entry->depth < depth)
    {
        return 0;
    }

    if (entry->flag == TT_EXACT)
    {
        *score = entry->score;
        searchTtCutoffs++;
        return 1;
    }

    if (entry->flag == TT_LOWER && entry->score > *alpha)
    {
        *alpha = entry->score;
    }
    else if (entry->flag == TT_UPPER && entry->score <= *alpha)
    {
        *score = entry->score;
        searchTtCutoffs++;
        return 1;
    }

    if (*alpha >= beta)
    {
        *score = entry->score;
        searchTtCutoffs++;
        return 1;
    }

    return 0;
}

//precondition: Take in a position key and search result
//postcondition: Store the search result in the transposition table
static void storeTransposition(uint64_t key,
                               int depth,
                               int score,
                               int flag,
                               Move bestMove)
{
    TranspositionEntry *entry;

    if (!ensureTranspositionTable())
    {
        return;
    }

    entry = &transpositionTable[transpositionIndex(key)];

    if (entry->valid &&
        entry->key != key &&
        entry->generation == currentTtGeneration &&
        entry->depth > depth + 2)
    {
        return;
    }

    entry->valid = 1;
    entry->key = key;
    entry->depth = depth;
    entry->score = score;
    entry->flag = (unsigned char)flag;
    entry->bestMove = bestMove;
    entry->generation = currentTtGeneration;
}

static int quiescenceSearch(GameState *state,
                            int alpha,
                            int beta,
                            PieceColor sideToMove,
                            int quietDepth);

static int alphaBeta(GameState *state,
                     int depth,
                     int alpha,
                     int beta,
                     PieceColor sideToMove,
                     int ply);

//precondition: Take in the game state, alpha, beta, side to move, and quiet depth
//postcondition: Return a capture-stable evaluation score
static int quiescenceSearch(GameState *state,
                            int alpha,
                            int beta,
                            PieceColor sideToMove,
                            int quietDepth)
{
    MoveList legalMoves;
    int inCheck;
    int i;
    int searchedMove;

    if (searchShouldStop())
    {
        return evaluatePosition(state, sideToMove);
    }

    state->currentTurn = sideToMove;
    inCheck = isKingInCheck(state, sideToMove);

    if (!inCheck)
    {
        int standPat = evaluatePosition(state, sideToMove);

        if (standPat >= beta)
        {
            return beta;
        }

        if (standPat > alpha)
        {
            alpha = standPat;
        }

        if (quietDepth <= 0)
        {
            return alpha;
        }
    }
    else if (quietDepth <= 0)
    {
        return evaluatePosition(state, sideToMove);
    }

    generateAllLegalMoves(state, sideToMove, &legalMoves);
    orderMoves(state, &legalMoves, sideToMove, makeInvalidMove(), -1);

    if (legalMoves.count == 0)
    {
        if (inCheck)
        {
            return matedScore(0);
        }

        return 0;
    }

    searchedMove = 0;
    for (i = 0; i < legalMoves.count; i++)
    {
        Move move = legalMoves.moves[i];
        MoveRecord record;
        int score;

        if (!inCheck && !moveIsNoisy(state, move, sideToMove))
        {
            continue;
        }

        searchedMove = 1;
        applyMoveRecord(state, move, &record);
        score = -quiescenceSearch(state,
                                  -beta,
                                  -alpha,
                                  oppositeColorLocal(sideToMove),
                                  quietDepth - 1);
        undoMove(state, &record);

        if (searchStopped)
        {
            return alpha;
        }

        if (score >= beta)
        {
            return beta;
        }

        if (score > alpha)
        {
            alpha = score;
        }
    }

    if (inCheck && !searchedMove)
    {
        return matedScore(0);
    }

    return alpha;
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

//precondition: Take in the game state, search depth, alpha, beta, and side to move
//postcondition: Return the negamax alpha-beta evaluation score
static int alphaBeta(GameState *state,
                     int depth,
                     int alpha,
                     int beta,
                     PieceColor sideToMove,
                     int ply)
{
    MoveList legalMoves;
    Move bestMove;
    Move hashMove;
    uint64_t key;
    int originalAlpha;
    int originalBeta;
    int bestScore;
    int ttScore;
    int inCheck;
    int staticScore;
    int i;

    if (searchShouldStop())
    {
        return evaluatePosition(state, sideToMove);
    }

    if (depth <= 0)
    {
        return quiescenceSearch(state,
                                alpha,
                                beta,
                                sideToMove,
                                activePreset->quiescenceDepth);
    }

    if (state->halfmoveClock >= 100)
    {
        return 0;
    }

    state->currentTurn = sideToMove;
    key = hashPosition(state, sideToMove);
    originalAlpha = alpha;
    originalBeta = beta;
    hashMove = makeInvalidMove();

    if (probeTransposition(key, depth, &alpha, beta, &ttScore, &hashMove))
    {
        return ttScore;
    }

    inCheck = isKingInCheck(state, sideToMove);
    staticScore = 0;

    if (!inCheck && depth <= activePreset->futilityMaxDepth)
    {
        staticScore = evaluatePosition(state, sideToMove);
    }

    if (!inCheck &&
        depth >= activePreset->nullMoveMinDepth &&
        ply > 0 &&
        sideHasNonPawnMaterial(state, sideToMove))
    {
        Position savedEnPassantTarget = state->enPassantTarget;
        PieceColor savedTurn = state->currentTurn;
        int nullDepth = depth - 1 - activePreset->nullMoveReduction;
        int nullScore;

        if (nullDepth < 0)
        {
            nullDepth = 0;
        }

        state->enPassantTarget = (Position){-1, -1};
        state->currentTurn = oppositeColorLocal(sideToMove);

        nullScore = -alphaBeta(state,
                               nullDepth,
                               -beta,
                               -beta + 1,
                               oppositeColorLocal(sideToMove),
                               ply + 1);

        state->enPassantTarget = savedEnPassantTarget;
        state->currentTurn = savedTurn;

        if (searchStopped)
        {
            return beta;
        }

        if (nullScore >= beta)
        {
            return beta;
        }
    }

    generateAllLegalMoves(state, sideToMove, &legalMoves);
    orderMoves(state, &legalMoves, sideToMove, hashMove, ply);

    if (legalMoves.count == 0)
    {
        if (inCheck)
        {
            return matedScore(ply);
        }

        return 0;
    }

    bestScore = -CPU_INF;
    bestMove = legalMoves.moves[0];

    for (i = 0; i < legalMoves.count; i++)
    {
        Move move = legalMoves.moves[i];
        MoveRecord record;
        int quietMove = !moveIsNoisy(state, move, sideToMove);
        int reduceMove = quietMove &&
                         !inCheck &&
                         depth >= activePreset->lmrMinDepth &&
                         i >= activePreset->lmrMoveIndex;
        int reducedDepth = depth - 2;
        int score;

        if (reduceMove &&
            depth >= activePreset->lmrDeepMinDepth &&
            i >= activePreset->lmrDeepMoveIndex)
        {
            reducedDepth = depth - 3;
        }

        if (reducedDepth < 0)
        {
            reducedDepth = 0;
        }

        if (quietMove &&
            !inCheck &&
            depth <= activePreset->futilityMaxDepth &&
            i > 0 &&
            staticScore + activePreset->futilityMargin * depth <= alpha)
        {
            continue;
        }

        applyMoveRecord(state, move, &record);

        if (reduceMove)
        {
            score = -alphaBeta(state,
                               reducedDepth,
                               -alpha - 1,
                               -alpha,
                               oppositeColorLocal(sideToMove),
                               ply + 1);

            if (activePreset->researchReducedMoves &&
                !searchStopped &&
                score > alpha)
            {
                score = -alphaBeta(state,
                                   depth - 1,
                                   -beta,
                                   -alpha,
                                   oppositeColorLocal(sideToMove),
                                   ply + 1);
            }
        }
        else
        {
            score = alpha - 1;
        }

        if (!reduceMove && i > 0)
        {
            score = -alphaBeta(state,
                               depth - 1,
                               -alpha - 1,
                               -alpha,
                               oppositeColorLocal(sideToMove),
                               ply + 1);
        }

        if (!reduceMove &&
            (i == 0 || (!searchStopped && score > alpha && score < beta)))
        {
            score = -alphaBeta(state,
                               depth - 1,
                               -beta,
                               -alpha,
                               oppositeColorLocal(sideToMove),
                               ply + 1);
        }

        undoMove(state, &record);

        if (searchStopped)
        {
            return alpha;
        }

        if (score > bestScore)
        {
            bestScore = score;
            bestMove = move;
        }

        if (score > alpha)
        {
            alpha = score;
        }

        if (alpha >= beta)
        {
            if (quietMove)
            {
                rememberKillerMove(ply, move);
                addHistoryScore(sideToMove, move, depth);
            }

            break;
        }
    }

    if (searchStopped)
    {
        return bestScore;
    }

    if (bestScore <= originalAlpha)
    {
        storeTransposition(key, depth, bestScore, TT_UPPER, bestMove);
    }
    else if (bestScore >= originalBeta)
    {
        storeTransposition(key, depth, bestScore, TT_LOWER, bestMove);
    }
    else
    {
        storeTransposition(key, depth, bestScore, TT_EXACT, bestMove);
    }

    return bestScore;
}

//precondition: Take in the game state, CPU side color, and search depth
//postcondition: Return the best move found by iterative deepening alpha-beta search
Move chooseBestMoveDepth(const GameState *state, PieceColor cpuSide, int depth)
{
    MoveList legalMoves;
    GameState root;
    Move bestMove;
    Move previousBest;
    int completedDepth;
    int rootBestScore;
    int currentDepth;
    int i;

    if (depth < 1)
    {
        depth = 1;
    }

    generateAllLegalMoves(state, cpuSide, &legalMoves);

    if (legalMoves.count == 0)
    {
        return makeInvalidMove();
    }

    root = *state;
    root.currentTurn = cpuSide;
    bestMove = legalMoves.moves[0];
    previousBest = makeInvalidMove();
    completedDepth = 0;
    rootBestScore = evaluatePosition(&root, cpuSide);
    searchStartClock = clock();
    searchTimeLimitMs = cpuSearchTimeLimitMs;
    searchUseTimeLimit = (depth >= 8);
    searchStopped = 0;
    searchNodeCount = 0u;
    searchTtHits = 0u;
    searchTtCutoffs = 0u;
    clearKillerMoves();
    currentTtGeneration++;
    if (currentTtGeneration == 0)
    {
        currentTtGeneration = 1;
    }

    for (currentDepth = 1; currentDepth <= depth; currentDepth++)
    {
        int alpha;
        int beta;
        int bestScore = -CPU_INF;
        Move iterationBest = bestMove;
        int iterationComplete = 1;
        int attempt;

        if (completedDepth > 0)
        {
            alpha = rootBestScore - CPU_ASPIRATION_WINDOW;
            beta = rootBestScore + CPU_ASPIRATION_WINDOW;
        }
        else
        {
            alpha = -CPU_INF;
            beta = CPU_INF;
        }

        for (attempt = 0; attempt < 2; attempt++)
        {
            int windowAlpha = alpha;
            int windowBeta = beta;
            int rootMoveLimit = legalMoves.count;

            bestScore = -CPU_INF;
            iterationBest = bestMove;
            iterationComplete = 1;

            orderMoves(&root, &legalMoves, cpuSide, previousBest, 0);

            if (currentDepth >= activePreset->rootLimitDepth &&
                rootMoveLimit > activePreset->rootMoveLimit)
            {
                rootMoveLimit = activePreset->rootMoveLimit;
            }

            for (i = 0; i < rootMoveLimit; i++)
            {
                Move move = legalMoves.moves[i];
                MoveRecord record;
                int quietMove = !moveIsNoisy(&root, move, cpuSide);
                int score;

                applyMoveRecord(&root, move, &record);

                if (i > 0)
                {
                    score = -alphaBeta(&root,
                                       currentDepth - 1,
                                       -windowAlpha - 1,
                                       -windowAlpha,
                                       oppositeColorLocal(cpuSide),
                                       1);
                }
                else
                {
                    score = windowAlpha - 1;
                }

                if (i == 0 || (!searchStopped && score > windowAlpha && score < windowBeta))
                {
                    score = -alphaBeta(&root,
                                       currentDepth - 1,
                                       -windowBeta,
                                       -windowAlpha,
                                       oppositeColorLocal(cpuSide),
                                       1);
                }

                undoMove(&root, &record);

                if (searchStopped)
                {
                    iterationComplete = 0;
                    break;
                }

                if (score > bestScore ||
                    (score == bestScore && movesAreEqual(move, previousBest)))
                {
                    bestScore = score;
                    iterationBest = move;
                }

                if (score > windowAlpha)
                {
                    windowAlpha = score;
                }

                if (windowAlpha >= windowBeta)
                {
                    if (quietMove)
                    {
                        rememberKillerMove(0, move);
                        addHistoryScore(cpuSide, move, currentDepth);
                    }

                    break;
                }
            }

            if (!iterationComplete)
            {
                break;
            }

            if (attempt == 0 &&
                completedDepth > 0 &&
                (bestScore <= alpha || bestScore >= beta))
            {
                alpha = -CPU_INF;
                beta = CPU_INF;
                continue;
            }

            break;
        }

        if (!iterationComplete)
        {
            break;
        }

        bestMove = iterationBest;
        previousBest = bestMove;
        rootBestScore = bestScore;
        completedDepth = currentDepth;
    }

    if (!moveIsValid(bestMove))
    {
        return makeInvalidMove();
    }

    {
        clock_t searchEndClock = clock();
        double timeMs = ((double)(searchEndClock - searchStartClock) * 1000.0) /
                        (double)CLOCKS_PER_SEC;

        totalSearchCount++;
        totalSearchNodeCount += searchNodeCount;
        totalSearchTtHits += searchTtHits;
        totalSearchTtCutoffs += searchTtCutoffs;
        totalSearchTimeMs += timeMs;

        lastSearchStats.bestMove = bestMove;
        lastSearchStats.side = cpuSide;
        lastSearchStats.requestedDepth = depth;
        lastSearchStats.completedDepth = completedDepth;
        lastSearchStats.score = rootBestScore;
        lastSearchStats.nodes = searchNodeCount;
        lastSearchStats.totalNodes = totalSearchNodeCount;
        lastSearchStats.timeMs = timeMs;
        lastSearchStats.averageTimeMs = totalSearchTimeMs / (double)totalSearchCount;
        lastSearchStats.nps = (timeMs > 0.0)
            ? (unsigned long)((double)searchNodeCount * 1000.0 / timeMs)
            : 0u;
        lastSearchStats.ttHits = searchTtHits;
        lastSearchStats.ttCutoffs = searchTtCutoffs;
        lastSearchStats.totalTtHits = totalSearchTtHits;
        lastSearchStats.totalTtCutoffs = totalSearchTtCutoffs;
    }

    return bestMove;
}

//precondition: Take in the game state and CPU side color
//postcondition: Return the best move found by the default CPU search
Move chooseBestMove(const GameState *state, PieceColor cpuSide)
{
    return chooseBestMoveDepth(state, cpuSide, CPU_SEARCH_DEPTH);
}

//precondition: Take in a positive millisecond limit
//postcondition: Set the CPU search time limit used for deep searches
void setCpuSearchTimeLimitMs(int milliseconds)
{
    if (milliseconds > 0)
    {
        cpuSearchTimeLimitMs = milliseconds;
    }
}

//precondition: Take in a preset name: fast, balanced, strong, sharp, or storm
//postcondition: Apply the requested CPU search preset
void setCpuSearchPreset(const char *presetName)
{
    if (presetName == NULL)
    {
        return;
    }

    if (strcmp(presetName, "strong") == 0)
    {
        activePreset = &CPU_PRESET_STRONG;
    }
    else if (strcmp(presetName, "sharp") == 0)
    {
        activePreset = &CPU_PRESET_SHARP;
    }
    else if (strcmp(presetName, "storm") == 0)
    {
        activePreset = &CPU_PRESET_STORM;
    }
    else if (strcmp(presetName, "balanced") == 0)
    {
        activePreset = &CPU_PRESET_BALANCED;
    }
    else
    {
        activePreset = &CPU_PRESET_FAST;
    }
}

//precondition: Take in a positive megabyte target
//postcondition: Resize and clear the CPU transposition table; return actual MB allocated, or 0 on failure
unsigned long setCpuTranspositionTableSizeMb(int megabytes)
{
    size_t bytes;
    size_t entries;

    if (megabytes <= 0)
    {
        return 0u;
    }

    bytes = (size_t)megabytes * 1024u * 1024u;
    entries = bytes / sizeof(TranspositionEntry);

    if (!allocateTranspositionTable(entries))
    {
        return 0u;
    }

    return getCpuTranspositionTableSizeMb();
}

//precondition: None
//postcondition: Return the currently allocated CPU transposition table size in MB
unsigned long getCpuTranspositionTableSizeMb(void)
{
    size_t bytes;

    if (!ensureTranspositionTable())
    {
        return 0u;
    }

    bytes = transpositionTableSize * sizeof(*transpositionTable);
    return (unsigned long)(bytes / (1024u * 1024u));
}

//precondition: None
//postcondition: Return the active CPU search preset name
const char *getCpuSearchPresetName(void)
{
    return activePreset->name;
}

//precondition: Take in an output pointer
//postcondition: Fill output with the most recent CPU search statistics
void getLastCpuSearchStats(CpuSearchStats *stats)
{
    if (stats == NULL)
    {
        return;
    }

    *stats = lastSearchStats;
}
