#include "board.h"
#include "cpu.h"
#include "eval.h"
#include "game.h"
#include "gui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DEFAULT_TT_MB 1024
#define DEFAULT_CPU_TIME_MS 2500
#define WARMUP_SEARCH_DEPTH 10
#define WARMUP_DEFAULT_GAMES 3
#define WARMUP_MAX_GAMES 10
#define WARMUP_MAX_PLIES 120

static void printUsage(const char *programName)
{
    printf("Usage: %s [--debug-log PATH]\n", programName ? programName : "anteater_chess");
}

static void printTtStatus(unsigned long ttMb)
{
    unsigned long long used = getCpuTranspositionTableUsedEntries();
    unsigned long long capacity = getCpuTranspositionTableCapacityEntries();
    double fill = capacity > 0u ? ((double)used * 100.0) / (double)capacity : 0.0;

    printf("TT=%luMB, fill=%.2f%% (%llu/%llu entries)",
           ttMb,
           fill,
           used,
           capacity);
}

static int moveIsUsable(Move move)
{
    return move.start.row >= 0 &&
           move.start.col >= 0 &&
           move.dest.row >= 0 &&
           move.dest.col >= 0;
}

static void runCpuWarmupGames(int gameCount)
{
    int totalPlies = 0;

    if (gameCount <= 0)
    {
        return;
    }

    if (gameCount > WARMUP_MAX_GAMES)
    {
        gameCount = WARMUP_MAX_GAMES;
    }

    printf("Warming TT with %d CPU self-play game(s), max %d plies each.\n",
           gameCount,
           WARMUP_MAX_PLIES);

    for (int game = 1; game <= gameCount; ++game)
    {
        GameState warmup;
        int plies = 0;

        initializeGameState(&warmup);

        while (plies < WARMUP_MAX_PLIES && getResultMessage(&warmup) == NULL)
        {
            Move move = chooseBestMoveDepth(&warmup,
                                            warmup.currentTurn,
                                            WARMUP_SEARCH_DEPTH);

            if (!moveIsUsable(move) || !isMoveLegal(&warmup, move))
            {
                break;
            }

            applyMove(&warmup, move);
            plies++;
        }

        totalPlies += plies;
        printf("Warmup game %d/%d complete: %d plies%s\n",
               game,
               gameCount,
               plies,
               getResultMessage(&warmup) ? ", terminal position" : "");
    }

    {
        CpuSearchStats stats;
        getLastCpuSearchStats(&stats);
        printf("Warmup complete: %d total plies, last depth %d/%d, last nodes %lu, TT hits total %llu, TT cutoffs total %llu\n",
               totalPlies,
               stats.completedDepth,
               stats.requestedDepth,
               stats.nodes,
               stats.totalTtHits,
               stats.totalTtCutoffs);
        printTtStatus(getCpuTranspositionTableSizeMb());
        printf("\n");
    }
}

int main(int argc, char **argv)
{
    GameState state;
    int choice;
    int color;
    int cpuTimeMs = DEFAULT_CPU_TIME_MS;
    unsigned long ttMb;
    const char *debugLogPath = NULL;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--debug-log") == 0)
        {
            if (i + 1 >= argc)
            {
                printUsage(argv[0]);
                return 1;
            }
            debugLogPath = argv[++i];
        }
        else if (strncmp(argv[i], "--debug-log=", 12) == 0)
        {
            debugLogPath = argv[i] + 12;
        }
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            printUsage(argv[0]);
            return 0;
        }
        else
        {
            printf("Unknown option: %s\n", argv[i]);
            printUsage(argv[0]);
            return 1;
        }
    }

    initializeGameState(&state);
    setCpuSearchPreset("storm");
    setCpuSearchTimeLimitMs(cpuTimeMs);
    setCpuOpeningBookEnabled(1);
    setPstScalePercent(100);
    setLearnedEvalEnabled(1);
    ttMb = setCpuTranspositionTableSizeMb(DEFAULT_TT_MB);
    if (ttMb == 0u)
    {
        printf("Warning: could not allocate %dMB CPU transposition table; using default allocation.\n",
               DEFAULT_TT_MB);
        ttMb = getCpuTranspositionTableSizeMb();
    }
    if (debugLogPath != NULL && !setGuiDebugLogPath(debugLogPath))
    {
        printf("Could not open GUI debug log: %s\n", debugLogPath);
        return 1;
    }
    atexit(closeGuiDebugLog);
    srand((unsigned)time(NULL));

    for (;;)
    {
        printf("\n===== ANTEATER CHESS =====\n");
        printf("CPU config: preset=%s, time=%dms, ",
               getCpuSearchPresetName(),
               cpuTimeMs);
        printTtStatus(ttMb);
        printf(", PST=100%%, learned_eval=on, book=on\n");
        if (debugLogPath != NULL)
        {
            printf("GUI debug log: %s\n", debugLogPath);
        }
        printf("1. Console: Human vs Human\n");
        printf("2. Console: Human vs CPU\n");
        printf("3. GUI: Human vs CPU\n");
        printf("4. GUI: CPU vs CPU\n");
        printf("5. CPU warmup self-play\n");
        printf("6. Configure TT memory\n");
        printf("7. Configure CPU time\n");
        printf("0. Exit\n");
        printf("Select mode: ");

        choice = 1;

        if (scanf("%d", &choice) != 1)
        {
            printf("Invalid input.\n");
            return 1;
        }

        if (choice == 0)
        {
            return 0;
        }

        if (choice == 1)
        {
            initializeGameState(&state);
            playConsoleGame(&state, 0, WHITE);
            continue;
        }

        if (choice == 2)
        {
            color = 1;

            printf("Choose side: 1 = White, 2 = Black: ");

            if (scanf("%d", &color) != 1)
            {
                return 1;
            }

            initializeGameState(&state);

            if (color == 2)
            {
                playConsoleGame(&state, 1, BLACK);
            }
            else
            {
                playConsoleGame(&state, 1, WHITE);
            }

            continue;
        }

        if (choice == 3)
        {
            color = 1;

            printf("Choose side: 1 = White, 2 = Black: ");

            if (scanf("%d", &color) != 1)
            {
                return 1;
            }

            initializeGameState(&state);
            if (color == 2)
            {
                runGuiGame(&state, H_VS_C_MODE, BLACK);
            }
            else
            {
                runGuiGame(&state, H_VS_C_MODE, WHITE);
            }
            continue;
        }

        if (choice == 4)
        {
            initializeGameState(&state);
            runGuiGame(&state, C_VS_C_MODE, WHITE);
            continue;
        }

        if (choice == 5)
        {
            int games = WARMUP_DEFAULT_GAMES;

            printf("Warmup games, 1-%d (recommended %d): ",
                   WARMUP_MAX_GAMES,
                   WARMUP_DEFAULT_GAMES);

            if (scanf("%d", &games) != 1)
            {
                return 1;
            }

            if (games < 1)
            {
                printf("Warmup cancelled.\n");
                continue;
            }

            if (games > WARMUP_MAX_GAMES)
            {
                games = WARMUP_MAX_GAMES;
            }

            runCpuWarmupGames(games);
            continue;
        }

        if (choice == 6)
        {
            int requestedMb;
            unsigned long newTtMb;

            printf("Current ");
            printTtStatus(ttMb);
            printf(". Enter new TT memory in MB (recommended %d, resize clears TT): ",
                   DEFAULT_TT_MB);

            if (scanf("%d", &requestedMb) != 1)
            {
                return 1;
            }

            if (requestedMb <= 0)
            {
                printf("TT memory unchanged.\n");
                continue;
            }

            newTtMb = setCpuTranspositionTableSizeMb(requestedMb);
            if (newTtMb == 0u)
            {
                printf("Could not allocate %dMB TT. Keeping current TT at %luMB.\n",
                       requestedMb,
                       ttMb);
            }
            else
            {
                ttMb = newTtMb;
                printf("TT resized to %luMB. Existing TT data was cleared by resize.\n", ttMb);
            }

            continue;
        }

        if (choice == 7)
        {
            int requestedMs;

            printf("Current CPU time: %dms. Enter new time in ms (2000, 2500, or 3000 recommended): ",
                   cpuTimeMs);

            if (scanf("%d", &requestedMs) != 1)
            {
                return 1;
            }

            if (requestedMs <= 0)
            {
                printf("CPU time unchanged.\n");
                continue;
            }

            if (requestedMs < 500)
            {
                requestedMs = 500;
            }
            else if (requestedMs > 10000)
            {
                requestedMs = 10000;
            }

            cpuTimeMs = requestedMs;
            setCpuSearchTimeLimitMs(cpuTimeMs);
            printf("CPU time set to %dms per move.\n", cpuTimeMs);
            continue;
        }

        printf("Invalid mode.\n");
    }
}
