#include <vector>

class Entity
{
protected:
    struct Hitbox
    {
        int width;
        int height;
    } hitbox;

    int posX = 0;
    int posY = 0;

public:
    std::vector<int> getPosition() const
    {
        return { posX, posY };
    }

    int getPosX() const
    {
        return posX;
    }

    int getPosY() const
    {
        return posY;
    }

    void setPosition(int x, int y)
    {
        posX = x;
        posY = y;
    }

    void setPosY(int y)
    {
        posY = y;
    }

    void setPosX(int x)
    {
        posX = x;
    }

    void setHitbox(int width, int height)
    {
        hitbox = { width, height };
    }

    Hitbox getHitbox() const
    {
        return hitbox;
    }
};