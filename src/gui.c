#include "gui.h"
#include "board.h"
#include "game.h"
#include "cpu.h"
#include "moves.h"
#include <SDL2/SDL_image.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

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
            scanf("%15s", promo);
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

static int moveListContainsDest(const MoveList *moves, int row, int col) {
    for (int i = 0; i < moves->count; ++i) {
        if (moves->moves[i].dest.row == row && moves->moves[i].dest.col == col) return 1;
    }
    return 0;
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
    TTF_Font *pieceFont = TTF_OpenFont("/usr/share/fonts/dejavu/DejaVuSans.ttf", 32);
    TTF_Font *labelFont = TTF_OpenFont("/usr/share/fonts/dejavu/DejaVuSans.ttf", 20);
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

    MoveList selectedMoves;
    clearMoveList(&selectedMoves);

    SDL_Rect continueButton = {
        BOARD_OFFSET_X + BOARD_COLS * CELL_SIZE + 35,
        BOARD_OFFSET_Y + 90,
        155,
        48
    };

    while (running) {
        SDL_Event e;

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;

            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT && !message) {
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
                                makeMoveFromClick(&m, selectedRow, selectedCol, row, col);

                                if (isMoveLegal(state, m)) {
                                    applyMove(state, m);
                                    message = getResultMessage(state);
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
                int depth = (mode == C_VS_C_MODE) ? 4 : 10;
                Move m;

                cpuStepRequested = 0;
                selectedRow = selectedCol = -1;
                clearMoveList(&selectedMoves);

                m = chooseBestMoveDepth(state, state->currentTurn, depth);

                if (m.start.row >= 0) {
                    applyMove(state, m);
                }

                message = getResultMessage(state);

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
