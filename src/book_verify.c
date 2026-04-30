#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "board.h"
#include "cpu.h"
#include "data_logger.h"
#include "eval.h"
#include "game.h"
#include "types.h"

#define MAX_CSV_LINE 4096
#define MAX_FIELDS 64
#define MAX_BOARD_TEXT 96
#define MAX_SOURCES_TEXT 1024

typedef struct
{
    PieceColor side;
    char board[MAX_BOARD_TEXT];
    char sourceMove[MOVE_STRING_SIZE];
    int count;
    double scorePct;
    double avgDepth;
    char sources[MAX_SOURCES_TEXT];
} Candidate;

typedef struct
{
    int side;
    int move;
    int count;
    int scorePct;
    int avgDepth;
    int board;
    int sources;
} CandidateColumns;

static int parseCsvLine(char *line, char **fields, int maxFields)
{
    int count = 0;
    char *cursor = line;

    while (count < maxFields)
    {
        fields[count++] = cursor;
        cursor = strchr(cursor, ',');
        if (cursor == NULL)
        {
            break;
        }
        *cursor = '\0';
        cursor++;
    }

    return count;
}

static PieceColor parseSide(const char *text)
{
    if (strcmp(text, "WHITE") == 0)
    {
        return WHITE;
    }
    if (strcmp(text, "BLACK") == 0)
    {
        return BLACK;
    }
    return NONE;
}

static PieceType charToPieceType(char ch)
{
    switch ((char)toupper((unsigned char)ch))
    {
        case 'P':
            return PAWN;
        case 'R':
            return ROOK;
        case 'K':
            return KING;
        case 'Q':
            return QUEEN;
        case 'N':
            return KNIGHT;
        case 'B':
            return BISHOP;
        case 'A':
            return ANTEATER;
        case '.':
        default:
            return EMPTY;
    }
}

static int isStartingSquare(PieceType type, PieceColor color, int row, int col)
{
    static const PieceType backRank[BOARD_COLS] = {
        ROOK, KNIGHT, BISHOP, ANTEATER, QUEEN,
        KING, ANTEATER, BISHOP, KNIGHT, ROOK
    };

    if (type == EMPTY || color == NONE)
    {
        return 0;
    }

    if (color == WHITE)
    {
        if (type == PAWN && row == 6)
        {
            return 1;
        }
        return row == 7 && col >= 0 && col < BOARD_COLS && backRank[col] == type;
    }

    if (type == PAWN && row == 1)
    {
        return 1;
    }
    return row == 0 && col >= 0 && col < BOARD_COLS && backRank[col] == type;
}

static int parseBoardString(const char *board, PieceColor side, GameState *state)
{
    int row;
    int col;
    int index = 0;

    clearGameState(state);
    state->currentTurn = side;
    state->enPassantTarget.row = -1;
    state->enPassantTarget.col = -1;

    for (row = 0; row < BOARD_ROWS; row++)
    {
        for (col = 0; col < BOARD_COLS; col++)
        {
            char ch = board[index++];
            PieceType type;
            PieceColor color;
            ChessPiece piece;

            if (ch == '\0' || ch == '/')
            {
                return 0;
            }

            type = charToPieceType(ch);
            color = NONE;
            if (type != EMPTY)
            {
                color = islower((unsigned char)ch) ? BLACK : WHITE;
            }

            piece = makePiece(type, color);
            piece.MoveCount = isStartingSquare(type, color, row, col) ? 0 : 1;
            state->squares[row][col] = piece;

            if (type == KING && color == WHITE)
            {
                state->whiteKingPos.row = row;
                state->whiteKingPos.col = col;
            }
            else if (type == KING && color == BLACK)
            {
                state->blackKingPos.row = row;
                state->blackKingPos.col = col;
            }
        }

        if (row != BOARD_ROWS - 1)
        {
            if (board[index++] != '/')
            {
                return 0;
            }
        }
    }

    if (board[index] != '\0')
    {
        return 0;
    }

    state->whiteCastleKingSide =
        state->whiteKingPos.row == 7 &&
        state->whiteKingPos.col == 5 &&
        state->squares[7][9].type == ROOK &&
        state->squares[7][9].color == WHITE &&
        state->squares[7][5].MoveCount == 0 &&
        state->squares[7][9].MoveCount == 0;
    state->whiteCastleQueenSide =
        state->whiteKingPos.row == 7 &&
        state->whiteKingPos.col == 5 &&
        state->squares[7][0].type == ROOK &&
        state->squares[7][0].color == WHITE &&
        state->squares[7][5].MoveCount == 0 &&
        state->squares[7][0].MoveCount == 0;
    state->blackCastleKingSide =
        state->blackKingPos.row == 0 &&
        state->blackKingPos.col == 5 &&
        state->squares[0][9].type == ROOK &&
        state->squares[0][9].color == BLACK &&
        state->squares[0][5].MoveCount == 0 &&
        state->squares[0][9].MoveCount == 0;
    state->blackCastleQueenSide =
        state->blackKingPos.row == 0 &&
        state->blackKingPos.col == 5 &&
        state->squares[0][0].type == ROOK &&
        state->squares[0][0].color == BLACK &&
        state->squares[0][5].MoveCount == 0 &&
        state->squares[0][0].MoveCount == 0;

    return state->whiteKingPos.row >= 0 && state->blackKingPos.row >= 0;
}

static int findColumn(char **fields, int count, const char *name)
{
    int i;

    for (i = 0; i < count; i++)
    {
        if (strcmp(fields[i], name) == 0)
        {
            return i;
        }
    }

    return -1;
}

static int loadColumns(char *header, CandidateColumns *columns)
{
    char *fields[MAX_FIELDS];
    int count = parseCsvLine(header, fields, MAX_FIELDS);

    columns->side = findColumn(fields, count, "side");
    columns->move = findColumn(fields, count, "move");
    columns->count = findColumn(fields, count, "count");
    columns->scorePct = findColumn(fields, count, "score_pct");
    columns->avgDepth = findColumn(fields, count, "avg_depth");
    columns->board = findColumn(fields, count, "board_before");
    columns->sources = findColumn(fields, count, "sources");

    return columns->side >= 0 &&
           columns->move >= 0 &&
           columns->count >= 0 &&
           columns->scorePct >= 0 &&
           columns->avgDepth >= 0 &&
           columns->board >= 0;
}

static int candidateAlreadySeen(const Candidate *candidates,
                                int count,
                                PieceColor side,
                                const char *board)
{
    int i;

    for (i = 0; i < count; i++)
    {
        if (candidates[i].side == side &&
            strcmp(candidates[i].board, board) == 0)
        {
            return 1;
        }
    }

    return 0;
}

static int readCandidates(const char *path,
                          Candidate *candidates,
                          int maxCandidates,
                          int minCount,
                          double minScorePct,
                          double minAvgDepth)
{
    FILE *in = fopen(path, "r");
    char line[MAX_CSV_LINE];
    CandidateColumns columns;
    int total = 0;

    if (in == NULL)
    {
        fprintf(stderr, "ERROR: could not open %s\n", path);
        return -1;
    }

    if (fgets(line, sizeof(line), in) == NULL)
    {
        fclose(in);
        return 0;
    }
    line[strcspn(line, "\r\n")] = '\0';

    if (!loadColumns(line, &columns))
    {
        fprintf(stderr, "ERROR: missing required columns in %s\n", path);
        fclose(in);
        return -1;
    }

    while (total < maxCandidates && fgets(line, sizeof(line), in) != NULL)
    {
        char *fields[MAX_FIELDS];
        int fieldCount;
        PieceColor side;
        int count;
        double scorePct;
        double avgDepth;
        Candidate *candidate;

        line[strcspn(line, "\r\n")] = '\0';
        fieldCount = parseCsvLine(line, fields, MAX_FIELDS);
        if (fieldCount <= columns.board ||
            fieldCount <= columns.side ||
            fieldCount <= columns.move)
        {
            continue;
        }

        side = parseSide(fields[columns.side]);
        count = atoi(fields[columns.count]);
        scorePct = atof(fields[columns.scorePct]);
        avgDepth = atof(fields[columns.avgDepth]);

        if (side == NONE ||
            count < minCount ||
            scorePct < minScorePct ||
            avgDepth < minAvgDepth ||
            candidateAlreadySeen(candidates, total, side, fields[columns.board]))
        {
            continue;
        }

        candidate = &candidates[total++];
        candidate->side = side;
        snprintf(candidate->board, sizeof(candidate->board), "%s", fields[columns.board]);
        snprintf(candidate->sourceMove, sizeof(candidate->sourceMove), "%s", fields[columns.move]);
        candidate->count = count;
        candidate->scorePct = scorePct;
        candidate->avgDepth = avgDepth;
        candidate->sources[0] = '\0';
        if (columns.sources >= 0 && fieldCount > columns.sources)
        {
            snprintf(candidate->sources, sizeof(candidate->sources), "%s", fields[columns.sources]);
        }
    }

    fclose(in);
    return total;
}

static void usage(const char *program)
{
    fprintf(stderr,
            "Usage: %s input.csv output.csv [max_positions] [depth] [time_ms] [preset] [tt_mb] [min_count] [min_score_pct] [min_avg_depth]\n",
            program);
}

int main(int argc, char **argv)
{
    const char *inputPath = argc > 1 ? argv[1] : "data/opening_book_final_candidates.csv";
    const char *outputPath = argc > 2 ? argv[2] : "data/opening_book_verified.csv";
    int maxPositions = argc > 3 ? atoi(argv[3]) : 128;
    int depth = argc > 4 ? atoi(argv[4]) : 10;
    int timeMs = argc > 5 ? atoi(argv[5]) : 2500;
    const char *preset = argc > 6 ? argv[6] : "storm";
    int ttMb = argc > 7 ? atoi(argv[7]) : 1024;
    int minCount = argc > 8 ? atoi(argv[8]) : 4;
    double minScorePct = argc > 9 ? atof(argv[9]) : 85.0;
    double minAvgDepth = argc > 10 ? atof(argv[10]) : 5.0;
    Candidate *candidates;
    FILE *out;
    int candidateCount;
    int i;
    int verified = 0;

    if (argc > 1 && strcmp(argv[1], "--help") == 0)
    {
        usage(argv[0]);
        return 0;
    }

    if (maxPositions <= 0)
    {
        maxPositions = 128;
    }
    if (depth <= 0)
    {
        depth = 10;
    }
    if (timeMs <= 0)
    {
        timeMs = 2500;
    }

    candidates = (Candidate *)calloc((size_t)maxPositions, sizeof(*candidates));
    if (candidates == NULL)
    {
        fprintf(stderr, "ERROR: out of memory\n");
        return 1;
    }

    candidateCount = readCandidates(inputPath,
                                    candidates,
                                    maxPositions,
                                    minCount,
                                    minScorePct,
                                    minAvgDepth);
    if (candidateCount < 0)
    {
        free(candidates);
        return 1;
    }

    out = fopen(outputPath, "w");
    if (out == NULL)
    {
        fprintf(stderr, "ERROR: could not open %s\n", outputPath);
        free(candidates);
        return 1;
    }

    setCpuSearchPreset(preset);
    setCpuSearchTimeLimitMs(timeMs);
    setCpuOpeningBookEnabled(0);
    setLearnedEvalEnabled(1);
    setPstScalePercent(100);
    if (ttMb > 0)
    {
        setCpuTranspositionTableSizeMb(ttMb);
    }

    fprintf(out,
            "side,move,count,score_pct,wins,draws,losses,avg_eval_delta_pst100,avg_depth,avg_time_ms,board_before,sources,source_move,verify_score,verify_depth,verify_time_ms,verify_nodes\n");

    printf("Verifying %d positions with preset=%s depth=%d time=%dms book=off\n",
           candidateCount,
           getCpuSearchPresetName(),
           depth,
           timeMs);

    for (i = 0; i < candidateCount; i++)
    {
        GameState state;
        Move bestMove;
        CpuSearchStats stats;
        char bestMoveText[MOVE_STRING_SIZE];
        Candidate *candidate = &candidates[i];

        if (!parseBoardString(candidate->board, candidate->side, &state))
        {
            fprintf(stderr, "Skipping unparsable board at candidate %d\n", i + 1);
            continue;
        }

        bestMove = chooseBestMoveDepth(&state, candidate->side, depth);
        if (bestMove.start.row < 0)
        {
            continue;
        }

        getLastCpuSearchStats(&stats);
        moveToString(bestMove, bestMoveText, sizeof(bestMoveText));

        fprintf(out,
                "%s,%s,%d,%.2f,0,0,0,0,%.2f,%.2f,%s,%s,%s,%d,%d,%.2f,%lu\n",
                candidate->side == WHITE ? "WHITE" : "BLACK",
                bestMoveText,
                candidate->count,
                candidate->scorePct,
                (double)stats.completedDepth,
                stats.timeMs,
                candidate->board,
                candidate->sources,
                candidate->sourceMove,
                stats.score,
                stats.completedDepth,
                stats.timeMs,
                stats.nodes);

        verified++;
        printf("%3d/%3d %s %s score=%d depth=%d time=%.0fms nodes=%lu\n",
               i + 1,
               candidateCount,
               candidate->side == WHITE ? "WHITE" : "BLACK",
               bestMoveText,
               stats.score,
               stats.completedDepth,
               stats.timeMs,
               stats.nodes);
        fflush(stdout);
        fflush(out);
    }

    fclose(out);
    free(candidates);

    printf("Wrote %d verified entries to %s\n", verified, outputPath);
    return 0;
}
