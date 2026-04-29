
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "types.h"

const char *pieceToSymbolConsole(ChessPiece piece);
void printConsoleBoard(const GameState *state);
int parsePosition(const char *text, Position *pos);
void formatPosition(Position pos, char out[3]);
void playConsoleGame(GameState *state, int useCpu, PieceColor playerColor);
void drawBoardGrid(SDL_Renderer *renderer, int selectedRow, int selectedCol);
void drawMoveHighlights(SDL_Renderer *renderer, const GameState *state, const MoveList *moves);
void drawBoardLabels(SDL_Renderer *renderer, TTF_Font *labelFont);
void drawPieces(SDL_Renderer *renderer, TTF_Font *pieceFont, const GameState *state, SDL_Texture *anteaterWhite, SDL_Texture *anteaterBlack);
void drawGameOverMessage(SDL_Renderer *renderer, TTF_Font *labelFont, const char *message);
int runGuiGame(GameState *state, GameMode mode, PieceColor playerColor);
int isHumanTurn(const GameState *state, GameMode mode, PieceColor playerColor);
int isCpuTurn(const GameState *state, GameMode mode, PieceColor playerColor);
int makeMoveFromClick(Move *move, int fromRow, int fromCol, int toRow, int toCol);
