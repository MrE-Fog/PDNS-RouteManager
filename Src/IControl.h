#ifndef ICONTROL_H
#define ICONTROL_H

class IControl
{
    public:
        virtual void Shutdown(int ec) = 0; //must be threadsafe
};

#endif // ICONTROL_H

