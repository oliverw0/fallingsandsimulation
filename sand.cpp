#include <raylib.h>
#include <cstdlib>
#include <vector>
#include <cstring>

constexpr int cellSize = 5;

struct Grid
{
    std::vector<int> cells;
    std::vector<int> next;
    int rows = 0;
    int cols = 0;

    inline int idx(int i, int j) const { return i * cols + j; }
};

struct Player
{
    int posX, posY;
};

Grid grid;

void initializeGrid(int screenWidth, int screenHeight)
{
    grid.rows = screenHeight / cellSize;
    grid.cols = screenWidth / cellSize;
    grid.cells.assign(grid.rows * grid.cols, 0);
    grid.next.assign(grid.rows * grid.cols, 0);
}

void resizeGrid(int newCols, int newRows)
{
    std::vector<int> newCells(newRows * newCols, 0);
    std::vector<int> newNext(newRows * newCols, 0);

    for (int i = 0; i < newRows; i++)
    {
        for (int j = 0; j < newCols; j++)
        {
            if (i < grid.rows && j < grid.cols)
                newCells[i * newCols + j] = grid.cells[grid.idx(i, j)];
        }
    }

    grid.cells.swap(newCells);
    grid.next.swap(newNext);
    grid.cols = newCols;
    grid.rows = newRows;
}

void resetGrid()
{
    std::fill(grid.cells.begin(), grid.cells.end(), 0);
    std::fill(grid.next.begin(), grid.next.end(), 0);
}

void drawGrid()
{
    // Simulate — bottom-up so particles don't cascade in one frame
    for (int i = grid.rows - 1; i >= 0; i--)
    {
        for (int j = 0; j < grid.cols; j++)
        {
            if (grid.cells[grid.idx(i, j)] == 1)
            {
                int below  = 0;
                int belowA = 0;
                int belowB = 0;

                if (i + 1 < grid.rows)
                {
                    below = grid.cells[grid.idx(i + 1, j)];
                    if (j - 1 >= 0)       belowA = grid.cells[grid.idx(i + 1, j - 1)];
                    if (j + 1 < grid.cols) belowB = grid.cells[grid.idx(i + 1, j + 1)];
                }

                if (i + 1 < grid.rows && below == 0)
                {
                    grid.next[grid.idx(i, j)] = 0;
                    grid.next[grid.idx(i + 1, j)] = 1;
                }
                else if (i + 1 < grid.rows && belowA == 0 && j - 1 >= 0)
                {
                    grid.next[grid.idx(i, j)] = 0;
                    grid.next[grid.idx(i + 1, j - 1)] = 1;
                }
                else if (i + 1 < grid.rows && belowB == 0 && j + 1 < grid.cols)
                {
                    grid.next[grid.idx(i, j)] = 0;
                    grid.next[grid.idx(i + 1, j + 1)] = 1;
                }
                else
                {
                    grid.next[grid.idx(i, j)] = 1;
                }
            }
        }
    }

    // Swap pointers instead of copying element-by-element
    grid.cells.swap(grid.next);
    std::fill(grid.next.begin(), grid.next.end(), 0);

    // Draw
    for (int i = 0; i < grid.rows; i++)
    {
        for (int j = 0; j < grid.cols; j++)
        {
            DrawRectangle(
                j * cellSize, i * cellSize, cellSize, cellSize,
                grid.cells[grid.idx(i, j)] == 1 ? BLUE : BLACK
            );
        }
    }
}

void drawPlayer(const Player& player)
{
    DrawRectangle(player.posX * cellSize, player.posY * cellSize, cellSize, cellSize, RED);
}

void handlePlayerInput(Player* player)
{
    // TODO: Collision with Sand
    /*
        Logic: If going left and there is sand, check above (y-1) and move there if empty, otherwise reject.
        Same for right. For down, check below (y+1) and move there if empty, otherwise reject.
    */
    if (IsKeyDown(KEY_A) && player->posX > 0)              
    { 
        player->posX--; 
    }
    if (IsKeyDown(KEY_D) && player->posX < grid.cols - 1)  
    { 
        player->posX++; 
    }
    if (IsKeyDown(KEY_W) && player->posY > 0)              
    { 
        player->posY--; 
    }
    if (IsKeyDown(KEY_S) && player->posY < grid.rows - 1)  
    { 
        player->posY++; 
    }

    // TODO: Add jump with timer & gravity
}

void handleInput(int rangeSize)
{
    Vector2 pos = GetMousePosition();
    int x = pos.x / cellSize;
    int y = pos.y / cellSize;

    if (IsKeyPressed(KEY_R))
        resetGrid();

    for (int dy = -rangeSize; dy <= rangeSize; ++dy)
    {
        for (int dx = -rangeSize; dx <= rangeSize; ++dx)
        {
            int nx = x + dx;
            int ny = y + dy;

            if (nx >= 0 && nx < grid.cols && ny >= 0 && ny < grid.rows)
            {
                if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))  grid.cells[grid.idx(ny, nx)] = 1;
                if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) grid.cells[grid.idx(ny, nx)] = 0;
            }
        }
    }
}

int main()
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1366, 768, "Falling Sand");
    SetTargetFPS(60);

    initializeGrid(GetScreenWidth(), GetScreenHeight());
    Player player = { grid.cols / 2, grid.rows / 2 };

    while (!WindowShouldClose())
    {
        if (IsWindowResized())
            resizeGrid(GetScreenWidth() / cellSize, GetScreenHeight() / cellSize);

        BeginDrawing();
        ClearBackground(BLACK);
        handleInput(5);
        handlePlayerInput(&player);
        drawGrid();
        drawPlayer(player);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}