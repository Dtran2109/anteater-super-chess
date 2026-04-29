// Author: Bader AlRifai
// Details: This is the dependency file for the rest of the project.
// Course: EECS22L
#ifndef TYPES_H
#define TYPES_H

//Constants
#define BOARD_ROWS 8
#define BOARD_COLS 10
#define MAX_MOVES 512
#define MAX_CAPTURED_PAWNS (BOARD_ROWS * BOARD_COLS)

#define CELL_SIZE 56
#define BOARD_OFFSET_X 60
#define BOARD_OFFSET_Y 40

// Piece types
typedef enum {
    EMPTY = 0,
    PAWN,
    ROOK,
    KING,
    QUEEN,
    KNIGHT,
    BISHOP,
    ANTEATER
} PieceType;

// Piece Color
typedef enum {
    NONE = 0,
    WHITE,
    BLACK
} PieceColor;

//For modes selection
typedef enum {
    H_VS_H_MODE = 0,
    H_VS_C_MODE,
    C_VS_C_MODE
} GameMode;

// Board Position
typedef struct {
    int row;
    int col;
} Position;

// Specific Piece info
typedef struct {
    PieceType type; // ex: pawn/knight
    PieceColor color; // team color
    int MoveCount;   // for castling, pawn first move, and CPU
} ChessPiece;

// Manage Move
typedef struct {
    Position start; // starting square
    Position dest; //destination square
    PieceType promotionGoal; // used for promotion of pawn
} Move;

// Collection of Moves
typedef struct {
    Move moves[MAX_MOVES];
    int count;
} MoveList;

// Represents total status of the game
typedef struct {
    ChessPiece squares[BOARD_ROWS][BOARD_COLS];  // full board, each square holds on chesspiece type

    PieceColor currentTurn;  //stores whose turn it is

    Position whiteKingPos; //stores current position of white king
    Position blackKingPos;  //stores current position of black king

    Position enPassantTarget; //enPassant rule

    // check if castling is possible
    int whiteCastleKingSide;
    int whiteCastleQueenSide;
    int blackCastleKingSide;
    int blackCastleQueenSide;

    int halfmoveClock;
    int fullmoveNumber;
} GameState;

#endif
