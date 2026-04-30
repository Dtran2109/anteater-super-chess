#include "gui.h"
#include "board.h"
#include "game.h"
#include "cpu.h"
#include "eval.h"
#include "moves.h"
#include <SDL2/SDL_image.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define GUI_MOVE_HISTORY_MAX 1024
#define GUI_POSITION_KEY_SIZE 128
#define GUI_REPETITION_LIMIT 3
#define GUI_REPEAT_POSITION_PENALTY 100000
#define GUI_IMMEDIATE_UNDO_PENALTY 3000
#define GUI_CAPTURE_PROGRESS_BONUS 500
#define GUI_PAWN_PROGRESS_BONUS 80
#define GUI_DEBUG_BOARD_SIZE (BOARD_ROWS * BOARD_COLS + BOARD_ROWS + 8)
#define GUI_DEBUG_MOVE_SIZE 12

static FILE *guiDebugLog = NULL;
static int guiDebugPly = 0;

const char *pieceToSymbolConsole(ChessPiece piece) {
    if (piece.type == EMPTY) return " .";
    static char text[3];
    text[0] = (piece.color == WHITE) ? 'w' : 'b';
    switch (piece.type) {
        case PAWN: text[1] = 'P'; break;
        case ROOK: text[1] = 'R'; break;
        case KNIGHT: text[1] = 'N'; break;
        case BISHOP: text[1] = 'B'; break;
        case QUEEN: text[1] = 'Q'; break;
        case KING: text[1] = 'K'; break;
        case ANTEATER: text[1] = 'A'; break;
        default: text[1] = '?'; break;
    }
    text[2] = '\0';
    return text;
}

void printConsoleBoard(const GameState *state) {
    printf("\n     ");
    for (int c = 0; c < BOARD_COLS; ++c) printf("  %c ", 'A' + c);
    printf("\n   +");
    for (int c = 0; c < BOARD_COLS; ++c) printf("---+");
    printf("\n");

    for (int r = 0; r < BOARD_ROWS; ++r) {
        printf(" %d |", BOARD_ROWS - r);
        for (int c = 0; c < BOARD_COLS; ++c) printf("%s |", pieceToSymbolConsole(state->squares[r][c]));
        printf(" %d\n   +", BOARD_ROWS - r);
        for (int c = 0; c < BOARD_COLS; ++c) printf("---+");
        printf("\n");
    }
    printf("     ");
    for (int c = 0; c < BOARD_COLS; ++c) printf("%c   ", 'A' + c);
    printf("\nTurn: %s\n", state->currentTurn == WHITE ? "White" : "Black");
}

int parsePosition(const char *text, Position *pos) {
    if (!text || !pos || strlen(text) < 2) return 0;
    char file = (char)toupper((unsigned char)text[0]);
    char rank = text[1];
    if (file < 'A' || file >= 'A' + BOARD_COLS || rank < '1' || rank > '8') return 0;
    pos->col = file - 'A';
    pos->row = BOARD_ROWS - (rank - '0');
    return 1;
}

void formatPosition(Position pos, char out[3]) {
    out[0] = (char)('A' + pos.col);
    out[1] = (char)('0' + (BOARD_ROWS - pos.row));
    out[2] = '\0';
}

static PieceType parsePromotion(const char *s) {
    if (!s || !s[0]) return EMPTY;
    switch (toupper((unsigned char)s[0])) {
        case 'Q': return QUEEN;
        case 'R': return ROOK;
        case 'B': return BISHOP;
        case 'N': return KNIGHT;
        case 'A': return ANTEATER;
        default: return EMPTY;
    }
}

void playConsoleGame(GameState *state, int useCpu, PieceColor playerColor) {
    char src[16], dst[16], promo[16];
    printf("\nAnteater Chess Console\n");
    printf("Move format: E2 E4. Promotion optional: E7 E8 Q. Type quit to exit.\n");

    while (getResultMessage(state) == NULL) {
        printConsoleBoard(state);

        if (useCpu && state->currentTurn != playerColor) {
            Move ai = chooseBestMove(state, state->currentTurn);
            if (ai.start.row < 0) break;
            char a[3], b[3];
            formatPosition(ai.start, a);
            formatPosition(ai.dest, b);
            printf("CPU plays %s %s\n", a, b);
            applyMove(state, ai);
            continue;
        }

        printf("%s move > ", state->currentTurn == WHITE ? "White" : "Black");
        int n = scanf("%15s", src);
        if (n != 1) break;
        if (strcmp(src, "quit") == 0 || strcmp(src, "exit") == 0) break;
        if (scanf("%15s", dst) != 1) break;
        promo[0] = '\0';
        int ch;
        while ((ch = getchar()) == ' ' || ch == '\t') { }
        if (ch != '\n' && ch != EOF) {
            ungetc(ch, stdin);
            if (scanf("%15s", promo) != 1) {
                promo[0] = '\0';
            }
            while ((ch = getchar()) != '\n' && ch != EOF) { }
        }

        Move move;
        if (!parsePosition(src, &move.start) || !parsePosition(dst, &move.dest)) {
            printf("Invalid square. Use A1 through J8.\n");
            continue;
        }
        move.promotionGoal = parsePromotion(promo);
        if (!isMoveLegal(state, move)) {
            printf("Illegal move.\n");
            continue;
        }
        applyMove(state, move);
    }

    printConsoleBoard(state);
    const char *result = getResultMessage(state);
    if (result) printf("%s\n", result);
}

int isHumanTurn(const GameState *state, GameMode mode, PieceColor playerColor) {
    if (!state) return 0;
    if (mode == H_VS_H_MODE) return 1;
    if (mode == H_VS_C_MODE) return state->currentTurn == playerColor;
    return 0;
}

int isCpuTurn(const GameState *state, GameMode mode, PieceColor playerColor) {
    if (!state) return 0;
    if (mode == C_VS_C_MODE) return 1;
    if (mode == H_VS_C_MODE) return state->currentTurn != playerColor;
    return 0;
}

int makeMoveFromClick(Move *move, int fromRow, int fromCol, int toRow, int toCol) {
    if (!move) return 0;
    move->start = (Position){fromRow, fromCol};
    move->dest = (Position){toRow, toCol};
    move->promotionGoal = EMPTY;
    return isPositionInBounds(move->start) && isPositionInBounds(move->dest);
}
static const char *pieceToUnicode(ChessPiece piece) {
    if (piece.color == WHITE) {
        switch (piece.type) {
            case KING: return "♚"; case QUEEN: return "♛"; case ROOK: return "♜";
        case BISHOP: return "♝"; case KNIGHT: return "♞"; case PAWN: return "♟";
        case ANTEATER: return "◆"; default: return NULL;
        }
    }
    switch (piece.type) {
        case KING: return "♚"; case QUEEN: return "♛"; case ROOK: return "♜";
        case BISHOP: return "♝"; case KNIGHT: return "♞"; case PAWN: return "♟";
        case ANTEATER: return "◆"; default: return NULL;
    }
}

static void drawText(SDL_Renderer *renderer, TTF_Font *font, const char *text, int x, int y, SDL_Color color) {
    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return;
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        SDL_Rect dst = {x, y, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture, NULL, &dst);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}


static int pointInRect(int x, int y, SDL_Rect rect) {
    return x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
}

static char guiPieceChar(ChessPiece piece) {
    char ch = '.';

    switch (piece.type) {
        case PAWN: ch = 'P'; break;
        case ROOK: ch = 'R'; break;
        case KING: ch = 'K'; break;
        case QUEEN: ch = 'Q'; break;
        case KNIGHT: ch = 'N'; break;
        case BISHOP: ch = 'B'; break;
        case ANTEATER: ch = 'A'; break;
        case EMPTY:
        default: ch = '.'; break;
    }

    if (piece.color == BLACK && ch >= 'A' && ch <= 'Z') {
        ch = (char)(ch - 'A' + 'a');
    }

    return ch;
}

static void buildGuiPositionKey(const GameState *state, char out[GUI_POSITION_KEY_SIZE]) {
    int index = 0;

    if (!state || !out) return;

    for (int r = 0; r < BOARD_ROWS; ++r) {
        for (int c = 0; c < BOARD_COLS; ++c) {
            if (index + 1 >= GUI_POSITION_KEY_SIZE) {
                out[GUI_POSITION_KEY_SIZE - 1] = '\0';
                return;
            }
            out[index++] = guiPieceChar(state->squares[r][c]);
        }
        if (r != BOARD_ROWS - 1) {
            out[index++] = '/';
        }
    }

    if (index + 3 < GUI_POSITION_KEY_SIZE) {
        out[index++] = ' ';
        out[index++] = state->currentTurn == WHITE ? 'W' : 'B';
    }
    out[index] = '\0';
}

static const char *guiSideName(PieceColor side) {
    if (side == WHITE) return "WHITE";
    if (side == BLACK) return "BLACK";
    return "NONE";
}

static void guiMoveToString(Move move, char *buffer, int bufferSize) {
    char startFile;
    char startRank;
    char destFile;
    char destRank;

    if (!buffer || bufferSize <= 0) return;
    buffer[0] = '\0';

    if (bufferSize < GUI_DEBUG_MOVE_SIZE) return;

    if (move.start.row < 0 || move.start.col < 0 ||
        move.dest.row < 0 || move.dest.col < 0) {
        snprintf(buffer, bufferSize, "NONE");
        return;
    }

    startFile = (char)('A' + move.start.col);
    startRank = (char)('1' + (BOARD_ROWS - 1 - move.start.row));
    destFile = (char)('A' + move.dest.col);
    destRank = (char)('1' + (BOARD_ROWS - 1 - move.dest.row));

    if (move.promotionGoal != EMPTY) {
        char promo = guiPieceChar((ChessPiece){move.promotionGoal, WHITE, 0});
        snprintf(buffer, bufferSize, "%c%c%c%c%c", startFile, startRank, destFile, destRank, promo);
    } else {
        snprintf(buffer, bufferSize, "%c%c%c%c", startFile, startRank, destFile, destRank);
    }
}

static void serializeGuiBoard(const GameState *state, char *buffer, int bufferSize) {
    int index = 0;

    if (!buffer || bufferSize <= 0) return;
    buffer[0] = '\0';
    if (!state) return;

    for (int row = 0; row < BOARD_ROWS; ++row) {
        for (int col = 0; col < BOARD_COLS; ++col) {
            if (index + 1 >= bufferSize) {
                buffer[bufferSize - 1] = '\0';
                return;
            }
            buffer[index++] = guiPieceChar(state->squares[row][col]);
        }

        if (row != BOARD_ROWS - 1) {
            if (index + 1 >= bufferSize) {
                buffer[bufferSize - 1] = '\0';
                return;
            }
            buffer[index++] = '/';
        }
    }

    buffer[index] = '\0';
}

static int evalWhiteWithGuiPstScale(const GameState *state, int pstScale) {
    int oldScale = getPstScalePercent();
    int result;

    setPstScalePercent(pstScale);
    result = evaluatePosition(state, WHITE);
    setPstScalePercent(oldScale);

    return result;
}

int setGuiDebugLogPath(const char *path) {
    if (!path || path[0] == '\0') {
        return 0;
    }

    closeGuiDebugLog();
    guiDebugLog = fopen(path, "w");
    if (!guiDebugLog) {
        return 0;
    }

    guiDebugPly = 0;
    fprintf(guiDebugLog,
            "ply,side_to_move,actor,selected_by,move,search_depth,search_requested_depth,search_score,search_nodes,search_total_nodes,search_time_ms,search_avg_time_ms,search_nps,search_tt_hits,search_tt_cutoffs,search_total_tt_hits,search_total_tt_cutoffs,board_before,eval_before_pst0,eval_before_pst50,eval_before_pst100,eval_after_pst0,eval_after_pst50,eval_after_pst100,board_after,next_turn\n");
    fflush(guiDebugLog);

    return 1;
}

void closeGuiDebugLog(void) {
    if (guiDebugLog) {
        fclose(guiDebugLog);
        guiDebugLog = NULL;
    }
}

static void logGuiAppliedMove(const GameState *before,
                              const GameState *after,
                              Move move,
                              const char *actor,
                              const char *selectedBy,
                              const CpuSearchStats *stats) {
    CpuSearchStats emptyStats;
    char boardBefore[GUI_DEBUG_BOARD_SIZE];
    char boardAfter[GUI_DEBUG_BOARD_SIZE];
    char moveString[GUI_DEBUG_MOVE_SIZE];
    int evalBeforePst0;
    int evalBeforePst50;
    int evalBeforePst100;
    int evalAfterPst0;
    int evalAfterPst50;
    int evalAfterPst100;

    if (!guiDebugLog || !before || !after) return;

    if (!stats) {
        memset(&emptyStats, 0, sizeof(emptyStats));
        stats = &emptyStats;
    }

    serializeGuiBoard(before, boardBefore, sizeof(boardBefore));
    serializeGuiBoard(after, boardAfter, sizeof(boardAfter));
    guiMoveToString(move, moveString, sizeof(moveString));

    evalBeforePst0 = evalWhiteWithGuiPstScale(before, 0);
    evalBeforePst50 = evalWhiteWithGuiPstScale(before, 50);
    evalBeforePst100 = evalWhiteWithGuiPstScale(before, 100);
    evalAfterPst0 = evalWhiteWithGuiPstScale(after, 0);
    evalAfterPst50 = evalWhiteWithGuiPstScale(after, 50);
    evalAfterPst100 = evalWhiteWithGuiPstScale(after, 100);

    fprintf(guiDebugLog,
            "%d,%s,%s,%s,%s,%d,%d,%d,%lu,%llu,%.2f,%.2f,%lu,%lu,%lu,%llu,%llu,%s,%d,%d,%d,%d,%d,%d,%s,%s\n",
            guiDebugPly,
            guiSideName(before->currentTurn),
            actor ? actor : "UNKNOWN",
            selectedBy ? selectedBy : "manual",
            moveString,
            stats->completedDepth,
            stats->requestedDepth,
            stats->score,
            stats->nodes,
            stats->totalNodes,
            stats->timeMs,
            stats->averageTimeMs,
            stats->nps,
            stats->ttHits,
            stats->ttCutoffs,
            stats->totalTtHits,
            stats->totalTtCutoffs,
            boardBefore,
            evalBeforePst0,
            evalBeforePst50,
            evalBeforePst100,
            evalAfterPst0,
            evalAfterPst50,
            evalAfterPst100,
            boardAfter,
            guiSideName(after->currentTurn));
    fflush(guiDebugLog);
    guiDebugPly++;
}

static void pushGuiPosition(const GameState *state,
                            char history[GUI_MOVE_HISTORY_MAX + 1][GUI_POSITION_KEY_SIZE],
                            int *historyCount) {
    if (!state || !history || !historyCount) return;

    if (*historyCount >= GUI_MOVE_HISTORY_MAX + 1) {
        memmove(history,
                history + 1,
                GUI_MOVE_HISTORY_MAX * sizeof(history[0]));
        *historyCount = GUI_MOVE_HISTORY_MAX;
    }

    buildGuiPositionKey(state, history[*historyCount]);
    (*historyCount)++;
}

static int currentGuiPositionRepeatCount(char history[GUI_MOVE_HISTORY_MAX + 1][GUI_POSITION_KEY_SIZE],
                                         int historyCount) {
    int count = 0;

    if (!history || historyCount <= 0) return 0;

    for (int i = 0; i < historyCount; ++i) {
        if (strcmp(history[i], history[historyCount - 1]) == 0) {
            count++;
        }
    }

    return count;
}

static int countGuiPositionKey(char history[GUI_MOVE_HISTORY_MAX + 1][GUI_POSITION_KEY_SIZE],
                               int historyCount,
                               const char *key) {
    int count = 0;

    if (!history || !key) return 0;

    for (int i = 0; i < historyCount; ++i) {
        if (strcmp(history[i], key) == 0) {
            count++;
        }
    }

    return count;
}

static int guiMovesAreReverse(Move previous, Move current) {
    return previous.start.row == current.dest.row &&
           previous.start.col == current.dest.col &&
           previous.dest.row == current.start.row &&
           previous.dest.col == current.start.col;
}

static int moveLooksValid(Move move) {
    return move.start.row >= 0 &&
           move.start.col >= 0 &&
           move.dest.row >= 0 &&
           move.dest.col >= 0;
}

static int repeatCountAfterMove(const GameState *state,
                                Move move,
                                char history[GUI_MOVE_HISTORY_MAX + 1][GUI_POSITION_KEY_SIZE],
                                int historyCount) {
    GameState copy;
    char key[GUI_POSITION_KEY_SIZE];

    if (!state || !moveLooksValid(move)) return GUI_REPETITION_LIMIT;

    copy = *state;
    applyMove(&copy, move);
    buildGuiPositionKey(&copy, key);
    return countGuiPositionKey(history, historyCount, key);
}

static int materialOfSideGui(const GameState *state, PieceColor side) {
    int total = 0;

    if (!state) return 0;

    for (int row = 0; row < BOARD_ROWS; ++row) {
        for (int col = 0; col < BOARD_COLS; ++col) {
            ChessPiece piece = state->squares[row][col];

            if (piece.type != EMPTY && piece.color == side) {
                total += getPieceValue(piece.type);
            }
        }
    }

    return total;
}

static int actualCaptureGainFromMoveGui(const GameState *state, Move move, PieceColor side) {
    GameState copy;
    PieceColor enemy;
    int before;
    int after;

    if (!state || !moveLooksValid(move)) return 0;

    copy = *state;
    enemy = oppositeColor(side);

    before = materialOfSideGui(state, enemy);
    applyMove(&copy, move);
    after = materialOfSideGui(&copy, enemy);

    return before - after;
}

static int pushGuiMove(GameState *state, Move move, MoveRecord *history, int *historyCount) {
    if (!state || !history || !historyCount || *historyCount >= GUI_MOVE_HISTORY_MAX) return 0;
    applyMoveRecord(state, move, &history[*historyCount]);
    (*historyCount)++;
    return 1;
}

static int canUndoGuiMove(GameMode mode, PieceColor playerColor, const GameState *state, int historyCount) {
    if (historyCount <= 0) return 0;
    if (mode == H_VS_C_MODE) {
        return state && state->currentTurn == playerColor && historyCount >= 2;
    }
    return 1;
}

static void undoGuiMove(GameState *state, GameMode mode, PieceColor playerColor, MoveRecord *history, int *historyCount) {
    if (!state || !history || !historyCount || !canUndoGuiMove(mode, playerColor, state, *historyCount)) return;

    undoMove(state, &history[*historyCount - 1]);
    (*historyCount)--;

    if (mode == H_VS_C_MODE && *historyCount > 0 && state->currentTurn != playerColor) {
        undoMove(state, &history[*historyCount - 1]);
        (*historyCount)--;
    }
}

static void trimGuiPositionHistoryAfterUndo(const GameState *state,
                                            char history[GUI_MOVE_HISTORY_MAX + 1][GUI_POSITION_KEY_SIZE],
                                            int *historyCount,
                                            int moveHistoryCount) {
    int desiredCount;

    if (!state || !history || !historyCount) return;

    desiredCount = moveHistoryCount + 1;
    if (desiredCount < 1) desiredCount = 1;
    if (desiredCount > GUI_MOVE_HISTORY_MAX + 1) desiredCount = GUI_MOVE_HISTORY_MAX + 1;

    *historyCount = desiredCount;
    buildGuiPositionKey(state, history[*historyCount - 1]);
}

static const char *guiResultMessage(const GameState *state,
                                    char history[GUI_MOVE_HISTORY_MAX + 1][GUI_POSITION_KEY_SIZE],
                                    int historyCount) {
    const char *result = getResultMessage(state);

    if (result) return result;

    if (currentGuiPositionRepeatCount(history, historyCount) >= GUI_REPETITION_LIMIT) {
        return "Repetition. Draw.";
    }

    return NULL;
}

static Move previousGuiMove(const MoveRecord *history, int historyCount) {
    if (!history || historyCount <= 0) {
        Move invalid = {{-1, -1}, {-1, -1}, EMPTY};
        return invalid;
    }

    return history[historyCount - 1].move;
}

static Move chooseGuiCpuMove(const GameState *state,
                             PieceColor side,
                             int depth,
                             char positionHistory[GUI_MOVE_HISTORY_MAX + 1][GUI_POSITION_KEY_SIZE],
                             int positionHistoryCount,
                             const MoveRecord *moveHistory,
                             int moveHistoryCount,
                             int *usedFallback) {
    Move candidate;
    Move previous = previousGuiMove(moveHistory, moveHistoryCount);
    MoveList legalMoves;
    Move bestFallback;
    int bestScore = -1000000000;

    if (usedFallback) {
        *usedFallback = 0;
    }

    candidate = chooseBestMoveDepth(state, side, depth);
    if (moveLooksValid(candidate) && isMoveLegal(state, candidate)) {
        int repeatCount = repeatCountAfterMove(state,
                                               candidate,
                                               positionHistory,
                                               positionHistoryCount);
        if (repeatCount == 0 &&
            !(moveLooksValid(previous) && guiMovesAreReverse(previous, candidate))) {
            return candidate;
        }
    }

    bestFallback.start.row = -1;
    bestFallback.start.col = -1;
    bestFallback.dest.row = -1;
    bestFallback.dest.col = -1;
    bestFallback.promotionGoal = EMPTY;

    generateAllLegalMoves(state, side, &legalMoves);
    for (int i = 0; i < legalMoves.count; ++i) {
        Move move = legalMoves.moves[i];
        GameState copy = *state;
        ChessPiece moving;
        ChessPiece target;
        int score;
        int repeatCount;

        if (!isMoveLegal(state, move)) {
            continue;
        }

        repeatCount = repeatCountAfterMove(state,
                                           move,
                                           positionHistory,
                                           positionHistoryCount);

        applyMove(&copy, move);
        score = evaluatePosition(&copy, side);

        if (repeatCount > 0) {
            score -= repeatCount * GUI_REPEAT_POSITION_PENALTY;
        }

        if (moveLooksValid(previous) && guiMovesAreReverse(previous, move)) {
            score -= GUI_IMMEDIATE_UNDO_PENALTY;
        }

        moving = state->squares[move.start.row][move.start.col];
        target = state->squares[move.dest.row][move.dest.col];
        (void)target;
        {
            int captureGain = actualCaptureGainFromMoveGui(state, move, side);
            if (captureGain > 0) {
                score += captureGain + GUI_CAPTURE_PROGRESS_BONUS;
            }
        }

        if (moving.type == PAWN) {
            if ((moving.color == WHITE && move.dest.row < move.start.row) ||
                (moving.color == BLACK && move.dest.row > move.start.row)) {
                score += GUI_PAWN_PROGRESS_BONUS;
            }
        }

        if (score > bestScore) {
            bestScore = score;
            bestFallback = move;
        }
    }

    if (moveLooksValid(bestFallback)) {
        if (usedFallback) {
            *usedFallback = 1;
        }
        return bestFallback;
    }

    return candidate;
}

static int moveListContainsDest(const MoveList *moves, int row, int col) {
    for (int i = 0; i < moves->count; ++i) {
        if (moves->moves[i].dest.row == row && moves->moves[i].dest.col == col) return 1;
    }
    return 0;
}

static int getMoveFromListByDest(const MoveList *moves, int row, int col, Move *outMove) {
    int found = 0;

    if (!moves || !outMove) return 0;

    for (int i = 0; i < moves->count; ++i) {
        Move move = moves->moves[i];

        if (move.dest.row != row || move.dest.col != col) {
            continue;
        }

        if (!found || move.promotionGoal == QUEEN) {
            *outMove = move;
            found = 1;
        }

        if (move.promotionGoal == QUEEN) {
            return 1;
        }
    }

    return found;
}

static void buildSelectedMoveList(const GameState *state, int selectedRow, int selectedCol, MoveList *selectedMoves) {
    MoveList allMoves;
    clearMoveList(selectedMoves);
    if (selectedRow < 0 || selectedCol < 0) return;

    generateAllLegalMoves(state, state->currentTurn, &allMoves);
    for (int i = 0; i < allMoves.count; ++i) {
        Move move = allMoves.moves[i];
        if (move.start.row == selectedRow && move.start.col == selectedCol) {
            addMove(selectedMoves, move.start, move.dest, move.promotionGoal);
        }
    }
}

void drawBoardGrid(SDL_Renderer *renderer, int selectedRow, int selectedCol) {
    for (int r = 0; r < BOARD_ROWS; ++r) {
        for (int c = 0; c < BOARD_COLS; ++c) {
            SDL_Rect square = { BOARD_OFFSET_X + c * CELL_SIZE, BOARD_OFFSET_Y + r * CELL_SIZE, CELL_SIZE, CELL_SIZE };
            if ((r + c) % 2 == 0) SDL_SetRenderDrawColor(renderer, 240, 217, 181, 255);
            else SDL_SetRenderDrawColor(renderer, 181, 136, 99, 255);
            SDL_RenderFillRect(renderer, &square);
            if (r == selectedRow && c == selectedCol) {
                SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255);
                SDL_RenderDrawRect(renderer, &square);
                square.x += 1; square.y += 1; square.w -= 2; square.h -= 2;
                SDL_RenderDrawRect(renderer, &square);
            }
        }
    }
}

void drawMoveHighlights(SDL_Renderer *renderer, const GameState *state, const MoveList *moves) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    for (int i = 0; i < moves->count; ++i) {
        Move move = moves->moves[i];
        ChessPiece target = state->squares[move.dest.row][move.dest.col];

        SDL_Rect mark = {
            BOARD_OFFSET_X + move.dest.col * CELL_SIZE + 8,
            BOARD_OFFSET_Y + move.dest.row * CELL_SIZE + 8,
            CELL_SIZE - 16,
            CELL_SIZE - 16
        };

        if (target.type == EMPTY) {
            SDL_SetRenderDrawColor(renderer, 40, 190, 80, 110);
        } else {
            SDL_SetRenderDrawColor(renderer, 220, 60, 60, 130);
        }

        SDL_RenderFillRect(renderer, &mark);
        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 150);
        SDL_RenderDrawRect(renderer, &mark);
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

void drawBoardLabels(SDL_Renderer *renderer, TTF_Font *labelFont) {
    SDL_Color color = {230, 230, 230, 255};
    char text[3];
    for (int c = 0; c < BOARD_COLS; ++c) {
        text[0] = (char)('A' + c); text[1] = '\0';
        drawText(renderer, labelFont, text, BOARD_OFFSET_X + c * CELL_SIZE + CELL_SIZE / 2 - 6, BOARD_OFFSET_Y + BOARD_ROWS * CELL_SIZE + 8, color);
    }
    for (int r = 0; r < BOARD_ROWS; ++r) {
        snprintf(text, sizeof(text), "%d", BOARD_ROWS - r);
        drawText(renderer, labelFont, text, BOARD_OFFSET_X - 24, BOARD_OFFSET_Y + r * CELL_SIZE + CELL_SIZE / 2 - 10, color);
    }
}

void drawPieces(SDL_Renderer *renderer, TTF_Font *pieceFont, const GameState *state,
                SDL_Texture *anteaterWhite, SDL_Texture *anteaterBlack) {
    for (int r = 0; r < BOARD_ROWS; ++r) {
        for (int c = 0; c < BOARD_COLS; ++c) {
            ChessPiece p = state->squares[r][c];
            if (p.type == ANTEATER) {
                SDL_Texture *tex = (p.color == WHITE) ? anteaterWhite : anteaterBlack;

            if (tex) {
                SDL_Rect dst = {
                BOARD_OFFSET_X + c * CELL_SIZE + 5,
                BOARD_OFFSET_Y + r * CELL_SIZE + 5,
                CELL_SIZE - 10,
                CELL_SIZE - 10
            };

            SDL_RenderCopy(renderer, tex, NULL, &dst);
            continue;
        }
    }
            const char *s = pieceToUnicode(p);
            if (!s) continue;
            SDL_Color color = (p.color == WHITE) ? (SDL_Color){245,245,245,255} : (SDL_Color){20,20,20,255};
            drawText(renderer, pieceFont, s, BOARD_OFFSET_X + c * CELL_SIZE + 12, BOARD_OFFSET_Y + r * CELL_SIZE + 6, color);
        }
    }
}

void drawGameOverMessage(SDL_Renderer *renderer, TTF_Font *labelFont, const char *message) {
    if (message) drawText(renderer, labelFont, message, 60, 10, (SDL_Color){255,80,80,255});
}

static void drawContinueButton(SDL_Renderer *renderer, TTF_Font *labelFont, SDL_Rect button, const GameState *state) {
    SDL_Color textColor = {255, 255, 255, 255};
    char turnText[32];

    snprintf(turnText, sizeof(turnText), "%s CPU turn", state->currentTurn == WHITE ? "White" : "Black");
    drawText(renderer, labelFont, turnText, button.x, button.y - 34, textColor);

    SDL_SetRenderDrawColor(renderer, 70, 70, 80, 255);
    SDL_RenderFillRect(renderer, &button);

    SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
    SDL_RenderDrawRect(renderer, &button);

    drawText(renderer, labelFont, "Continue", button.x + 32, button.y + 14, textColor);
}

static void drawUndoButton(SDL_Renderer *renderer, TTF_Font *labelFont, SDL_Rect button, int enabled) {
    SDL_Color textColor = enabled
        ? (SDL_Color){255, 255, 255, 255}
        : (SDL_Color){155, 155, 160, 255};

    if (enabled) {
        SDL_SetRenderDrawColor(renderer, 76, 86, 104, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 45, 45, 50, 255);
    }
    SDL_RenderFillRect(renderer, &button);

    SDL_SetRenderDrawColor(renderer, enabled ? 220 : 95, enabled ? 220 : 95, enabled ? 220 : 100, 255);
    SDL_RenderDrawRect(renderer, &button);

    drawText(renderer, labelFont, "Undo", button.x + 52, button.y + 14, textColor);
}

static void drawMenuButton(SDL_Renderer *renderer, TTF_Font *labelFont, SDL_Rect button) {
    SDL_Color textColor = {255, 255, 255, 255};

    SDL_SetRenderDrawColor(renderer, 70, 76, 88, 255);
    SDL_RenderFillRect(renderer, &button);

    SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
    SDL_RenderDrawRect(renderer, &button);

    drawText(renderer, labelFont, "Menu", button.x + 52, button.y + 14, textColor);
}

int runGuiGame(GameState *state, GameMode mode, PieceColor playerColor) {
    const int width = 900, height = 620;
    if (SDL_Init(SDL_INIT_VIDEO) != 0) { printf("SDL_Init Error: %s\n", SDL_GetError()); return 1; }
    if (TTF_Init() != 0) { printf("TTF_Init Error: %s\n", TTF_GetError()); SDL_Quit(); return 1; }
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
    printf("IMG_Init Error: %s\n", IMG_GetError());
    TTF_Quit();
    SDL_Quit();
    return 1;
}

    SDL_Window *window = SDL_CreateWindow("Anteater Chess", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_SHOWN);
    SDL_Renderer *renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED) : NULL;
    TTF_Font *pieceFont = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 32);
    TTF_Font *labelFont = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 20);
    SDL_Texture *anteaterWhite = IMG_LoadTexture(renderer, "assets/anteater_white.png");
    SDL_Texture *anteaterBlack = IMG_LoadTexture(renderer, "assets/anteater_black.png");

    if (anteaterWhite == NULL)
    {
        anteaterWhite = IMG_LoadTexture(renderer, "../assets/anteater_white.png");
    }

    if (anteaterBlack == NULL)
    {
        anteaterBlack = IMG_LoadTexture(renderer, "../assets/anteater_black.png");
    }

    if (anteaterWhite == NULL || anteaterBlack == NULL)
    {
        printf("Failed to load anteater images: %s\n", IMG_GetError());
    }
    if (!anteaterWhite || !anteaterBlack) {
        printf("Failed to load anteater images: %s\n", IMG_GetError());
    }

    if (!window || !renderer || !pieceFont || !labelFont) {
        printf("SDL/TTF setup failed.\n");
        if (pieceFont) TTF_CloseFont(pieceFont);
        if (labelFont) TTF_CloseFont(labelFont);
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    int running = 1;
    int selectedRow = -1;
    int selectedCol = -1;
    int cpuStepRequested = 0;
    const char *message = NULL;
    MoveRecord moveHistory[GUI_MOVE_HISTORY_MAX];
    int moveHistoryCount = 0;
    char positionHistory[GUI_MOVE_HISTORY_MAX + 1][GUI_POSITION_KEY_SIZE];
    int positionHistoryCount = 0;

    MoveList selectedMoves;
    clearMoveList(&selectedMoves);
    pushGuiPosition(state, positionHistory, &positionHistoryCount);

    SDL_Rect continueButton = {
        BOARD_OFFSET_X + BOARD_COLS * CELL_SIZE + 35,
        BOARD_OFFSET_Y + 90,
        155,
        48
    };
    SDL_Rect undoButton = {
        BOARD_OFFSET_X + BOARD_COLS * CELL_SIZE + 35,
        BOARD_OFFSET_Y + 160,
        155,
        48
    };
    SDL_Rect menuButton = {
        BOARD_OFFSET_X + BOARD_COLS * CELL_SIZE + 35,
        BOARD_OFFSET_Y + 230,
        155,
        48
    };

    while (running) {
        SDL_Event e;

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;

            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                if (pointInRect(e.button.x, e.button.y, menuButton)) {
                    running = 0;
                    continue;
                }

                if (pointInRect(e.button.x, e.button.y, undoButton) &&
                    canUndoGuiMove(mode, playerColor, state, moveHistoryCount)) {
                    undoGuiMove(state, mode, playerColor, moveHistory, &moveHistoryCount);
                    message = NULL;
                    cpuStepRequested = 0;
                    selectedRow = selectedCol = -1;
                    clearMoveList(&selectedMoves);
                    trimGuiPositionHistoryAfterUndo(state,
                                                    positionHistory,
                                                    &positionHistoryCount,
                                                    moveHistoryCount);
                    continue;
                }

                if (message) {
                    continue;
                }

                if (mode == C_VS_C_MODE && pointInRect(e.button.x, e.button.y, continueButton)) {
                    cpuStepRequested = 1;
                    continue;
                }

                int col = (e.button.x - BOARD_OFFSET_X) / CELL_SIZE;
                int row = (e.button.y - BOARD_OFFSET_Y) / CELL_SIZE;
                Position clicked = {row, col};

                if (isPositionInBounds(clicked) && e.button.x >= BOARD_OFFSET_X && e.button.y >= BOARD_OFFSET_Y) {
                    if (isHumanTurn(state, mode, playerColor)) {
                        if (selectedRow < 0) {
                            ChessPiece p = state->squares[row][col];

                            if (p.type != EMPTY &&
                                p.color == state->currentTurn &&
                                (mode == H_VS_H_MODE || p.color == playerColor)) {
                                selectedRow = row;
                                selectedCol = col;
                                buildSelectedMoveList(state, selectedRow, selectedCol, &selectedMoves);
                            }
                        } else {
                            if (row == selectedRow && col == selectedCol) {
                                selectedRow = selectedCol = -1;
                                clearMoveList(&selectedMoves);
                            } else if (moveListContainsDest(&selectedMoves, row, col)) {
                                Move m;

                                if (getMoveFromListByDest(&selectedMoves, row, col, &m) &&
                                    isMoveLegal(state, m)) {
                                    GameState beforeMove = *state;
                                    pushGuiMove(state, m, moveHistory, &moveHistoryCount);
                                    logGuiAppliedMove(&beforeMove, state, m, "Human", "manual", NULL);
                                    pushGuiPosition(state, positionHistory, &positionHistoryCount);
                                    message = guiResultMessage(state, positionHistory, positionHistoryCount);
                                }

                                selectedRow = selectedCol = -1;
                                clearMoveList(&selectedMoves);
                            } else {
                                ChessPiece p = state->squares[row][col];

                                if (p.type != EMPTY &&
                                    p.color == state->currentTurn &&
                                    (mode == H_VS_H_MODE || p.color == playerColor)) {
                                    selectedRow = row;
                                    selectedCol = col;
                                    buildSelectedMoveList(state, selectedRow, selectedCol, &selectedMoves);
                                } else {
                                    selectedRow = selectedCol = -1;
                                    clearMoveList(&selectedMoves);
                                }
                            }
                        }
                    }
                } else {
                    selectedRow = selectedCol = -1;
                    clearMoveList(&selectedMoves);
                }
            }
        }

        if (!message && isCpuTurn(state, mode, playerColor)) {
            int allowCpuMove = (mode == C_VS_C_MODE) ? cpuStepRequested : 1;

            if (allowCpuMove) {
                int depth = 10;
                Move m;
                GameState beforeMove;
                CpuSearchStats searchStats;
                int usedFallback = 0;
                const char *selectedBy = "search";

                cpuStepRequested = 0;
                selectedRow = selectedCol = -1;
                clearMoveList(&selectedMoves);

                beforeMove = *state;
                memset(&searchStats, 0, sizeof(searchStats));

                m = chooseGuiCpuMove(state,
                                     state->currentTurn,
                                     depth,
                                     positionHistory,
                                     positionHistoryCount,
                                     moveHistory,
                                     moveHistoryCount,
                                     &usedFallback);
                getLastCpuSearchStats(&searchStats);

                if (usedFallback) {
                    selectedBy = "anti_repeat_fallback";
                } else if (searchStats.completedDepth == 0 &&
                           searchStats.nodes == 0u &&
                           searchStats.requestedDepth > 0) {
                    selectedBy = "book";
                }

                if (m.start.row >= 0) {
                    pushGuiMove(state, m, moveHistory, &moveHistoryCount);
                    logGuiAppliedMove(&beforeMove, state, m, "CPU", selectedBy, &searchStats);
                    pushGuiPosition(state, positionHistory, &positionHistoryCount);
                }

                message = guiResultMessage(state, positionHistory, positionHistoryCount);

                if (mode != C_VS_C_MODE) {
                    SDL_Delay(300);
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 28, 28, 32, 255);
        SDL_RenderClear(renderer);

        drawBoardGrid(renderer, selectedRow, selectedCol);
        drawMoveHighlights(renderer, state, &selectedMoves);
        drawBoardLabels(renderer, labelFont);
        drawPieces(renderer, pieceFont, state, anteaterWhite, anteaterBlack);

        if (mode == C_VS_C_MODE && !message) {
            drawContinueButton(renderer, labelFont, continueButton, state);
        }
        drawUndoButton(renderer, labelFont, undoButton,
                       canUndoGuiMove(mode, playerColor, state, moveHistoryCount));
        drawMenuButton(renderer, labelFont, menuButton);

        drawGameOverMessage(renderer, labelFont, message);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    if (anteaterWhite) SDL_DestroyTexture(anteaterWhite);
    if (anteaterBlack) SDL_DestroyTexture(anteaterBlack);
    IMG_Quit();
    TTF_CloseFont(pieceFont);
    TTF_CloseFont(labelFont);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
