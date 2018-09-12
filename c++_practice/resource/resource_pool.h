#ifndef RESOURCE_POOL_H
#define RESOURCE_POOL_H

#include "resource_pool_inl.h"

template <typename T>
inline T* get_resource(ResourceId<T>* id) {
	return ResourcePool<T>::singleton()->get_resource(id);
}

template <typename T>
inline int return_resource(ResourceId<T> id) {
	return ResourcePool<T>::singleton()->return_resource(id);
}

template <typename T>
inline T* address_resource(ResourceId<T> id) {
	return ResourcePool<T>::address_resource(id);
}

template <typename T>
inline void clear_resource() {
	ResourcePool<T>::singleton()->clear_resource();
}

#endif // RESOURCE_POOL_H
