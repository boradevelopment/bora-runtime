#pragma once

struct IResourceHandle {
    virtual ~IResourceHandle() = default;
    virtual void Release() = 0;
    virtual bool Exists() const = 0;
};

enum ResourceStatus {
    NOTHING = 0,
    CREATED = 1,
    DESTROYED = 2,
    TO_BE_CREATED = 3,
    TO_BE_DESTROYED = 4,
};

template<typename T>
class ResourceHandle : public IResourceHandle {
    T* resource = nullptr;
    T* prevResource = nullptr;
public:
    ResourceHandle() : resource(nullptr), prevResource(nullptr) {

    }

    ResourceHandle(T* res) {
        Resolve(res);
    }

    ResourceStatus status = ResourceStatus::NOTHING;
    void Resolve(T* res) {
        if (resource) {
            delete resource;
            resource = nullptr;
        }
        status = CREATED;
        resource = res;
    }

    T* Get() const { return resource; }
    void MoveAsPrevious() {
        if (prevResource)
            delete prevResource; // delete previous one if it existed

        prevResource = resource; // copy pointer
        resource = nullptr;
    };
    T** GetAddress() { return &resource; }
    T** GetPreviousResource() { return &prevResource; };


    void DestroyHandle() {
        delete this;
    }
    bool Exists() const override {
        return status == CREATED;
    }

    bool PlannedToExist() const {
        return status == CREATED || status == TO_BE_CREATED;
    }

    T* operator->() {
        return resource;
    }

    operator T* () const { return resource; }
    operator T** () { return &resource; }
    operator bool() { return Exists(); }

    void Reset() {
        resource = nullptr;
    }

    void Invalidate() {
        status = DESTROYED;
        if (resource) {
            delete resource;
            resource = nullptr;
        }
    }

    void Release() override {
        status = DESTROYED;
        if (resource) {
            delete resource;
            resource = nullptr;
        }
    }
};