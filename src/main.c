#include "board.h"
#include "gui.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void)
{
    GameState state;
    int choice;
    int color;

    initializeGameState(&state);
    srand((unsigned)time(NULL));

    printf("\n===== ANTEATER CHESS =====\n");
    printf("1. Console: Human vs Human\n");
    printf("2. Console: Human vs CPU\n");
    printf("3. GUI: Human vs CPU\n");
    printf("4. GUI: CPU vs CPU\n");
    printf("Select mode: ");

    choice = 1;

    if (scanf("%d", &choice) != 1)
    {
        printf("Invalid input.\n");
        return 1;
    }

    if (choice == 1)
    {
        playConsoleGame(&state, 0, WHITE);
        return 0;
    }

    if (choice == 2)
    {
        color = 1;

        printf("Choose side: 1 = White, 2 = Black: ");

        if (scanf("%d", &color) != 1)
        {
            return 1;
        }

        if (color == 2)
        {
            playConsoleGame(&state, 1, BLACK);
        }
        else
        {
            playConsoleGame(&state, 1, WHITE);
        }

        return 0;
    }

    if (choice == 3)
    {
        color = 1;

        printf("Choose side: 1 = White, 2 = Black: ");

        if (scanf("%d", &color) != 1)
        {
            return 1;
        }

        if (color == 2)
        {
            return runGuiGame(&state, H_VS_C_MODE, BLACK);
        }
        else
        {
            return runGuiGame(&state, H_VS_C_MODE, WHITE);
        }
    }

    if (choice == 4)
    {
        return runGuiGame(&state, C_VS_C_MODE, WHITE);
    }

    printf("Invalid mode.\n");
    return 1;
}