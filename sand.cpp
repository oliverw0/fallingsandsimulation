#include <raylib.h>
#include <cstdlib>
#include <vector>

constexpr int cellSize = 5;

std::vector<int> grid;
std::vector<int> nextgrid;
int rows = 0;
int cols = 0;

/*
	initializeGrid: 
		Initializes the grid based on the screen dimensions. 
		Calculates the number of rows and columns by dividing the screen width and height by the cell size. 
		It also allocates memory for the grid and nextgrid arrays, which will hold the current and next states of the simulation.
	
	vars
		int screenWidth: The width of the window in pixels.
		int screenHeight: The height of the window in pixels.
*/
void initializeGrid(int screenWidth, int screenHeight)
{
	rows = screenHeight / cellSize;
	cols = screenWidth / cellSize;

	grid.assign(rows * cols, 0);
	nextgrid.assign(rows * cols, 0);
}


/*
	resizeGrid: 
		Resizes the grid when the window is resized. 
		It creates new grid and nextgrid arrays with the new dimensions, copies the existing data from the old grids to the new ones, and initializes any new cells to 0. 
		Finally, it frees the memory of the old grids and updates the global variables to point to the new grids and their dimensions.
	vars
		int newCols: The new number of columns in the grid after resizing.
		int newRows: The new number of rows in the grid after resizing.
*/
void resizeGrid(int newCols, int newRows)
{
	std::vector<int> newGrid(newRows * newCols, 0);
	std::vector<int> newNextgrid(newRows * newCols, 0);

	for (int i = 0; i < newRows; i++)
	{
		for (int j = 0; j < newCols; j++)
		{
			if (i < rows && j < cols)
			{
				newGrid[i * newCols + j] = grid[i * cols + j];
			}
		}
	}

	grid.swap(newGrid);
	nextgrid.swap(newNextgrid);
	cols = newCols;
	rows = newRows;
}

/*
	drawGrid:
		Draws the grid on the screen.
*/
void drawGrid()
{
	for (int i = rows - 1; i >= 0; i--)
	{
		for (int j = 0; j < cols; j++)
		{
			if (grid[i * cols + j] == 1)
			{
				int belowA = 0, belowB = 0, below = 0;
				if (i + 1 < rows)
				{
					if (j - 1 >= 0)
						belowA = grid[(i + 1) * cols + (j - 1)];
					if (j + 1 < cols)
						belowB = grid[(i + 1) * cols + (j + 1)];
					below = grid[(i + 1) * cols + j];
				}

				if (i + 1 < rows && below == 0)
				{
					nextgrid[i * cols + j] = 0;
					nextgrid[(i + 1) * cols + j] = 1;
				}
				else if (i + 1 < rows && belowA == 0 && j - 1 >= 0)
				{
					nextgrid[i * cols + j] = 0;
					nextgrid[(i + 1) * cols + (j - 1)] = 1;
				}
				else if (i + 1 < rows && belowB == 0 && j + 1 < cols)
				{
					nextgrid[i * cols + j] = 0;
					nextgrid[(i + 1) * cols + (j + 1)] = 1;
				}
				else
				{
					nextgrid[i * cols + j] = 1;
				}
			}
		}
	}

	for (int i = 0; i < rows; i++)
	{
		for (int j = 0; j < cols; j++)
		{
			int idx = i * cols + j;
			grid[idx] = nextgrid[idx];
			nextgrid[idx] = 0;
			DrawRectangle(j * cellSize, i * cellSize, cellSize, cellSize,
				grid[idx] == 1 ? BLUE : BLACK);
		}
	}
}

void resetGrid()
{
    std::fill(grid.begin(), grid.end(), 0);
    std::fill(nextgrid.begin(), nextgrid.end(), 0);
}




void handleInput(int rangeSize)
{
    if (IsKeyPressed(KEY_R))
    {
        resetGrid();
    }

	if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
	{
		Vector2 pos = GetMousePosition();
		int x = pos.x / cellSize;
		int y = pos.y / cellSize;
		if (!rangeSize) {
			grid[x + y * cols] = 1;
		} else {
			for (int dy = -rangeSize; dy <= rangeSize; ++dy)
			{
				for (int dx = -rangeSize; dx <= rangeSize; ++dx)
				{
					int nx = x + dx;
					int ny = y + dy;

					if (nx >= 0 && nx < cols && ny >= 0 && ny < rows)
					{
						grid[ny * cols + nx] = 1;
					}
				}
			}
		}
	}
    
	if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON))
	{
		Vector2 pos = GetMousePosition();
		int x = pos.x / cellSize;
		int y = pos.y / cellSize;
		grid[x + y * cols] = 0;
	}

}

int main()
{
	SetConfigFlags(FLAG_WINDOW_RESIZABLE);
	InitWindow(1366, 768, "Falling Sand");
	SetTargetFPS(60);

	initializeGrid(GetScreenWidth(), GetScreenHeight());

	while (!WindowShouldClose())
	{
		if (IsWindowResized())
		{
			resizeGrid(GetScreenWidth() / cellSize, GetScreenHeight() / cellSize);
		}

		BeginDrawing();
		ClearBackground(BLACK);
		handleInput(2);
		drawGrid();
		EndDrawing();
	}

	CloseWindow();
	return 0;
}
