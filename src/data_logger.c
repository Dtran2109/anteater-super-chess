#include "data_logger.h"
#include "eval.h"
#include <stdio.h>
#include <string.h>

static ChessPiece makePieceLocal(PieceType type, PieceColor color) {
    ChessPiece piece;
    piece.type = type;
    piece.color = color;
    piece.MoveCount = 0;
    return piece;
}

static ChessPiece makeEmptyLocal(void) {
    return makePieceLocal(EMPTY, NONE);
}

static char pieceToChar(ChessPiece piece) {
    char ch = '.';

    switch (piece.type) {
        case PAWN:     ch = 'P'; break;
        case ROOK:     ch = 'R'; break;
        case KNIGHT:   ch = 'N'; break;
        case BISHOP:   ch = 'B'; break;
        case QUEEN:    ch = 'Q'; break;
        case KING:     ch = 'K'; break;
        case ANTEATER: ch = 'A'; break;
        case EMPTY:
        default:       ch = '.'; break;
    }

    if (piece.color == BLACK && ch >= 'A' && ch <= 'Z') {
        ch = (char)(ch - 'A' + 'a');
    }

    return ch;
}

void initTrainingGameState(GameState *state) {
    int row, col;

    if (state == NULL) return;

    for (row = 0; row < BOARD_ROWS; row++) {
        for (col = 0; col < BOARD_COLS; col++) {
            state->squares[row][col] = makeEmptyLocal();
        }
    }

    state->currentTurn = WHITE;

    state->whiteCastleKingSide = 1;
    state->whiteCastleQueenSide = 1;
    state->blackCastleKingSide = 1;
    state->blackCastleQueenSide = 1;

    state->halfmoveClock = 0;
    state->fullmoveNumber = 1;

    state->enPassantTarget.row = -1;
    state->enPassantTarget.col = -1;

    /* Black back row: R N B A Q K A B N R */
    state->squares[0][0] = makePieceLocal(ROOK, BLACK);
    state->squares[0][1] = makePieceLocal(KNIGHT, BLACK);
    state->squares[0][2] = makePieceLocal(BISHOP, BLACK);
    state->squares[0][3] = makePieceLocal(ANTEATER, BLACK);
    state->squares[0][4] = makePieceLocal(QUEEN, BLACK);
    state->squares[0][5] = makePieceLocal(KING, BLACK);
    state->squares[0][6] = makePieceLocal(ANTEATER, BLACK);
    state->squares[0][7] = makePieceLocal(BISHOP, BLACK);
    state->squares[0][8] = makePieceLocal(KNIGHT, BLACK);
    state->squares[0][9] = makePieceLocal(ROOK, BLACK);

    for (col = 0; col < BOARD_COLS; col++) {
        state->squares[1][col] = makePieceLocal(PAWN, BLACK);
        state->squares[6][col] = makePieceLocal(PAWN, WHITE);
    }

    /* White back row: R N B A Q K A B N R */
    state->squares[7][0] = makePieceLocal(ROOK, WHITE);
    state->squares[7][1] = makePieceLocal(KNIGHT, WHITE);
    state->squares[7][2] = makePieceLocal(BISHOP, WHITE);
    state->squares[7][3] = makePieceLocal(ANTEATER, WHITE);
    state->squares[7][4] = makePieceLocal(QUEEN, WHITE);
    state->squares[7][5] = makePieceLocal(KING, WHITE);
    state->squares[7][6] = makePieceLocal(ANTEATER, WHITE);
    state->squares[7][7] = makePieceLocal(BISHOP, WHITE);
    state->squares[7][8] = makePieceLocal(KNIGHT, WHITE);
    state->squares[7][9] = makePieceLocal(ROOK, WHITE);

    state->blackKingPos.row = 0;
    state->blackKingPos.col = 5;
    state->whiteKingPos.row = 7;
    state->whiteKingPos.col = 5;
}

void serializeBoard(const GameState *state, char *buffer, int bufferSize) {
    int row, col;
    int index = 0;

    if (buffer == NULL || bufferSize <= 0) return;
    buffer[0] = '\0';
    if (state == NULL) return;

    for (row = 0; row < BOARD_ROWS; row++) {
        for (col = 0; col < BOARD_COLS; col++) {
            if (index + 1 >= bufferSize) {
                buffer[bufferSize - 1] = '\0';
                return;
            }
            buffer[index++] = pieceToChar(state->squares[row][col]);
        }

        if (row != BOARD_ROWS - 1) {
            if (index + 1 >= bufferSize) {
                buffer[bufferSize - 1] = '\0';
                return;
            }
            buffer[index++] = '/';
        }
    }

    if (index >= bufferSize) index = bufferSize - 1;
    buffer[index] = '\0';
}

void moveToString(Move move, char *buffer, int bufferSize) {
    char fromFile;
    char toFile;
    int fromRank;
    int toRank;

    if (buffer == NULL || bufferSize <= 0) return;
    buffer[0] = '\0';

    if (bufferSize < MOVE_STRING_SIZE) return;

    if (move.start.row < 0 || move.start.col < 0 ||
        move.dest.row < 0 || move.dest.col < 0) {
        snprintf(buffer, bufferSize, "none");
        return;
    }

    fromFile = (char)('A' + move.start.col);
    toFile = (char)('A' + move.dest.col);
    fromRank = BOARD_ROWS - move.start.row;
    toRank = BOARD_ROWS - move.dest.row;

    if (move.promotionGoal != EMPTY) {
        char promo = pieceToChar(makePieceLocal(move.promotionGoal, WHITE));
        snprintf(buffer, bufferSize, "%c%d%c%d%c", fromFile, fromRank, toFile, toRank, promo);
    } else {
        snprintf(buffer, bufferSize, "%c%d%c%d", fromFile, fromRank, toFile, toRank);
    }
}

const char *sideToString(PieceColor side) {
    if (side == WHITE) return "WHITE";
    if (side == BLACK) return "BLACK";
    return "NONE";
}

void writeTrainingCsvHeader(FILE *out) {
    if (out == NULL) return;

    fprintf(out,
            "game_id,ply,side_to_move,chosen_move,search_score,"
            "eval_total,eval_material,eval_mobility,eval_pawn_progress,"
            "eval_anteater_activity,eval_king_safety,eval_hanging_pieces,"
            "eval_king_escape,eval_anteater_capture_potential,eval_passed_pawns,"
            "final_result_for_side,board\n");
}

void writeTrainingSampleCsv(FILE *out, const TrainingSample *sample, int finalResultForSide) {
    char board[BOARD_STRING_SIZE];
    char move[MOVE_STRING_SIZE];
    int evalTotal;
    int evalMaterial;
    int evalMobility;
    int evalPawnProgress;
    int evalAnteaterActivity;
    int evalKingSafety;
    int evalHangingPieces;
    int evalKingEscape;
    int evalAnteaterCapturePotential;
    int evalPassedPawns;

    if (out == NULL || sample == NULL) return;

    serializeBoard(&sample->stateBefore, board, sizeof(board));
    moveToString(sample->chosenMove, move, sizeof(move));

    evalTotal = evaluatePosition(&sample->stateBefore, sample->sideToMove);
    evalMaterial = evaluateMaterial(&sample->stateBefore, sample->sideToMove);
    evalMobility = evaluateMobility(&sample->stateBefore, sample->sideToMove);
    evalPawnProgress = evaluatePawnProgress(&sample->stateBefore, sample->sideToMove);
    evalAnteaterActivity = evaluateAnteaterActivity(&sample->stateBefore, sample->sideToMove);
    evalKingSafety = evaluateKingSafety(&sample->stateBefore, sample->sideToMove);
    evalHangingPieces = evaluateHangingPieces(&sample->stateBefore, sample->sideToMove);
    evalKingEscape = evaluateKingEscapeSquares(&sample->stateBefore, sample->sideToMove);
    evalAnteaterCapturePotential = evaluateAnteaterCapturePotential(&sample->stateBefore, sample->sideToMove);
    evalPassedPawns = evaluatePassedPawns(&sample->stateBefore, sample->sideToMove);

    fprintf(out,
            "%d,%d,%s,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s\n",
            sample->gameId,
            sample->ply,
            sideToString(sample->sideToMove),
            move,
            sample->searchScore,
            evalTotal,
            evalMaterial,
            evalMobility,
            evalPawnProgress,
            evalAnteaterActivity,
            evalKingSafety,
            evalHangingPieces,
            evalKingEscape,
            evalAnteaterCapturePotential,
            evalPassedPawns,
            finalResultForSide,
            board);
}
