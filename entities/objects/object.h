
class Object : public Entity
{
    protected:
        int rotation; // Current rotation state of the object (0, 90, 180, 270 degrees)
        double velX, velY; // Velocity of the object for movement and physics
    public:
        Object(int x, int y) : posX(x), posY(y), rotation(0), velX(0), velY(0) {}\
        
        void update() const;
}