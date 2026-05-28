#include "world/world.h"

int main()
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1366, 768, "Falling Sand");
    SetTargetFPS(60);

    World world(GetScreenWidth(), GetScreenHeight());

    while (!WindowShouldClose())
    {
        world.update();
        world.draw();
    }

    CloseWindow();
    return 0;
}
