
class Rock : public Object
{
    public:
        Rock(int x, int y) : Object(x, y)
        {
            setHitbox(0, 0);
            draw();
        }
        

}