#include "../../constants.h"
class Rock : public Object
{
    public:
        Rock(int x, int y) : Object(x, y)
        {
            setHitbox(0, 0);
        }
        
        void update() const
        {
            velY += GRAVITY;
            if (velY > MAX_FALL)
            {
                velY = MAX_FALL;
            }
            double nextX = posX + velX;
            double nextY = posY + velY;
            if (nextX >= 0 && nextX < grid.cols && nextY >= 0 && nextY < grid.rows && !props(grid.cells[grid.idx(nextY, nextX)].material).blocks)
            {
                posX = nextX;
                posY = nextY;
            }
        }
    private:
        bool checkCollision(int x, int y) const 
        {
            // Sample points along the bottom/edges of the hitbox x/y
            // Convert world floats to your integer grid coordinates (e.g., target_x // CELL_SIZE)
            // Return True if any tested grid cell contains Sand (1) or Wall (2)
            for (int i = 0; i < hitbox.width; i++)
            {
                for (int j = 0; j < hitbox.height; j++)
                {
                    int testX = x + i * CELL_SIZE;
                    int testY = y + j * CELL_SIZE;
                    if (testX >= 0 && testX < grid.cols && testY >= 0 && testY < grid.rows && !props(grid.cells[grid.idx(testY, testX)].material).blocks)
                    {
                        if (props(grid.cells[grid.idx(testY, testX)].material).blocks)
                        {
                            return false;
                        }
                        else
                        {
                            return true;
                        }
                    }
                }
            }
            return false;
        }
};