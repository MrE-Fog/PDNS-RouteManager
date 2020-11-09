#ifndef IMMUTABLESTORAGE_H
#define IMMUTABLESTORAGE_H

#include <vector>

template <class T> class ImmutableStorage
{
    private:
        std::vector<T> storage;
    public:
        bool isUpdated;
        ImmutableStorage(const T &payload) {storage.push_back(payload);isUpdated=false;}
        T Get() const {return storage.at(0);}
        void Set(const T &payload) {storage.clear();storage.push_back(payload);isUpdated=true;}

        ImmutableStorage<T>& operator=(const T &payload) {Set(payload); return *this;}
};

#endif // IMMUTABLESTORAGE_H
