#include "eval.h"
#include "cpu.h"
#include "game.h"
#include "moves.h"


#if defined(__has_include)
#  if __has_include("generated_eval_weights.h")
#    include "generated_eval_weights.h"
#  endif
#else
#  include "generated_eval_weights.h"
#endif
#include "generated_piece_square_tables.h"
#ifndef EVAL_BIAS
#define EVAL_BIAS 0.0
#endif
#ifndef EVAL_WEIGHT_MATERIAL
#define EVAL_WEIGHT_MATERIAL 0.0
#endif
#ifndef EVAL_WEIGHT_MOBILITY
#define EVAL_WEIGHT_MOBILITY 0.0
#endif
#ifndef EVAL_WEIGHT_PAWN_PROGRESS
#define EVAL_WEIGHT_PAWN_PROGRESS 0.0
#endif
#ifndef EVAL_WEIGHT_ANTEATER_ACTIVITY
#define EVAL_WEIGHT_ANTEATER_ACTIVITY 0.0
#endif
#ifndef EVAL_WEIGHT_KING_SAFETY
#define EVAL_WEIGHT_KING_SAFETY 0.0
#endif
#ifndef EVAL_WEIGHT_HANGING_PIECES
#define EVAL_WEIGHT_HANGING_PIECES 0.0
#endif
#ifndef EVAL_WEIGHT_KING_ESCAPE
#define EVAL_WEIGHT_KING_ESCAPE 0.0
#endif
#ifndef EVAL_WEIGHT_ANTEATER_CAPTURE_POTENTIAL
#define EVAL_WEIGHT_ANTEATER_CAPTURE_POTENTIAL 0.0
#endif
#ifndef EVAL_WEIGHT_PASSED_PAWNS
#define EVAL_WEIGHT_PASSED_PAWNS 0.0
#endif
#ifndef EVAL_WEIGHT_PAWN_STRUCTURE
#define EVAL_WEIGHT_PAWN_STRUCTURE 0.0
#endif

static int g_pstScalePercent = 0;
static int g_learnedEvalEnabled = 0;


//precondition: Take in the piece square table scale percent
//postcondition: Set the piece square table scale percent
void setPstScalePercent(int percent)
{
    if (percent < 0)
    {
        percent = 0;
    }

    if (percent > 100)
    {
        percent = 100;
    }

    g_pstScalePercent = percent;
}

//precondition: None
//postcondition: Return the current piece square table scale percent
int getPstScalePercent(void)
{
    return g_pstScalePercent;
}

//precondition: Take in a boolean-like integer
//postcondition: Enable or disable learned residual evaluation
void setLearnedEvalEnabled(int enabled)
{
    g_learnedEvalEnabled = enabled ? 1 : 0;
}

//precondition: None
//postcondition: Return integer if learned residual evaluation is enabled
int getLearnedEvalEnabled(void)
{
    return g_learnedEvalEnabled;
}

//precondition: Take in the side color
//postcondition: Return the opposite side color
static PieceColor oppositeColorLocal(PieceColor side) {
    if (side == WHITE) return BLACK;
    if (side == BLACK) return WHITE;
    return NONE;
}

//precondition: Take in row and column values
//postcondition: Return integer if the square is inside the board
static int isInsideLocal(int row, int col) {
    return row >= 0 && row < BOARD_ROWS && col >= 0 && col < BOARD_COLS;
}

//precondition: Take in a chess piece and side color
//postcondition: Return score sign based on whether the piece belongs to the side
static int scoreSign(ChessPiece piece, PieceColor side) {
    if (piece.color == side) return 1;
    if (piece.color == oppositeColorLocal(side)) return -1;
    return 0;
}

//precondition: Take in the game state, row, column, and side color
//postcondition: Return integer if the square has an enemy pawn
static int isEnemyPawn(const GameState *state, int row, int col, PieceColor side) {
    ChessPiece p;
    if (!isInsideLocal(row, col)) return 0;
    p = state->squares[row][col];
    return p.type == PAWN && p.color == oppositeColorLocal(side);
}

//precondition: Take in the game state and side color
//postcondition: Return the material score for that side
int evaluateMaterial(const GameState *state, PieceColor side) {
    int row, col;
    int score = 0;

    for (row = 0; row < BOARD_ROWS; row++) {
        for (col = 0; col < BOARD_COLS; col++) {
            ChessPiece piece = state->squares[row][col];
            if (piece.type != EMPTY) {
                score += scoreSign(piece, side) * getPieceValue(piece.type);
            }
        }
    }

    return score;
}

//precondition: Take in the game state and side color
//postcondition: Return the mobility score for that side
int evaluateMobility(const GameState *state, PieceColor side) {
    MoveList myMoves;
    MoveList enemyMoves;
    GameState copy = *state;
    PieceColor enemy = oppositeColorLocal(side);

    copy.currentTurn = side;
    generateAllPseudoLegalMoves(&copy, side, &myMoves);

    copy.currentTurn = enemy;
    generateAllPseudoLegalMoves(&copy, enemy, &enemyMoves);

    return (myMoves.count - enemyMoves.count) * 4;
}

//precondition: Take in the game state and side color
//postcondition: Return the pawn progress score for that side
int evaluatePawnProgress(const GameState *state, PieceColor side) {
    int row, col;
    int score = 0;

    for (row = 0; row < BOARD_ROWS; row++) {
        for (col = 0; col < BOARD_COLS; col++) {
            ChessPiece piece = state->squares[row][col];
            int progress = 0;

            if (piece.type != PAWN) continue;

            if (piece.color == WHITE) progress = 6 - row;
            else if (piece.color == BLACK) progress = row - 1;

            if (progress < 0) progress = 0;
            score += scoreSign(piece, side) * progress * 8;
        }
    }

    return score;
}

//precondition: Take in the game state and side color
//postcondition: Return the anteater activity score for that side
int evaluateAnteaterActivity(const GameState *state, PieceColor side) {
    int row, col;
    int score = 0;

    for (row = 0; row < BOARD_ROWS; row++) {
        for (col = 0; col < BOARD_COLS; col++) {
            ChessPiece piece = state->squares[row][col];
            int local = 0;
            int dr, dc;

            if (piece.type != ANTEATER) continue;

            for (dr = -1; dr <= 1; dr++) {
                for (dc = -1; dc <= 1; dc++) {
                    int r = row + dr;
                    int c = col + dc;
                    ChessPiece target;

                    if (dr == 0 && dc == 0) continue;
                    if (!isInsideLocal(r, c)) continue;

                    target = state->squares[r][c];
                    if (target.type == PAWN && target.color != piece.color && target.color != NONE) {
                        local += 45;
                    }
                }
            }

            if (piece.color == WHITE) local += (7 - row) * 5;
            else if (piece.color == BLACK) local += row * 5;

            score += scoreSign(piece, side) * local;
        }
    }

    return score;
}

//precondition: Take in the game state and side color
//postcondition: Return the king safety score for that side
int evaluateKingSafety(const GameState *state, PieceColor side) {
    PieceColor enemy = oppositeColorLocal(side);
    Position myKing = (side == WHITE) ? state->whiteKingPos : state->blackKingPos;
    Position enemyKing = (enemy == WHITE) ? state->whiteKingPos : state->blackKingPos;
    int score = 0;
    int dr, dc;

    if (isInsideLocal(myKing.row, myKing.col)) {
        for (dr = -1; dr <= 1; dr++) {
            for (dc = -1; dc <= 1; dc++) {
                int r = myKing.row + dr;
                int c = myKing.col + dc;
                ChessPiece p;
                if (dr == 0 && dc == 0) continue;
                if (!isInsideLocal(r, c)) continue;
                p = state->squares[r][c];
                if (p.type != EMPTY && p.color == side) score += 12;
            }
        }
    } else {
        score -= 100000;
    }

    if (isInsideLocal(enemyKing.row, enemyKing.col)) {
        for (dr = -1; dr <= 1; dr++) {
            for (dc = -1; dc <= 1; dc++) {
                int r = enemyKing.row + dr;
                int c = enemyKing.col + dc;
                ChessPiece p;
                if (dr == 0 && dc == 0) continue;
                if (!isInsideLocal(r, c)) continue;
                p = state->squares[r][c];
                if (p.type != EMPTY && p.color == enemy) score -= 12;
            }
        }
    } else {
        score += 100000;
    }

    return score;
}

//precondition: Take in the game state and side color
//postcondition: Return the hanging pieces score for that side
int evaluateHangingPieces(const GameState *state, PieceColor side) {
    int row, col;
    int score = 0;
    PieceColor enemy = oppositeColorLocal(side);

    for (row = 0; row < BOARD_ROWS; row++) {
        for (col = 0; col < BOARD_COLS; col++) {
            Position pos = { row, col };
            ChessPiece piece = state->squares[row][col];
            int value;
            int attackedByEnemy;
            int defendedByFriend;
            int localPenalty;

            if (piece.type == EMPTY || piece.type == KING || piece.color == NONE) continue;

            value = getPieceValue(piece.type);
            attackedByEnemy = isSquareAttacked(state, pos, oppositeColorLocal(piece.color));
            defendedByFriend = isSquareAttacked(state, pos, piece.color);

            if (!attackedByEnemy) continue;

            localPenalty = value / 4;
            if (!defendedByFriend) localPenalty += value / 2;

            if (piece.color == side) score -= localPenalty;
            else if (piece.color == enemy) score += localPenalty;
        }
    }

    return score;
}

//precondition: Take in the game state and king side color
//postcondition: Return the number of escape squares for that king
static int countKingEscapeForSide(const GameState *state, PieceColor kingSide) {
    Position king = (kingSide == WHITE) ? state->whiteKingPos : state->blackKingPos;
    PieceColor enemy = oppositeColorLocal(kingSide);
    int dr, dc;
    int count = 0;

    if (!isInsideLocal(king.row, king.col)) return 0;

    for (dr = -1; dr <= 1; dr++) {
        for (dc = -1; dc <= 1; dc++) {
            Position p;
            ChessPiece target;
            if (dr == 0 && dc == 0) continue;
            p.row = king.row + dr;
            p.col = king.col + dc;
            if (!isInsideLocal(p.row, p.col)) continue;

            target = state->squares[p.row][p.col];
            if (target.type != EMPTY && target.color == kingSide) continue;
            if (isSquareAttacked(state, p, enemy)) continue;
            count++;
        }
    }

    return count;
}

//precondition: Take in the game state and side color
//postcondition: Return the king escape square score for that side
int evaluateKingEscapeSquares(const GameState *state, PieceColor side) {
    PieceColor enemy = oppositeColorLocal(side);
    int myEscapes = countKingEscapeForSide(state, side);
    int enemyEscapes = countKingEscapeForSide(state, enemy);
    return (myEscapes - enemyEscapes) * 35;
}

//precondition: Take in the game state, pawn position, and anteater side color
//postcondition: Return the number of connected enemy pawns in the cluster
static int countConnectedEnemyPawnCluster(const GameState *state, int startRow, int startCol, PieceColor anteaterSide) {
    int queueR[BOARD_ROWS * BOARD_COLS];
    int queueC[BOARD_ROWS * BOARD_COLS];
    int visited[BOARD_ROWS][BOARD_COLS];
    int head = 0;
    int tail = 0;
    int count = 0;
    int r, c, dr, dc;

    for (r = 0; r < BOARD_ROWS; r++) {
        for (c = 0; c < BOARD_COLS; c++) visited[r][c] = 0;
    }

    if (!isEnemyPawn(state, startRow, startCol, anteaterSide)) return 0;

    queueR[tail] = startRow;
    queueC[tail] = startCol;
    tail++;
    visited[startRow][startCol] = 1;

    while (head < tail) {
        r = queueR[head];
        c = queueC[head];
        head++;
        count++;

        for (dr = -1; dr <= 1; dr++) {
            for (dc = -1; dc <= 1; dc++) {
                int nr = r + dr;
                int nc = c + dc;
                if (dr == 0 && dc == 0) continue;
                if (!isInsideLocal(nr, nc)) continue;
                if (visited[nr][nc]) continue;
                if (!isEnemyPawn(state, nr, nc, anteaterSide)) continue;

                visited[nr][nc] = 1;
                queueR[tail] = nr;
                queueC[tail] = nc;
                tail++;
            }
        }
    }

    return count;
}

//precondition: Take in the game state and side color
//postcondition: Return the anteater capture potential score for that side
int evaluateAnteaterCapturePotential(const GameState *state, PieceColor side) {
    int row, col;
    int score = 0;

    for (row = 0; row < BOARD_ROWS; row++) {
        for (col = 0; col < BOARD_COLS; col++) {
            ChessPiece piece = state->squares[row][col];
            int bestCluster = 0;
            int adjacentPawns = 0;
            int dr, dc;
            int local;

            if (piece.type != ANTEATER) continue;

            for (dr = -1; dr <= 1; dr++) {
                for (dc = -1; dc <= 1; dc++) {
                    int r = row + dr;
                    int c = col + dc;
                    int cluster;
                    if (dr == 0 && dc == 0) continue;
                    if (!isInsideLocal(r, c)) continue;
                    if (!isEnemyPawn(state, r, c, piece.color)) continue;

                    adjacentPawns++;
                    cluster = countConnectedEnemyPawnCluster(state, r, c, piece.color);
                    if (cluster > bestCluster) bestCluster = cluster;
                }
            }

            local = adjacentPawns * 30 + bestCluster * 45;
            if (bestCluster >= 2) local += 40;
            if (bestCluster >= 4) local += 80;

            score += scoreSign(piece, side) * local;
        }
    }

    return score;
}

//precondition: Take in the game state and side color
//postcondition: Return the passed pawns score for that side
int evaluatePassedPawns(const GameState *state, PieceColor side) {
    int row, col;
    int score = 0;

    for (row = 0; row < BOARD_ROWS; row++) {
        for (col = 0; col < BOARD_COLS; col++) {
            ChessPiece pawn = state->squares[row][col];
            int blocked = 0;
            int checkRow, file;
            int progress = 0;
            int local;

            if (pawn.type != PAWN) continue;

            if (pawn.color == WHITE) {
                for (checkRow = row - 1; checkRow >= 0; checkRow--) {
                    for (file = col - 1; file <= col + 1; file++) {
                        if (!isInsideLocal(checkRow, file)) continue;
                        if (state->squares[checkRow][file].type == PAWN &&
                            state->squares[checkRow][file].color == BLACK) {
                            blocked = 1;
                        }
                    }
                }
                progress = 6 - row;
            } else if (pawn.color == BLACK) {
                for (checkRow = row + 1; checkRow < BOARD_ROWS; checkRow++) {
                    for (file = col - 1; file <= col + 1; file++) {
                        if (!isInsideLocal(checkRow, file)) continue;
                        if (state->squares[checkRow][file].type == PAWN &&
                            state->squares[checkRow][file].color == WHITE) {
                            blocked = 1;
                        }
                    }
                }
                progress = row - 1;
            }

            if (blocked) continue;
            if (progress < 0) progress = 0;

            local = 60 + progress * 20;
            score += scoreSign(pawn, side) * local;
        }
    }

    return score;
}

//precondition: Take in the game state and side color
//postcondition: Return the pawn structure score for that side
int evaluatePawnStructure(const GameState *state, PieceColor side)
{
    int row;
    int col;
    int score = 0;
    int whiteFileCount[BOARD_COLS] = {0};
    int blackFileCount[BOARD_COLS] = {0};

    for (row = 0; row < BOARD_ROWS; row++)
    {
        for (col = 0; col < BOARD_COLS; col++)
        {
            ChessPiece pawn = state->squares[row][col];

            if (pawn.type != PAWN)
            {
                continue;
            }

            if (pawn.color == WHITE)
            {
                whiteFileCount[col]++;
            }
            else if (pawn.color == BLACK)
            {
                blackFileCount[col]++;
            }
        }
    }

    for (row = 0; row < BOARD_ROWS; row++)
    {
        for (col = 0; col < BOARD_COLS; col++)
        {
            ChessPiece pawn = state->squares[row][col];
            int local = 0;
            int *fileCount;
            int hasAdjacentFilePawn = 0;
            int hasConnectedPawn = 0;
            int dc;

            if (pawn.type != PAWN || pawn.color == NONE)
            {
                continue;
            }

            fileCount = (pawn.color == WHITE) ? whiteFileCount : blackFileCount;

            if (fileCount[col] > 1)
            {
                local -= 14 * (fileCount[col] - 1);
            }

            for (dc = -1; dc <= 1; dc += 2)
            {
                int adjFile = col + dc;
                int supportRow = (pawn.color == WHITE) ? row + 1 : row - 1;

                if (adjFile < 0 || adjFile >= BOARD_COLS)
                {
                    continue;
                }

                if (fileCount[adjFile] > 0)
                {
                    hasAdjacentFilePawn = 1;
                }

                if (isInsideLocal(supportRow, adjFile) &&
                    state->squares[supportRow][adjFile].type == PAWN &&
                    state->squares[supportRow][adjFile].color == pawn.color)
                {
                    hasConnectedPawn = 1;
                }

                if (isInsideLocal(row, adjFile) &&
                    state->squares[row][adjFile].type == PAWN &&
                    state->squares[row][adjFile].color == pawn.color)
                {
                    hasConnectedPawn = 1;
                }
            }

            if (!hasAdjacentFilePawn)
            {
                local -= 18;
            }

            if (hasConnectedPawn)
            {
                local += 16;
            }

            score += scoreSign(pawn, side) * local;
        }
    }

    return score;
}
static int getPieceSquareBonus(PieceType type, PieceColor color, int row, int col)
{
    int tableRow;

    /*
       Use White's perspective table.
       For Black, mirror the row so both sides use the same learned table.
    */
    tableRow = (color == WHITE) ? row : (BOARD_ROWS - 1 - row);

    switch (type)
    {
        case PAWN:
            return PST_PAWN[tableRow][col];

        case ROOK:
            return PST_ROOK[tableRow][col];

        case KING:
            return PST_KING[tableRow][col];

        case QUEEN:
            return PST_QUEEN[tableRow][col];

        case KNIGHT:
            return PST_KNIGHT[tableRow][col];

        case BISHOP:
            return PST_BISHOP[tableRow][col];

        case ANTEATER:
            return PST_ANTEATER[tableRow][col];

        default:
            return 0;
    }
}

//precondition: Take in the game state and side color
//postcondition: Return the piece square table score for that side
int evaluatePieceSquareTables(const GameState *state, PieceColor side)
{
    int row;
    int col;
    int score = 0;

    for (row = 0; row < BOARD_ROWS; row++)
    {
        for (col = 0; col < BOARD_COLS; col++)
        {
            ChessPiece piece = state->squares[row][col];

            if (piece.type == EMPTY || piece.color == NONE)
            {
                continue;
            }

            if (piece.color == side)
            {
                score += getPieceSquareBonus(piece.type, piece.color, row, col);
            }
            else
            {
                score -= getPieceSquareBonus(piece.type, piece.color, row, col);
            }
        }
    }

    return score;
}



//precondition: Take in the game state and side color
//postcondition: Return the total evaluation score for that side
int evaluatePosition(const GameState *state, PieceColor side) {
    int pstScore;
    int material;
    int mobility;
    int pawnProgress;
    int anteaterActivity;
    int kingSafety;
    int hangingPieces;
    int kingEscape;
    int anteaterCapturePotential;
    int passedPawns;
    int pawnStructure;
    int score;
    double learned = 0.0;
    PieceColor enemy = oppositeColorLocal(side);

    if (isKingDead(state, side)) return -1000000;
    if (isKingDead(state, enemy)) return 1000000;

    pstScore = evaluatePieceSquareTables(state, side);
    material = evaluateMaterial(state, side);
    mobility = evaluateMobility(state, side);
    pawnProgress = evaluatePawnProgress(state, side);
    anteaterActivity = evaluateAnteaterActivity(state, side);
    kingSafety = evaluateKingSafety(state, side);
    hangingPieces = evaluateHangingPieces(state, side);
    kingEscape = evaluateKingEscapeSquares(state, side);
    anteaterCapturePotential = evaluateAnteaterCapturePotential(state, side);
    passedPawns = evaluatePassedPawns(state, side);
    pawnStructure = evaluatePawnStructure(state, side);

score = material
      + mobility
      + pawnProgress
      + anteaterActivity
      + kingSafety
      + hangingPieces
      + kingEscape
      + anteaterCapturePotential
      + passedPawns
      + pawnStructure
      + ((pstScore * g_pstScalePercent) / 100);

    if (g_learnedEvalEnabled)
    {
        learned =
        EVAL_BIAS +
        EVAL_WEIGHT_MATERIAL * material +
        EVAL_WEIGHT_MOBILITY * mobility +
        EVAL_WEIGHT_PAWN_PROGRESS * pawnProgress +
        EVAL_WEIGHT_ANTEATER_ACTIVITY * anteaterActivity +
        EVAL_WEIGHT_KING_SAFETY * kingSafety +
        EVAL_WEIGHT_HANGING_PIECES * hangingPieces +
        EVAL_WEIGHT_KING_ESCAPE * kingEscape +
        EVAL_WEIGHT_ANTEATER_CAPTURE_POTENTIAL * anteaterCapturePotential +
        EVAL_WEIGHT_PASSED_PAWNS * passedPawns +
        EVAL_WEIGHT_PAWN_STRUCTURE * pawnStructure;
    }

return score + (int)(EVAL_RESIDUAL_SCALE * learned);
}
