#ifndef IMMUTABLESTORAGE_H
#define IMMUTABLESTORAGE_H

#include <vector>

template <class T> class ImmutableStorage
{
    private:
        std::vector<T> storage;
    public:
        ImmutableStorage(const T &payload) {storage.push_back(payload);}
        T Get() const { return storage.at(0); }
        void Store(const T &payload) { storage.clear(); storage.push_back(payload); }
        ImmutableStorage<T>& operator=(const T &payload) {Store(payload); return *this;}
};

#endif // IMMUTABLESTORAGE_H
