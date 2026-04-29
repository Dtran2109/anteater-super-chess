#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "types.h"
#include "board.h"
#include "game.h"
#include "cpu.h"
#include "eval.h"

#define MAX_MATCH_PLIES 160
#define DEFAULT_RANDOM_OPENING_PLIES 6
#define DEFAULT_ENGINE_MATCH_CPU_DEPTH 10
#define DEFAULT_ENGINE_MATCH_TIME_MS 1000
#define DEFAULT_ENGINE_MATCH_PRESET "fast"

#define MATCH_HISTORY_LIMIT 512
#define REPETITION_LIMIT 3

#define REPEAT_POSITION_PENALTY 800
#define IMMEDIATE_UNDO_PENALTY 500
#define CAPTURE_PROGRESS_BONUS 50
#define PAWN_PROGRESS_BONUS 15

static int engineMatchCpuDepth = DEFAULT_ENGINE_MATCH_CPU_DEPTH;
static int engineMatchTimeMs = DEFAULT_ENGINE_MATCH_TIME_MS;
static const char *engineMatchPreset = DEFAULT_ENGINE_MATCH_PRESET;

typedef enum
{
    END_CHECKMATE_OR_NO_MOVE,
    END_KING_CAPTURED,
    END_MAX_PLIES_ADJUDICATION,
    END_ILLEGAL_MOVE,
    END_REPETITION,
    END_UNKNOWN
} EndReason;

typedef struct
{
    PieceColor winner;
    int plies;
    EndReason reason;
    int finalEvalPst0;
    int finalEvalPst50;
    int finalEvalPst100;
} MatchResult;

/* ---------- Forward declarations ---------- */

static PieceColor oppositeColorLocal(PieceColor side);
static const char *sideName(PieceColor side);
static const char *reasonName(EndReason reason);

static Move makeInvalidMoveLocal(void);
static int movesAreReverse(Move previousMove, Move currentMove);

static unsigned long long hashGameStateLocal(const GameState *state);
static int countHashInHistoryLocal(const unsigned long long *history,
                                   int historyCount,
                                   unsigned long long hash);
static void addHashToHistoryLocal(unsigned long long *history,
                                  int *historyCount,
                                  unsigned long long hash);

static int materialOfSideLocal(const GameState *state, PieceColor side);
static int actualCaptureGainFromMoveLocal(const GameState *state,
                                          Move move,
                                          PieceColor side);

static void moveToStringLocal(Move move, char *buffer, size_t size);
static void serializeBoardLocal(const GameState *state, char *buffer, size_t size);
static char pieceToChar(ChessPiece piece);

static void initMatchState(GameState *state);
static int evalWhiteWithPstScale(const GameState *state, int pstScale);
static PieceColor adjudicateWinner(const GameState *state);
static void fillFinalEvals(const GameState *state, MatchResult *result);
static MatchResult makeResult(const GameState *state,
                              PieceColor winner,
                              int plies,
                              EndReason reason);

static Move chooseSafeMove(const GameState *state,
                           PieceColor side,
                           int useRandom,
                           Move previousMove,
                           const unsigned long long *positionHistory,
                           int historyCount);

static MatchResult playOneGame(const char *matchupName,
                               int matchupGameId,
                               const char *whiteEngineName,
                               const char *blackEngineName,
                               int whitePstScale,
                               int blackPstScale,
                               int randomOpeningPlies,
                               FILE *moveLog);

static void runMatch(const char *matchupName,
                     const char *nameA,
                     int pstA,
                     const char *nameB,
                     int pstB,
                     int games,
                     int randomOpeningPlies,
                     FILE *gameLog,
                     FILE *moveLog,
                     FILE *summaryLog);

/* ---------- Small helpers ---------- */

static PieceColor oppositeColorLocal(PieceColor side)
{
    if (side == WHITE)
    {
        return BLACK;
    }

    if (side == BLACK)
    {
        return WHITE;
    }

    return NONE;
}

static const char *sideName(PieceColor side)
{
    if (side == WHITE)
    {
        return "WHITE";
    }

    if (side == BLACK)
    {
        return "BLACK";
    }

    return "NONE";
}

static const char *reasonName(EndReason reason)
{
    switch (reason)
    {
        case END_CHECKMATE_OR_NO_MOVE:
            return "CHECKMATE_OR_NO_MOVE";
        case END_KING_CAPTURED:
            return "KING_CAPTURED";
        case END_MAX_PLIES_ADJUDICATION:
            return "MAX_PLIES_ADJUDICATION";
        case END_ILLEGAL_MOVE:
            return "ILLEGAL_MOVE";
        case END_REPETITION:
            return "REPETITION";
        default:
            return "UNKNOWN";
    }
}

static Move makeInvalidMoveLocal(void)
{
    Move move;

    move.start.row = -1;
    move.start.col = -1;
    move.dest.row = -1;
    move.dest.col = -1;
    move.promotionGoal = EMPTY;

    return move;
}

static int movesAreReverse(Move previousMove, Move currentMove)
{
    return previousMove.start.row == currentMove.dest.row &&
           previousMove.start.col == currentMove.dest.col &&
           previousMove.dest.row == currentMove.start.row &&
           previousMove.dest.col == currentMove.start.col;
}

static int materialOfSideLocal(const GameState *state, PieceColor side)
{
    int row;
    int col;
    int total = 0;

    for (row = 0; row < BOARD_ROWS; row++)
    {
        for (col = 0; col < BOARD_COLS; col++)
        {
            ChessPiece piece = state->squares[row][col];

            if (piece.type != EMPTY && piece.color == side)
            {
                total += getPieceValue(piece.type);
            }
        }
    }

    return total;
}

static int actualCaptureGainFromMoveLocal(const GameState *state, Move move, PieceColor side)
{
    GameState copy;
    PieceColor enemy;
    int before;
    int after;

    copy = *state;
    enemy = oppositeColorLocal(side);

    before = materialOfSideLocal(state, enemy);
    applyMove(&copy, move);
    after = materialOfSideLocal(&copy, enemy);

    return before - after;
}

/*
   Repetition hash for match testing.
   IMPORTANT: This ignores MoveCount, because pieces moving back and forth should
   still count as the same repeated board position.
*/
static unsigned long long hashGameStateLocal(const GameState *state)
{
    unsigned long long hash = 1469598103934665603ULL;
    int row;
    int col;

    for (row = 0; row < BOARD_ROWS; row++)
    {
        for (col = 0; col < BOARD_COLS; col++)
        {
            ChessPiece piece = state->squares[row][col];
            unsigned long long value;

            value = (unsigned long long)(piece.type + 17 * piece.color);

            hash ^= value;
            hash *= 1099511628211ULL;
        }
    }

    hash ^= (unsigned long long)(state->currentTurn + 97);
    hash *= 1099511628211ULL;

    return hash;
}

static int countHashInHistoryLocal(const unsigned long long *history,
                                   int historyCount,
                                   unsigned long long hash)
{
    int i;
    int count = 0;

    for (i = 0; i < historyCount; i++)
    {
        if (history[i] == hash)
        {
            count++;
        }
    }

    return count;
}

static void addHashToHistoryLocal(unsigned long long *history,
                                  int *historyCount,
                                  unsigned long long hash)
{
    int i;

    if (*historyCount < MATCH_HISTORY_LIMIT)
    {
        history[*historyCount] = hash;
        (*historyCount)++;
        return;
    }

    for (i = 1; i < MATCH_HISTORY_LIMIT; i++)
    {
        history[i - 1] = history[i];
    }

    history[MATCH_HISTORY_LIMIT - 1] = hash;
}

static void moveToStringLocal(Move move, char *buffer, size_t size)
{
    char startFile;
    char startRank;
    char destFile;
    char destRank;

    if (buffer == NULL || size < 6)
    {
        return;
    }

    if (move.start.row < 0 || move.start.col < 0 ||
        move.dest.row < 0 || move.dest.col < 0)
    {
        snprintf(buffer, size, "NONE");
        return;
    }

    startFile = (char)('A' + move.start.col);
    startRank = (char)('1' + (BOARD_ROWS - 1 - move.start.row));

    destFile = (char)('A' + move.dest.col);
    destRank = (char)('1' + (BOARD_ROWS - 1 - move.dest.row));

    snprintf(buffer, size, "%c%c%c%c", startFile, startRank, destFile, destRank);
}

static char pieceToChar(ChessPiece piece)
{
    if (piece.type == EMPTY || piece.color == NONE)
    {
        return '.';
    }

    switch (piece.type)
    {
        case PAWN:
            return (piece.color == WHITE) ? 'P' : 'p';
        case ROOK:
            return (piece.color == WHITE) ? 'R' : 'r';
        case KING:
            return (piece.color == WHITE) ? 'K' : 'k';
        case QUEEN:
            return (piece.color == WHITE) ? 'Q' : 'q';
        case KNIGHT:
            return (piece.color == WHITE) ? 'N' : 'n';
        case BISHOP:
            return (piece.color == WHITE) ? 'B' : 'b';
        case ANTEATER:
            return (piece.color == WHITE) ? 'A' : 'a';
        default:
            return '?';
    }
}

static void serializeBoardLocal(const GameState *state, char *buffer, size_t size)
{
    int row;
    int col;
    size_t index = 0;

    if (buffer == NULL || size == 0)
    {
        return;
    }

    for (row = 0; row < BOARD_ROWS; row++)
    {
        for (col = 0; col < BOARD_COLS; col++)
        {
            if (index + 2 >= size)
            {
                buffer[index] = '\0';
                return;
            }

            buffer[index++] = pieceToChar(state->squares[row][col]);
        }

        if (row != BOARD_ROWS - 1)
        {
            if (index + 2 >= size)
            {
                buffer[index] = '\0';
                return;
            }

            buffer[index++] = '/';
        }
    }

    buffer[index] = '\0';
}

static void initMatchState(GameState *state)
{
    initializeGameState(state);
}

static int evalWhiteWithPstScale(const GameState *state, int pstScale)
{
    int result;

    setPstScalePercent(pstScale);
    result = evaluatePosition(state, WHITE);

    return result;
}

static PieceColor adjudicateWinner(const GameState *state)
{
    int whiteScore;

    if (isKingDead(state, WHITE))
    {
        return BLACK;
    }

    if (isKingDead(state, BLACK))
    {
        return WHITE;
    }

    whiteScore = evalWhiteWithPstScale(state, 50);

    if (whiteScore > 100)
    {
        return WHITE;
    }

    if (whiteScore < -100)
    {
        return BLACK;
    }

    return NONE;
}

static void fillFinalEvals(const GameState *state, MatchResult *result)
{
    result->finalEvalPst0 = evalWhiteWithPstScale(state, 0);
    result->finalEvalPst50 = evalWhiteWithPstScale(state, 50);
    result->finalEvalPst100 = evalWhiteWithPstScale(state, 100);
}

static MatchResult makeResult(const GameState *state,
                              PieceColor winner,
                              int plies,
                              EndReason reason)
{
    MatchResult result;

    result.winner = winner;
    result.plies = plies;
    result.reason = reason;

    fillFinalEvals(state, &result);

    return result;
}

/*
   Safe move selection:
   1. Random openings use verified legal random moves.
   2. Normal engine first tries chooseBestMove(), preserving alpha-beta/minimax.
   3. If chooseBestMove() is illegal or repetition-prone, fallback to a shallow
      legal move scorer with repetition/progress penalties.
*/
static Move chooseSafeMove(const GameState *state,
                           PieceColor side,
                           int useRandom,
                           Move previousMove,
                           const unsigned long long *positionHistory,
                           int historyCount)
{
    MoveList legalMoves;
    MoveList safeMoves;
    Move candidate;
    int i;

    safeMoves.count = 0;

    generateAllLegalMoves(state, side, &legalMoves);

    for (i = 0; i < legalMoves.count; i++)
    {
        if (isMoveLegal(state, legalMoves.moves[i]))
        {
            if (safeMoves.count < MAX_MOVES)
            {
                safeMoves.moves[safeMoves.count] = legalMoves.moves[i];
                safeMoves.count++;
            }
        }
    }

    if (safeMoves.count <= 0)
    {
        return makeInvalidMoveLocal();
    }

    if (useRandom)
    {
        return safeMoves.moves[rand() % safeMoves.count];
    }

    /* First try the real CPU search move. */
    candidate = chooseBestMoveDepth(state, side, engineMatchCpuDepth);
    {
        CpuSearchStats stats;
        char candidateString[16];

        getLastCpuSearchStats(&stats);
        moveToStringLocal(stats.bestMove, candidateString, sizeof(candidateString));

        printf("CPU depth=%d/%d side=%s move=%s score=%d "
               "nodes=%lu total_nodes=%llu time=%.2f ms avg=%.2f ms "
               "nps=%lu tt_hits=%lu tt_cutoffs=%lu "
               "total_tt_hits=%llu total_tt_cutoffs=%llu\n",
               stats.completedDepth,
               stats.requestedDepth,
               sideName(stats.side),
               candidateString,
               stats.score,
               stats.nodes,
               stats.totalNodes,
               stats.timeMs,
               stats.averageTimeMs,
               stats.nps,
               stats.ttHits,
               stats.ttCutoffs,
               stats.totalTtHits,
               stats.totalTtCutoffs);
    }

    if (candidate.start.row >= 0 && isMoveLegal(state, candidate))
    {
        GameState copy = *state;
        unsigned long long nextHash;
        int repeatCount;

        applyMove(&copy, candidate);
        nextHash = hashGameStateLocal(&copy);
        repeatCount = countHashInHistoryLocal(positionHistory, historyCount, nextHash);

        if (repeatCount == 0 &&
            !(previousMove.start.row >= 0 && movesAreReverse(previousMove, candidate)))
        {
            return candidate;
        }
    }

    /* Fallback: legal 1-ply selector only when the real engine move is unsafe. */
    {
        Move bestMove = safeMoves.moves[0];
        int bestScore = -100000000;

        for (i = 0; i < safeMoves.count; i++)
        {
            GameState copy = *state;
            Move move = safeMoves.moves[i];
            int score;
            int repeatCount;
            unsigned long long nextHash;
            int captureGain;

            applyMove(&copy, move);
            score = evaluatePosition(&copy, side);

            nextHash = hashGameStateLocal(&copy);
            repeatCount = countHashInHistoryLocal(positionHistory, historyCount, nextHash);

            if (repeatCount > 0)
            {
                score -= repeatCount * REPEAT_POSITION_PENALTY;
            }

            if (previousMove.start.row >= 0 && movesAreReverse(previousMove, move))
            {
                score -= IMMEDIATE_UNDO_PENALTY;
            }

            captureGain = actualCaptureGainFromMoveLocal(state, move, side);
            if (captureGain > 0)
            {
                score += captureGain + CAPTURE_PROGRESS_BONUS;
            }

            {
                ChessPiece movingPiece = state->squares[move.start.row][move.start.col];

                if (movingPiece.type == PAWN)
                {
                    if (movingPiece.color == WHITE && move.dest.row < move.start.row)
                    {
                        score += PAWN_PROGRESS_BONUS;
                    }
                    else if (movingPiece.color == BLACK && move.dest.row > move.start.row)
                    {
                        score += PAWN_PROGRESS_BONUS;
                    }
                }
            }

            if (score > bestScore)
            {
                bestScore = score;
                bestMove = move;
            }
        }

        return bestMove;
    }
}

static MatchResult playOneGame(const char *matchupName,
                               int matchupGameId,
                               const char *whiteEngineName,
                               const char *blackEngineName,
                               int whitePstScale,
                               int blackPstScale,
                               int randomOpeningPlies,
                               FILE *moveLog)
{
    GameState state;
    int ply;
    unsigned long long positionHistory[MATCH_HISTORY_LIMIT];
    int historyCount = 0;
    Move previousMove = makeInvalidMoveLocal();

    initMatchState(&state);
    addHashToHistoryLocal(positionHistory, &historyCount, hashGameStateLocal(&state));

    for (ply = 0; ply < MAX_MATCH_PLIES; ply++)
    {
        PieceColor side;
        const char *engineName;
        int pstScale;
        Move move;
        char moveString[16];
        char boardBefore[BOARD_ROWS * BOARD_COLS + BOARD_ROWS + 8];
        char boardAfter[BOARD_ROWS * BOARD_COLS + BOARD_ROWS + 8];
        int evalBeforePst0;
        int evalBeforePst50;
        int evalBeforePst100;
        int evalAfterPst0;
        int evalAfterPst50;
        int evalAfterPst100;
        int usedRandomOpening;
        CpuSearchStats searchStats;

        memset(&searchStats, 0, sizeof(searchStats));

        if (isKingDead(&state, WHITE))
        {
            return makeResult(&state, BLACK, ply, END_KING_CAPTURED);
        }

        if (isKingDead(&state, BLACK))
        {
            return makeResult(&state, WHITE, ply, END_KING_CAPTURED);
        }

        side = state.currentTurn;

        if (side == WHITE)
        {
            engineName = whiteEngineName;
            pstScale = whitePstScale;
        }
        else
        {
            engineName = blackEngineName;
            pstScale = blackPstScale;
        }

        serializeBoardLocal(&state, boardBefore, sizeof(boardBefore));

        evalBeforePst0 = evalWhiteWithPstScale(&state, 0);
        evalBeforePst50 = evalWhiteWithPstScale(&state, 50);
        evalBeforePst100 = evalWhiteWithPstScale(&state, 100);

        setPstScalePercent(pstScale);
        usedRandomOpening = (ply < randomOpeningPlies);

        move = chooseSafeMove(&state,
                              side,
                              usedRandomOpening,
                              previousMove,
                              positionHistory,
                              historyCount);

        if (!usedRandomOpening)
        {
            getLastCpuSearchStats(&searchStats);
        }

        moveToStringLocal(move, moveString, sizeof(moveString));

        if (move.start.row < 0 || move.start.col < 0)
        {
            PieceColor winner;

            if (isKingInCheck(&state, side))
            {
                winner = oppositeColorLocal(side);
            }
            else
            {
                winner = adjudicateWinner(&state);
            }

            return makeResult(&state, winner, ply, END_CHECKMATE_OR_NO_MOVE);
        }

        if (!isMoveLegal(&state, move))
        {
            fprintf(stderr,
                    "ERROR: illegal move selected in match=%s game=%d ply=%d side=%s engine=%s move=%s board=%s\n",
                    matchupName,
                    matchupGameId,
                    ply,
                    sideName(side),
                    engineName,
                    moveString,
                    boardBefore);

            return makeResult(&state, oppositeColorLocal(side), ply, END_ILLEGAL_MOVE);
        }

        applyMove(&state, move);
        previousMove = move;

        {
            unsigned long long newHash;
            int repeatCount;

            newHash = hashGameStateLocal(&state);
            addHashToHistoryLocal(positionHistory, &historyCount, newHash);
            repeatCount = countHashInHistoryLocal(positionHistory, historyCount, newHash);

            if (repeatCount >= REPETITION_LIMIT)
            {
                return makeResult(&state, NONE, ply + 1, END_REPETITION);
            }
        }

        serializeBoardLocal(&state, boardAfter, sizeof(boardAfter));

        evalAfterPst0 = evalWhiteWithPstScale(&state, 0);
        evalAfterPst50 = evalWhiteWithPstScale(&state, 50);
        evalAfterPst100 = evalWhiteWithPstScale(&state, 100);

        if (moveLog != NULL)
        {
            fprintf(moveLog,
                    "%s,%d,%d,%s,%s,%d,%s,%d,%d,%d,%lu,%llu,%.2f,%.2f,%lu,%lu,%lu,%llu,%llu,%s,%d,%d,%d,%d,%d,%d,%s,%s\n",
                    matchupName,
                    matchupGameId,
                    ply,
                    sideName(side),
                    engineName,
                    pstScale,
                    moveString,
                    usedRandomOpening,
                    searchStats.completedDepth,
                    searchStats.requestedDepth,
                    searchStats.nodes,
                    searchStats.totalNodes,
                    searchStats.timeMs,
                    searchStats.averageTimeMs,
                    searchStats.nps,
                    searchStats.ttHits,
                    searchStats.ttCutoffs,
                    searchStats.totalTtHits,
                    searchStats.totalTtCutoffs,
                    boardBefore,
                    evalBeforePst0,
                    evalBeforePst50,
                    evalBeforePst100,
                    evalAfterPst0,
                    evalAfterPst50,
                    evalAfterPst100,
                    boardAfter,
                    sideName(state.currentTurn));
            fflush(moveLog);
        }
    }

    return makeResult(&state, adjudicateWinner(&state), MAX_MATCH_PLIES, END_MAX_PLIES_ADJUDICATION);
}

static void runMatch(const char *matchupName,
                     const char *nameA,
                     int pstA,
                     const char *nameB,
                     int pstB,
                     int games,
                     int randomOpeningPlies,
                     FILE *gameLog,
                     FILE *moveLog,
                     FILE *summaryLog)
{
    int whiteWins = 0;
    int blackWins = 0;
    int aWins = 0;
    int bWins = 0;
    int draws = 0;
    int totalPlies = 0;
    int i;

    for (i = 0; i < games; i++)
    {
        MatchResult result;
        const char *whiteEngineName;
        const char *blackEngineName;
        int whitePstScale;
        int blackPstScale;

        if ((i % 2) == 0)
        {
            whiteEngineName = nameA;
            blackEngineName = nameB;
            whitePstScale = pstA;
            blackPstScale = pstB;

            result = playOneGame(matchupName,
                                 i + 1,
                                 whiteEngineName,
                                 blackEngineName,
                                 whitePstScale,
                                 blackPstScale,
                                 randomOpeningPlies,
                                 moveLog);

            if (result.winner == WHITE)
            {
                aWins++;
            }
            else if (result.winner == BLACK)
            {
                bWins++;
            }
            else
            {
                draws++;
            }
        }
        else
        {
            whiteEngineName = nameB;
            blackEngineName = nameA;
            whitePstScale = pstB;
            blackPstScale = pstA;

            result = playOneGame(matchupName,
                                 i + 1,
                                 whiteEngineName,
                                 blackEngineName,
                                 whitePstScale,
                                 blackPstScale,
                                 randomOpeningPlies,
                                 moveLog);

            if (result.winner == WHITE)
            {
                bWins++;
            }
            else if (result.winner == BLACK)
            {
                aWins++;
            }
            else
            {
                draws++;
            }
        }

        if (result.winner == WHITE)
        {
            whiteWins++;
        }
        else if (result.winner == BLACK)
        {
            blackWins++;
        }

        totalPlies += result.plies;

        if (gameLog != NULL)
        {
            fprintf(gameLog,
                    "%s,%d,%s,%s,%d,%d,%s,%d,%s,%d,%d,%d\n",
                    matchupName,
                    i + 1,
                    whiteEngineName,
                    blackEngineName,
                    whitePstScale,
                    blackPstScale,
                    sideName(result.winner),
                    result.plies,
                    reasonName(result.reason),
                    result.finalEvalPst0,
                    result.finalEvalPst50,
                    result.finalEvalPst100);
            fflush(gameLog);
        }
    }

    {
        double aScore = (double)aWins + 0.5 * (double)draws;
        double bScore = (double)bWins + 0.5 * (double)draws;
        double avgPlies = (games > 0) ? ((double)totalPlies / (double)games) : 0.0;

        printf("%s, %d games\n", matchupName, games);
        printf("  %s wins: %d\n", nameA, aWins);
        printf("  %s wins: %d\n", nameB, bWins);
        printf("  Draws:   %d\n", draws);
        printf("  White wins: %d\n", whiteWins);
        printf("  Black wins: %d\n", blackWins);
        printf("  %s score: %.1f / %d\n", nameA, aScore, games);
        printf("  %s score: %.1f / %d\n", nameB, bScore, games);
        printf("  Average plies: %.2f\n\n", avgPlies);

        if (summaryLog != NULL)
        {
            fprintf(summaryLog, "%s, %d games\n", matchupName, games);
            fprintf(summaryLog, "  %s wins: %d\n", nameA, aWins);
            fprintf(summaryLog, "  %s wins: %d\n", nameB, bWins);
            fprintf(summaryLog, "  Draws:   %d\n", draws);
            fprintf(summaryLog, "  White wins: %d\n", whiteWins);
            fprintf(summaryLog, "  Black wins: %d\n", blackWins);
            fprintf(summaryLog, "  %s score: %.1f / %d\n", nameA, aScore, games);
            fprintf(summaryLog, "  %s score: %.1f / %d\n", nameB, bScore, games);
            fprintf(summaryLog, "  Average plies: %.2f\n\n", avgPlies);
            fflush(summaryLog);
        }
    }
}

int main(int argc, char **argv)
{
    int games = 100;
    int randomOpeningPlies = DEFAULT_RANDOM_OPENING_PLIES;
    int ttMb = 0;
    const char *outputPrefix = "engine_match";

    char gameLogPath[256];
    char moveLogPath[256];
    char summaryLogPath[256];

    FILE *gameLog;
    FILE *moveLog;
    FILE *summaryLog;

    if (argc >= 2)
    {
        games = atoi(argv[1]);
    }

    if (argc >= 3)
    {
        randomOpeningPlies = atoi(argv[2]);
    }

    if (argc >= 4)
    {
        outputPrefix = argv[3];
    }

    if (argc >= 5)
    {
        engineMatchCpuDepth = atoi(argv[4]);
    }

    if (argc >= 6)
    {
        engineMatchTimeMs = atoi(argv[5]);
    }

    if (argc >= 7)
    {
        engineMatchPreset = argv[6];
    }

    if (argc >= 8)
    {
        ttMb = atoi(argv[7]);
    }

    snprintf(gameLogPath, sizeof(gameLogPath), "%s_games.csv", outputPrefix);
    snprintf(moveLogPath, sizeof(moveLogPath), "%s_moves.csv", outputPrefix);
    snprintf(summaryLogPath, sizeof(summaryLogPath), "%s_summary.txt", outputPrefix);

    if (games <= 0)
    {
        games = 100;
    }

    if (randomOpeningPlies < 0)
    {
        randomOpeningPlies = 0;
    }

    if (engineMatchCpuDepth <= 0)
    {
        engineMatchCpuDepth = DEFAULT_ENGINE_MATCH_CPU_DEPTH;
    }

    if (engineMatchTimeMs <= 0)
    {
        engineMatchTimeMs = DEFAULT_ENGINE_MATCH_TIME_MS;
    }

    setCpuSearchTimeLimitMs(engineMatchTimeMs);
    setCpuSearchPreset(engineMatchPreset);
    if (ttMb > 0 && setCpuTranspositionTableSizeMb(ttMb) == 0u)
    {
        fprintf(stderr, "Warning: could not allocate %d MB transposition table.\n", ttMb);
    }

    srand((unsigned int)(time(NULL) ^ getpid()));

    gameLog = fopen(gameLogPath, "w");
    moveLog = fopen(moveLogPath, "w");
    summaryLog = fopen(summaryLogPath, "w");

    if (gameLog == NULL || moveLog == NULL || summaryLog == NULL)
    {
        fprintf(stderr, "ERROR: could not open one or more log files.\n");

        if (gameLog != NULL) fclose(gameLog);
        if (moveLog != NULL) fclose(moveLog);
        if (summaryLog != NULL) fclose(summaryLog);

        return 1;
    }

    fprintf(gameLog,
            "matchup,game_id,white_engine,black_engine,pst_white,pst_black,winner,plies,end_reason,final_eval_pst0,final_eval_pst50,final_eval_pst100\n");

    fprintf(moveLog,
            "matchup,game_id,ply,side_to_move,engine,pst_scale,move,random_opening,search_depth,search_requested_depth,search_nodes,search_total_nodes,search_time_ms,search_avg_time_ms,search_nps,search_tt_hits,search_tt_cutoffs,search_total_tt_hits,search_total_tt_cutoffs,board_before,eval_before_pst0,eval_before_pst50,eval_before_pst100,eval_after_pst0,eval_after_pst50,eval_after_pst100,board_after,next_turn\n");

    fflush(gameLog);
    fflush(moveLog);

    printf("===== ENGINE MATCH TEST =====\n");
    printf("Games per matchup: %d\n", games);
    printf("Random opening plies: %d\n", randomOpeningPlies);
    printf("CPU search depth: %d\n", engineMatchCpuDepth);
    printf("CPU time limit: %d ms\n", engineMatchTimeMs);
    printf("CPU preset: %s\n", getCpuSearchPresetName());
    printf("CPU TT: %lu MB\n", getCpuTranspositionTableSizeMb());
    printf("Game log: %s\n", gameLogPath);
    printf("Move log: %s\n", moveLogPath);
    printf("Summary log: %s\n\n", summaryLogPath);

    fprintf(summaryLog, "===== ENGINE MATCH TEST =====\n");
    fprintf(summaryLog, "Games per matchup: %d\n", games);
    fprintf(summaryLog, "Random opening plies: %d\n", randomOpeningPlies);
    fprintf(summaryLog, "CPU search depth: %d\n", engineMatchCpuDepth);
    fprintf(summaryLog, "CPU time limit: %d ms\n", engineMatchTimeMs);
    fprintf(summaryLog, "CPU preset: %s\n", getCpuSearchPresetName());
    fprintf(summaryLog, "CPU TT: %lu MB\n\n", getCpuTranspositionTableSizeMb());
    fflush(summaryLog);

    runMatch("No_PST_vs_PST_Half",
             "No PST", 0,
             "PST Half", 50,
             games,
             randomOpeningPlies,
             gameLog,
             moveLog,
             summaryLog);

    runMatch("No_PST_vs_PST_Full",
             "No PST", 0,
             "PST Full", 100,
             games,
             randomOpeningPlies,
             gameLog,
             moveLog,
             summaryLog);

    runMatch("PST_Half_vs_PST_Full",
             "PST Half", 50,
             "PST Full", 100,
             games,
             randomOpeningPlies,
             gameLog,
             moveLog,
             summaryLog);
    runMatch("No_PST_vs_No_PST",
         "No PST A",
         0,
         "No PST B",
         0,
         games,
         randomOpeningPlies,
         gameLog,
         moveLog,
         summaryLog);
    runMatch("PST_Half_vs_PST_Half",
         "PST Half A",
         50,
         "PST Half B",
         50,
         games,
         randomOpeningPlies,
         gameLog,
         moveLog,
         summaryLog);
runMatch("PST_Full_vs_PST_Full",
         "PST Full A",
         100,
         "PST Full B",
         100,
         games,
         randomOpeningPlies,
         gameLog,
         moveLog,
         summaryLog);
    fclose(gameLog);
    fclose(moveLog);
    fclose(summaryLog);

    printf("Logs written:\n");
    printf("  %s\n", gameLogPath);
    printf("  %s\n", moveLogPath);
    printf("  %s\n", summaryLogPath);

    return 0;
}
