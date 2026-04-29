#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include <stdio.h>
#include "types.h"

#define BOARD_STRING_SIZE 96
#define MOVE_STRING_SIZE 8

/*
 * A compact snapshot used for self-play training data.
 * Store the position before the move, the move selected, and the side to move.
 * After the game ends, write all saved samples with the final result attached.
 */
typedef struct {
    int gameId;
    int ply;
    GameState stateBefore;
    PieceColor sideToMove;
    Move chosenMove;
    int searchScore;
} TrainingSample;

/* Creates a starting position matching the GUI orientation: white at rows 6/7, black at rows 0/1. */
void initTrainingGameState(GameState *state);

/* Converts board and move data into CSV-safe strings. */
void serializeBoard(const GameState *state, char *buffer, int bufferSize);
void moveToString(Move move, char *buffer, int bufferSize);
const char *sideToString(PieceColor side);

/* CSV writer helpers. */
void writeTrainingCsvHeader(FILE *out);
void writeTrainingSampleCsv(FILE *out, const TrainingSample *sample, int finalResultForSide);

#endif
