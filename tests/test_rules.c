#include "board.h"
#include "cpu.h"
#include "game.h"
#include "moves.h"
#include <assert.h>
#include <stdio.h>

static void onlyKingsAndAnteater(GameState *s) {
    clearGameState(s);
    s->squares[7][5] = makePiece(KING, WHITE);
    s->whiteKingPos = (Position){7,5};
    s->squares[0][5] = makePiece(KING, BLACK);
    s->blackKingPos = (Position){0,5};
    s->currentTurn = WHITE;
}

static void test_initial_board(void) {
    GameState s;
    initializeGameState(&s);
    assert(s.squares[7][3].type == ANTEATER && s.squares[7][3].color == WHITE);
    assert(s.squares[7][6].type == ANTEATER && s.squares[7][6].color == WHITE);
    assert(s.squares[0][3].type == ANTEATER && s.squares[0][3].color == BLACK);
    assert(s.squares[0][6].type == ANTEATER && s.squares[0][6].color == BLACK);
    assert(s.whiteKingPos.row == 7 && s.whiteKingPos.col == 5);
    assert(s.blackKingPos.row == 0 && s.blackKingPos.col == 5);
}

static void test_anteater_does_not_attack_king(void) {
    GameState s;
    onlyKingsAndAnteater(&s);
    s.squares[6][5] = makePiece(ANTEATER, BLACK);
    assert(!isKingInCheck(&s, WHITE));
}

static void test_anteater_cannot_capture_piece(void) {
    GameState s;
    onlyKingsAndAnteater(&s);
    s.squares[4][4] = makePiece(ANTEATER, WHITE);
    s.squares[3][4] = makePiece(ROOK, BLACK);
    Move m = {{4,4},{3,4},EMPTY};
    assert(!isMoveLegal(&s, m));
}

static void test_anteater_multi_capture(void) {
    GameState s;
    onlyKingsAndAnteater(&s);
    s.squares[4][3] = makePiece(ANTEATER, WHITE); /* D4 */
    s.squares[3][4] = makePiece(PAWN, BLACK);     /* E5 adjacent */
    s.squares[2][5] = makePiece(PAWN, BLACK);     /* F6 connected */
    s.squares[2][6] = makePiece(PAWN, BLACK);     /* G6 connected */
    s.squares[5][9] = makePiece(PAWN, BLACK);     /* J3 disconnected */
    Move m = {{4,3},{2,6},EMPTY};
    assert(isMoveLegal(&s, m));
    applyMove(&s, m);
    assert(s.squares[2][6].type == ANTEATER);
    assert(s.squares[3][4].type == EMPTY);
    assert(s.squares[2][5].type == EMPTY);
    assert(s.squares[5][9].type == PAWN);
}

static void test_en_passant_and_promotion_basics(void) {
    GameState s;
    onlyKingsAndAnteater(&s);
    s.squares[6][0] = makePiece(PAWN, WHITE);
    Move two = {{6,0},{4,0},EMPTY};
    assert(isMoveLegal(&s, two));
    applyMove(&s, two);
    assert(s.enPassantTarget.row == 5 && s.enPassantTarget.col == 0);

    clearGameState(&s);
    s.squares[7][5] = makePiece(KING, WHITE); 
    s.whiteKingPos = (Position){7,5};
    s.squares[0][5] = makePiece(KING, BLACK); s
    .blackKingPos = (Position){0,5};
    s.squares[1][0] = makePiece(PAWN, WHITE);
    s.currentTurn = WHITE;
    Move promo = {{1,0},{0,0},QUEEN};
    assert(isMoveLegal(&s, promo));
    applyMove(&s, promo);
    assert(s.squares[0][0].type == QUEEN);
}

static void test_cpu_depth_ten_returns_legal_move(void) {
    GameState s;
    Move m;

    onlyKingsAndAnteater(&s);
    m = chooseBestMoveDepth(&s, WHITE, 10);
    assert(m.start.row >= 0);
    assert(isMoveLegal(&s, m));
}

int main(void) {
    test_initial_board();
    test_anteater_does_not_attack_king();
    test_anteater_cannot_capture_piece();
    test_anteater_multi_capture();
    test_en_passant_and_promotion_basics();
    test_cpu_depth_ten_returns_legal_move();
    puts("All rule tests passed.");
    return 0;
}
