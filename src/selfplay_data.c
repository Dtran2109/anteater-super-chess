#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "types.h"
#include "moves.h"
#include "board.h"
#include "game.h"
#include "cpu.h"
#include "eval.h"
#include "data_logger.h"

#ifndef MAX_GAME_MOVES
#define MAX_GAME_MOVES 1024
#endif
#define RESIGN_MIN_PLY 60
#define RESIGN_EVAL_THRESHOLD 4500
#define ADJUDICATION_DRAW_MARGIN 100
#define DEFAULT_GAMES 25
#define DEFAULT_MAX_PLIES 200
#define DEFAULT_SELFPLAY_SEARCH_DEPTH 10
#define DEFAULT_SELFPLAY_TIME_MS 1000
#define DEFAULT_SELFPLAY_PRESET "fast"
#define EXPLORATION_PERCENT 30
#define RECENT_HISTORY_LIMIT 256
#define REPEAT_POSITION_PENALTY 50000
#define IMMEDIATE_UNDO_PENALTY 5000
#define RANDOM_SCORE_JITTER 25

typedef struct {
    Move move;
    int score;
} MoveCandidate;

static int selfplaySearchDepth = DEFAULT_SELFPLAY_SEARCH_DEPTH;
static int selfplayTimeMs = DEFAULT_SELFPLAY_TIME_MS;
static const char *selfplayPreset = DEFAULT_SELFPLAY_PRESET;
static int lastTrainingMoveScore = 0;

static void safeAddMoveLocal(MoveList *list, Move move)
{
    if (list == NULL) return;
    if (list->count >= MAX_MOVES) return;

    list->moves[list->count] = move;
    list->count++;
}

static int moveLooksInsideBoard(Move move)
{
    return move.start.row >= 0 && move.start.row < BOARD_ROWS &&
           move.start.col >= 0 && move.start.col < BOARD_COLS &&
           move.dest.row >= 0 && move.dest.row < BOARD_ROWS &&
           move.dest.col >= 0 && move.dest.col < BOARD_COLS;
}
static PieceColor oppositeColorLocal(PieceColor side) {
    if (side == WHITE) return BLACK;
    if (side == BLACK) return WHITE;
    return NONE;
}

static int movesAreReverse(Move previousMove, Move currentMove) {
    return previousMove.start.row == currentMove.dest.row &&
           previousMove.start.col == currentMove.dest.col &&
           previousMove.dest.row == currentMove.start.row &&
           previousMove.dest.col == currentMove.start.col;
}

static int moveIsInvalid(Move move) {
    return move.start.row < 0 || move.start.col < 0 ||
           move.dest.row < 0 || move.dest.col < 0;
}

static unsigned long long hashGameState(const GameState *state) {
    unsigned long long hash = 1469598103934665603ULL;
    int row, col;

    for (row = 0; row < BOARD_ROWS; row++) {
        for (col = 0; col < BOARD_COLS; col++) {
            ChessPiece piece = state->squares[row][col];
            unsigned long long value;

            value = (unsigned long long)(piece.type + 1);
            value = value * 17ULL + (unsigned long long)(piece.color + 1);
            value = value * 83ULL + (unsigned long long)(row * BOARD_COLS + col + 1);

            hash ^= value;
            hash *= 1099511628211ULL;
        }
    }

    hash ^= (unsigned long long)(state->currentTurn + 11);
    hash *= 1099511628211ULL;

    return hash;
}

static int countHashInHistory(const unsigned long long *history, int historyCount, unsigned long long hash) {
    int i;
    int count = 0;

    for (i = 0; i < historyCount; i++) {
        if (history[i] == hash) count++;
    }

    return count;
}

static int isKingDeadFast(const GameState *state, PieceColor side) {
    int row, col;
    for (row = 0; row < BOARD_ROWS; row++) {
        for (col = 0; col < BOARD_COLS; col++) {
            ChessPiece piece = state->squares[row][col];
            if (piece.type == KING && piece.color == side) return 0;
        }
    }
    return 1;
}

static PieceColor determineTerminalWinner(const GameState *state) {
    if (isKingDeadFast(state, WHITE)) return BLACK;
    if (isKingDeadFast(state, BLACK)) return WHITE;

    if (state->currentTurn == WHITE && isCheckmate(state, WHITE)) return BLACK;
    if (state->currentTurn == BLACK && isCheckmate(state, BLACK)) return WHITE;

    return NONE;
}

static PieceColor adjudicateWinnerByEval(const GameState *state)
{
    int whiteEval;

    if (isKingDeadFast(state, WHITE))
    {
        return BLACK;
    }

    if (isKingDeadFast(state, BLACK))
    {
        return WHITE;
    }

    whiteEval = evaluatePosition(state, WHITE);

    if (whiteEval > ADJUDICATION_DRAW_MARGIN)
    {
        return WHITE;
    }

    if (whiteEval < -ADJUDICATION_DRAW_MARGIN)
    {
        return BLACK;
    }

    return NONE;
}

static int resultForSide(PieceColor winner, PieceColor side) {
    if (winner == NONE) return 0;
    if (winner == side) return 1;
    return -1;
}

static Move makeInvalidMoveLocal(void) {
    Move move;
    move.start.row = -1;
    move.start.col = -1;
    move.dest.row = -1;
    move.dest.col = -1;
    move.promotionGoal = EMPTY;
    return move;
}

static void addCandidateSorted(MoveCandidate *top, int *topCount, int topLimit, Move move, int score) {
    int insertIndex;
    int i;

    if (*topCount < topLimit) {
        insertIndex = *topCount;
        (*topCount)++;
    } else if (score <= top[*topCount - 1].score) {
        return;
    } else {
        insertIndex = *topCount - 1;
    }

    while (insertIndex > 0 && score > top[insertIndex - 1].score) {
        top[insertIndex] = top[insertIndex - 1];
        insertIndex--;
    }

    top[insertIndex].move = move;
    top[insertIndex].score = score;

    for (i = *topCount; i < topLimit; i++) {
        /* no-op, keeps old C compilers from warning about unused in some builds */
        break;
    }
}

static int trainingMoveIsUseful(const GameState *state,
                                Move move,
                                Move previousMove,
                                const unsigned long long *positionHistory,
                                int historyCount)
{
    GameState copy;
    unsigned long long nextHash;

    if (!moveLooksInsideBoard(move) || !isMoveLegal(state, move))
    {
        return 0;
    }

    if (!moveIsInvalid(previousMove) && movesAreReverse(previousMove, move))
    {
        return 0;
    }

    copy = *state;
    applyMove(&copy, move);
    nextHash = hashGameState(&copy);

    return countHashInHistory(positionHistory, historyCount, nextHash) == 0;
}

static int materialOfSide(const GameState *state, PieceColor side)
{
    int row, col;
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

static int actualCaptureGainFromMove(const GameState *state, Move move, PieceColor side)
{
    GameState copy;
    PieceColor enemy;
    int enemyBefore;
    int enemyAfter;

    copy = *state;
    enemy = oppositeColorLocal(side);

    enemyBefore = materialOfSide(state, enemy);

    applyMove(&copy, move);

    enemyAfter = materialOfSide(&copy, enemy);

    return enemyBefore - enemyAfter;
}
static Move chooseTrainingMove(const GameState *state,
                               PieceColor side,
                               Move previousMove,
                               const unsigned long long *positionHistory,
                               int historyCount)
{
    MoveList generatedMoves;
    MoveList safeMoves;
    int i;
    MoveCandidate top[4];
    int topCount = 0;

    safeMoves.count = 0;

    /*
        generateAllLegalMoves() should already return legal moves,
        but training data must be extra safe. We rebuild a verified list.
    */
    generateAllLegalMoves(state, side, &generatedMoves);

    for (i = 0; i < generatedMoves.count; i++)
    {
        Move candidate = generatedMoves.moves[i];

        if (!moveLooksInsideBoard(candidate))
        {
            continue;
        }

        if (!isMoveLegal(state, candidate))
        {
            continue;
        }

        safeAddMoveLocal(&safeMoves, candidate);
    }

    if (safeMoves.count <= 0)
    {
        lastTrainingMoveScore = evaluatePosition(state, side);
        return makeInvalidMoveLocal();
    }

    /*
        Random exploration, but only from verified legal moves.
    */
    if ((rand() % 100) < EXPLORATION_PERCENT)
    {
        int attempts;

        for (attempts = 0; attempts < 16; attempts++)
        {
            Move candidate = safeMoves.moves[rand() % safeMoves.count];

            if (isMoveLegal(state, candidate) &&
                (moveIsInvalid(previousMove) || !movesAreReverse(previousMove, candidate)))
            {
                lastTrainingMoveScore = evaluatePosition(state, side);
                return candidate;
            }
        }

        for (attempts = 0; attempts < safeMoves.count; attempts++)
        {
            Move candidate = safeMoves.moves[attempts];

            if (isMoveLegal(state, candidate))
            {
                lastTrainingMoveScore = evaluatePosition(state, side);
                return candidate;
            }
        }

        lastTrainingMoveScore = evaluatePosition(state, side);
        return makeInvalidMoveLocal();
    }

    {
        Move searchMove = chooseBestMoveDepth(state, side, selfplaySearchDepth);
        CpuSearchStats stats;

        getLastCpuSearchStats(&stats);

        if (trainingMoveIsUseful(state,
                                 searchMove,
                                 previousMove,
                                 positionHistory,
                                 historyCount))
        {
            lastTrainingMoveScore = stats.score;
            return searchMove;
        }
    }

    /*
        Score only verified legal moves as a fallback when the search move would
        make self-play data less useful, for example by repeating immediately.
    */
    for (i = 0; i < safeMoves.count; i++)
    {
        GameState copy = *state;
        Move move = safeMoves.moves[i];
        unsigned long long nextHash;
        int repeatCount;
        int score;

        if (!isMoveLegal(state, move))
        {
            continue;
        }

applyMove(&copy, move);
score = evaluatePosition(&copy, side);

{
    int captureGain = actualCaptureGainFromMove(state, move, side);
    score += captureGain * 5;
}

if (move.promotionGoal != EMPTY)
{
    score += getPieceValue(move.promotionGoal) * 3;
}

        nextHash = hashGameState(&copy);
        repeatCount = countHashInHistory(positionHistory, historyCount, nextHash);
        score -= repeatCount * REPEAT_POSITION_PENALTY;

        if (!moveIsInvalid(previousMove) && movesAreReverse(previousMove, move))
        {
            score -= IMMEDIATE_UNDO_PENALTY;
        }

        score += (rand() % (2 * RANDOM_SCORE_JITTER + 1)) - RANDOM_SCORE_JITTER;

        addCandidateSorted(top, &topCount, 4, move, score);
    }

    if (topCount <= 0)
    {
        return makeInvalidMoveLocal();
    }

    if (topCount > 1 && (rand() % 100) < 20)
    {
        int tries;

        for (tries = 0; tries < 8; tries++)
        {
            int index = rand() % topCount;
            Move candidate = top[index].move;
            int candidateScore = top[index].score;

            if (isMoveLegal(state, candidate))
            {
                lastTrainingMoveScore = candidateScore;
                return candidate;
            }
        }
    }

    if (isMoveLegal(state, top[0].move))
    {
        lastTrainingMoveScore = top[0].score;
        return top[0].move;
    }

    lastTrainingMoveScore = evaluatePosition(state, side);
    return makeInvalidMoveLocal();
}
static int countNonKingPieces(const GameState *state, PieceColor side)
{
    int row, col;
    int count = 0;

    for (row = 0; row < BOARD_ROWS; row++)
    {
        for (col = 0; col < BOARD_COLS; col++)
        {
            ChessPiece piece = state->squares[row][col];

            if (piece.color == side &&
                piece.type != EMPTY &&
                piece.type != KING)
            {
                count++;
            }
        }
    }

    return count;
}

int main(int argc, char **argv) {
    const char *outputPath = "training_data.csv";
    int games = DEFAULT_GAMES;
    int maxPlies = DEFAULT_MAX_PLIES;
    int gameIdStart = 1;
    int ttMb = 0;
    unsigned int randomSeed;
    FILE *out;
    int gameIndex;

    if (argc >= 2) outputPath = argv[1];
    if (argc >= 3) games = atoi(argv[2]);
    if (argc >= 4) maxPlies = atoi(argv[3]);
    if (argc >= 5) selfplaySearchDepth = atoi(argv[4]);
    if (argc >= 6) selfplayTimeMs = atoi(argv[5]);
    if (argc >= 7) selfplayPreset = argv[6];
    randomSeed = (unsigned int)time(NULL) ^ ((unsigned int)getpid() * 2654435761u);
    if (argc >= 8) randomSeed = (unsigned int)strtoul(argv[7], NULL, 10);
    if (argc >= 9) gameIdStart = atoi(argv[8]);
    if (argc >= 10) ttMb = atoi(argv[9]);

    if (games <= 0) games = DEFAULT_GAMES;
    if (maxPlies <= 0 || maxPlies > MAX_GAME_MOVES) maxPlies = DEFAULT_MAX_PLIES;
    if (gameIdStart <= 0) gameIdStart = 1;
    if (selfplaySearchDepth <= 0) selfplaySearchDepth = DEFAULT_SELFPLAY_SEARCH_DEPTH;
    if (selfplayTimeMs <= 0) selfplayTimeMs = DEFAULT_SELFPLAY_TIME_MS;

    setCpuSearchTimeLimitMs(selfplayTimeMs);
    setCpuSearchPreset(selfplayPreset);
    if (ttMb > 0 && setCpuTranspositionTableSizeMb(ttMb) == 0u)
    {
        fprintf(stderr, "Warning: could not allocate %d MB transposition table.\n", ttMb);
    }

    srand(randomSeed);

    out = fopen(outputPath, "w");
    if (out == NULL) {
        printf("Could not open output file: %s\n", outputPath);
        return 1;
    }

    writeTrainingCsvHeader(out);

    fprintf(stderr,
            "Self-play search: depth=%d time=%d ms preset=%s exploration=%d%%\n",
            selfplaySearchDepth,
            selfplayTimeMs,
            getCpuSearchPresetName(),
            EXPLORATION_PERCENT);
    fprintf(stderr, "Self-play seed: %u\n", randomSeed);
    fprintf(stderr, "Self-play TT: %lu MB\n", getCpuTranspositionTableSizeMb());

    if (freopen("/dev/null", "w", stdout) == NULL)
    {
        fprintf(stderr, "Warning: could not silence stdout during self-play.\n");
    }

    for (gameIndex = 0; gameIndex < games; gameIndex++) {
        GameState state;
        TrainingSample samples[MAX_GAME_MOVES];
        unsigned long long positionHistory[RECENT_HISTORY_LIMIT];
        int historyCount = 0;
        int sampleCount = 0;
        int ply;
        PieceColor winner = NONE;
        int endedByTerminal = 0;
        int endedByRepetition = 0;
        int gameId = gameIdStart + gameIndex;
        Move previousMove = makeInvalidMoveLocal();

        initTrainingGameState(&state);
        positionHistory[historyCount++] = hashGameState(&state);

        for (ply = 0; ply < maxPlies; ply++) {
            PieceColor side = state.currentTurn;
            Move chosenMove;
            unsigned long long currentHash;
            int currentRepeatCount;

            winner = determineTerminalWinner(&state);
            if (winner != NONE) {
                endedByTerminal = 1;
                break;
            }

            currentHash = hashGameState(&state);
            currentRepeatCount = countHashInHistory(positionHistory, historyCount, currentHash);
            if (currentRepeatCount >= 3) {
                endedByRepetition = 1;
                winner = adjudicateWinnerByEval(&state);
                break;
            }

            chosenMove = chooseTrainingMove(&state, side, previousMove, positionHistory, historyCount);

if (chosenMove.start.row < 0)
{
    /*
        No legal/training-safe move was found.
        This could be stalemate, checkmate, or a move-filter issue.
        Do not automatically label it NONE.
    */

    if (isKingInCheck(&state, side))
    {
        winner = oppositeColorLocal(side);
    }
    else
    {
        winner = adjudicateWinnerByEval(&state);
    }

    endedByTerminal = 1;
    break;
}

/* HARD SAFETY CHECK:
   The data generator must never log or apply an illegal move. */
if (!isMoveLegal(&state, chosenMove)) {
    char badMove[MOVE_STRING_SIZE];
    char badBoard[BOARD_STRING_SIZE];

    moveToString(chosenMove, badMove, sizeof(badMove));
    serializeBoard(&state, badBoard, sizeof(badBoard));

    fprintf(stderr,
            "ERROR: selfplay selected illegal move at game %d ply %d: %s side=%s board=%s\n",
            gameId,
            ply,
            badMove,
            sideToString(side),
            badBoard);

    winner = NONE;
    endedByTerminal = 1;
    break;
}

            samples[sampleCount].gameId = gameId;
            samples[sampleCount].ply = ply;
            samples[sampleCount].stateBefore = state;
            samples[sampleCount].sideToMove = side;
            samples[sampleCount].chosenMove = chosenMove;
            samples[sampleCount].searchScore = lastTrainingMoveScore;
            sampleCount++;

            applyMove(&state, chosenMove);
            if (ply >= RESIGN_MIN_PLY)
{
    int whiteNonKing = countNonKingPieces(&state, WHITE);
    int blackNonKing = countNonKingPieces(&state, BLACK);

    if (whiteNonKing <= 1 && blackNonKing >= 4)
    {
        winner = BLACK;
        endedByTerminal = 1;
        break;
    }
    

    if (blackNonKing <= 1 && whiteNonKing >= 4)
    {
        winner = WHITE;
        endedByTerminal = 1;
        break;
    }
}
            previousMove = chosenMove;

            if (historyCount < RECENT_HISTORY_LIMIT) {
                positionHistory[historyCount++] = hashGameState(&state);
            } else {
                int h;
                for (h = 1; h < RECENT_HISTORY_LIMIT; h++) {
                    positionHistory[h - 1] = positionHistory[h];
                }
                positionHistory[RECENT_HISTORY_LIMIT - 1] = hashGameState(&state);
            }
        }

       if (!endedByRepetition) {
    PieceColor terminalWinner = determineTerminalWinner(&state);

    if (terminalWinner != NONE) {
        winner = terminalWinner;
    }
}

if (winner == NONE) {
    winner = adjudicateWinnerByEval(&state);
}

        for (ply = 0; ply < sampleCount; ply++) {
            int finalResult = resultForSide(winner, samples[ply].sideToMove);
            writeTrainingSampleCsv(out, &samples[ply], finalResult);
        }

        fprintf(stderr, "Game %d/%d complete: %d samples, winner=%s%s%s\n",
                gameIndex + 1,
                games,
                sampleCount,
                sideToString(winner),
                endedByTerminal ? "" : " (adjudicated by eval)",
                endedByRepetition ? " [repetition stopped]" : "");
    }

    fclose(out);
    fprintf(stderr, "Training data written to %s\n", outputPath);
    return 0;
}
