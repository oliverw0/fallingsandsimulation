
class Object : public Entity
{
    protected:
        int rotation; // Current rotation state of the object (0, 90, 180, 270 degrees)
        double velX, velY; // Velocity of the object for movement and physics
    public:
        Object(int x, int y) : posX(x), posY(y), rotation(0), velX(0), velY(0) {}\
        
        void draw() const
        {
            // posX and posY are representative of it's central position
            // Hitbox is used to determine the size of the object for drawing and collision
            int drawX = posX - hitbox.width / 2;
            int drawY = posY - hitbox.height / 2;
            DrawRectangle(drawX * cellSize, drawY * cellSize, hitbox.width * cellSize, hitbox.height * cellSize, GRAY);
        }
}