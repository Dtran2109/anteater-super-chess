// Author: Duc Tran
// Course: EECS 22L
// Assignment: Chess Game Project
/* description: This file implements the board evaluation system for the CPU player.
 It generate from the a linear combination of raw cpu vs cpu data.
 
 The handcrafted features include material count, mobility, pawn progress,
 anteater activity, king safety, hanging pieces, king escape squares,
 anteater capture potential, and passed pawns.
 
 This file can be treat as a preprocessed data for generated_piece_square_tables.h
 */
#ifndef GENERATED_EVAL_WEIGHTS_H
#define GENERATED_EVAL_WEIGHTS_H

#define EVAL_RESIDUAL_SCALE 1.000000000000
#define EVAL_BIAS 42.473070621490
#define EVAL_WEIGHT_MATERIAL 0.000000000000
#define EVAL_WEIGHT_MOBILITY 0.729553341866
#define EVAL_WEIGHT_PAWN_PROGRESS 1.456616520882
#define EVAL_WEIGHT_ANTEATER_ACTIVITY -3.384404420853
#define EVAL_WEIGHT_KING_SAFETY 2.124426603317
#define EVAL_WEIGHT_HANGING_PIECES -0.195119604468
#define EVAL_WEIGHT_KING_ESCAPE -0.105444088578
#define EVAL_WEIGHT_ANTEATER_CAPTURE_POTENTIAL 1.806458830833
#define EVAL_WEIGHT_PASSED_PAWNS 0.339110881090

#endif /* GENERATED_EVAL_WEIGHTS_H */
