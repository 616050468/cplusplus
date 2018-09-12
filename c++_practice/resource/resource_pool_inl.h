#ifndef RESOURCE_POOL_INL_H
#define RESOURCE_POOL_INL_H

#include <pthread.h>
#include <thread>
#include <algorithm>
#include <vector>
#include <atomic>
#include <string.h>
#include <stdio.h>

#define BAIDU_CACHELINE_SIZE 64

#ifdef _MSC_VER
# define BAIDU_CACHELINE_ALIGNMENT __declspec(align(BAIDU_CACHELINE_SIZE))
#endif /* _MSC_VER */

#ifdef __GNUC__
# define BAIDU_CACHELINE_ALIGNMENT __attribute__((aligned(BAIDU_CACHELINE_SIZE)))
#endif /* __GNUC__ */

#ifndef BAIDU_CACHELINE_ALIGNMENT
# define BAIDU_CACHELINE_ALIGNMENT /*BAIDU_CACHELINE_ALIGNMENT*/
#endif

template <typename T>
struct ResourceId {
    uint64_t value;
    
	operator uint64_t() const {
		return value;
	}

	template <typename T2>
	ResourceId<T2> cast() const {
		ResourceId<T2> id = { value };
		return id;
	}
};

template <typename T, size_t NITEM>
struct ResourcePoolFreeChunk {
	size_t nfree;
	ResourceId<T> ids[NITEM];
};

static const size_t RP_MAX_BLOCK_NGROUP = 65536;
static const size_t RP_GROUP_NBLOCK_NBIT = 16;
static const size_t RP_GROUP_NBLOCK = (1UL << RP_GROUP_NBLOCK_NBIT);
static const size_t RP_INITIAL_FREE_LIST_SIZE = 1024;

template <typename T>
struct ResourcePoolBlockMaxSize {
	static const size_t value = 64*1024;
};

template <typename T>
struct ResourcePoolBlockMaxItem {
	static const size_t value = 256;
};

template <typename T>
struct ResourcePoolFreeChunkMaxItem {
	static size_t value() {
		return 256;
	}
};

template <typename T>
struct ResourcePoolValidator {
	static bool validate(const T*) { return true; }
};

template <typename T>
struct ResourcePoolBlockItemNum {
	static const size_t N1 = ResourcePoolBlockMaxSize<T>::value / sizeof(T);
	static const size_t N2 = N1 < 1 ? 1 : N1;
	static const size_t value = (N2 > ResourcePoolBlockMaxItem<T>::value ?
								ResourcePoolBlockMaxItem<T>::value : N2);
};

template <typename T>
class BAIDU_CACHELINE_ALIGNMENT ResourcePool {
public:
	static const size_t BLOCK_NITEM = ResourcePoolBlockItemNum<T>::value;
	static const size_t FREE_CHUNK_NITEM = BLOCK_NITEM;

	typedef ResourcePoolFreeChunk<T, FREE_CHUNK_NITEM> FreeChunk;
	typedef ResourcePoolFreeChunk<T, 1> DynamicFreeChunk;

	struct BAIDU_CACHELINE_ALIGNMENT Block {
		size_t nitem;
		char items[sizeof(T) * BLOCK_NITEM];

		Block(): nitem(0) {}
	};

	struct BlockGroup {
		std::atomic<size_t> nblock;
		std::atomic<Block*> blocks[RP_GROUP_NBLOCK];

		BlockGroup(): nblock(0) {
			memset(blocks, 0, sizeof(std::atomic<Block*>) * RP_GROUP_NBLOCK);
		}
	};

	class BAIDU_CACHELINE_ALIGNMENT LocalPool {
	public:
		explicit LocalPool(ResourcePool* pool)
			: _pool(pool)
			, _cur_block(NULL)
			, _cur_block_index(0) {
			_cur_free.nfree = 0;
		}
        
		~LocalPool() {
			if (_cur_free.nfree > 0) {
				_pool->push_free_chunk(_cur_free);
			}
			_pool->clear_from_destructor_of_local_pool();
		}

#define RESOURCE_POOL_GET(CTOR_ARGS)					\
	 	if (_cur_free.nfree > 0) {                      \
			*id = _cur_free.ids[--_cur_free.nfree];     \
			return unsafe_address_resource(*id);        \
		}                                               \
	 	if (_pool->pop_free_chunk(_cur_free)) {         \
			*id = _cur_free.ids[--_cur_free.nfree];     \
			return unsafe_address_resource(*id);        \
	 	}                                               \
		if (_cur_block && _cur_block->nitem < BLOCK_NITEM) { \
			id->value = _cur_block_index*BLOCK_NITEM + _cur_block->nitem; \
			T* p = new((T*)_cur_block->items+_cur_block->nitem) T CTOR_ARGS; \
			++_cur_block->nitem;						\
			return p;									\
		}												\
		_cur_block = add_block(&_cur_block_index);		\
		if (_cur_block) {								\
			id->value = _cur_block_index*BLOCK_NITEM + _cur_block->nitem; \
			T* p = new((T*)_cur_block->items+_cur_block->nitem) T CTOR_ARGS; \
			++_cur_block->nitem;							\
			return p;									\
		}												\
	 	return NULL

		inline T* get(ResourceId<T>* id) {
			RESOURCE_POOL_GET();
		}

#undef RESOURCE_POOL_GET

		inline int return_resource(ResourceId<T> id) {
			if (_cur_free.nfree >= ResourcePool::free_chunk_nitem()) {
				if (!_pool->push_free_chunk(_cur_free)) {
					return -1;
				}
				_cur_free.nfree = 0;
			}
			_cur_free.ids[_cur_free.nfree++] = id;
			return 0;
		}

	private:
		ResourcePool* _pool;
		Block* _cur_block;
		size_t _cur_block_index;
		FreeChunk _cur_free;
	};

	class ExitHelper {
	public:
		ExitHelper()
			:lp(NULL) {
			//printf("ExitHelper\n");
		}
		~ExitHelper() {
			if (lp) {
				delete lp;
			}
			//printf("~ExitHelper\n");
		}
		void set_local_pool(LocalPool* lp) {
			this->lp = lp;
		}
	private:
		LocalPool* lp;
	};

	static inline T* unsafe_address_resource(ResourceId<T> id) {
		size_t block_index = id.value / BLOCK_NITEM;
		return (T*)(_block_groups[block_index >> RP_GROUP_NBLOCK_NBIT]
					.load(std::memory_order_consume)
					->blocks[block_index & (RP_GROUP_NBLOCK - 1)]
					.load(std::memory_order_consume)->items) +
				id.value - block_index * BLOCK_NITEM;
	}

	static inline T* address_resource(ResourceId<T> id) {
		size_t block_index = id.value / BLOCK_NITEM;
		size_t group_index = block_index >> RP_GROUP_NBLOCK_NBIT;
		if (group_index > RP_MAX_BLOCK_NGROUP - 1) {
			return NULL;
		}
		BlockGroup* bg = _block_groups[group_index].load(std::memory_order_consume);
		if (bg == NULL) {
			return NULL;
		}
		Block* block = bg->blocks[block_index & (RP_GROUP_NBLOCK - 1)].load(std::memory_order_consume);
		if (block == NULL) {
			return NULL;
		}
		return (T*)block->items + id.value - block_index * BLOCK_NITEM;
	}

	inline T* get_resource(ResourceId<T>* id) {
		LocalPool* lp = get_or_new_local_pool();
		if (__builtin_expect(lp!=NULL, 1)) {
			return lp->get(id);
		}
		return NULL;
	}

	inline int return_resource(ResourceId<T> id) {
		LocalPool* lp = get_or_new_local_pool();
		if (__builtin_expect(lp!=NULL, 1)) {
			return lp->return_resource(id);
		}
		return -1;
	}

	void clear_resource() {
		LocalPool* lp = _local_pool;
		if (lp) {
			_local_pool = NULL;
			_exit_helper.set_local_pool(NULL);
			delete lp;
		}
	}

	static inline size_t free_chunk_nitem() {
		const size_t n = ResourcePoolFreeChunkMaxItem<T>::value();
		return n < FREE_CHUNK_NITEM ? n : FREE_CHUNK_NITEM;
	}

	static inline ResourcePool* singleton() {
		if (_singleton != NULL) {
			return _singleton;
		}
		pthread_mutex_lock(&_singleton_mutex);
		if (_singleton == NULL) {
			_singleton = new ResourcePool();
		}
		pthread_mutex_unlock(&_singleton_mutex);
		return _singleton;
	}

private:
	ResourcePool() {
		_free_chunks.reserve(RP_INITIAL_FREE_LIST_SIZE);
		pthread_mutex_init(&_free_chunks_mutex, NULL);
	}

	~ResourcePool() {
		pthread_mutex_destroy(&_free_chunks_mutex);
	}

	inline LocalPool* get_or_new_local_pool() {
		//static thread_local ExitHelper exit_helper;
		LocalPool* lp = _local_pool;
		if (lp != NULL) {
			return lp;
		}
		lp = new(std::nothrow) LocalPool(this);
		if (lp == NULL) {
			return NULL;
		}
		pthread_mutex_lock(&_clear_mutex);
		_local_pool = lp;
		_exit_helper.set_local_pool(lp);
		_nlocal.fetch_add(1, std::memory_order_relaxed);
		pthread_mutex_unlock(&_clear_mutex);
		return lp;
	}

	void clear_from_destructor_of_local_pool() {
		_local_pool = NULL;
		if (_nlocal.fetch_sub(1, std::memory_order_relaxed) != 1) {
			return;
		}
		pthread_mutex_lock(&_clear_mutex);
		if (_nlocal.load(std::memory_order_relaxed) != 0) {
			pthread_mutex_unlock(&_clear_mutex);
			return;
		}
		// all local_pool destoried
		printf("clear_all_resource\n");
		FreeChunk c;
		while (pop_free_chunk(c));

		const size_t ngroup = _ngroup.exchange(0, std::memory_order_relaxed);
		for (size_t i=0; i<ngroup; ++i) {
			BlockGroup* bg = _block_groups[i].load(std::memory_order_relaxed);
			if (bg == NULL)
				continue;

			const size_t nblock = bg->nblock.load(std::memory_order_relaxed);
			for (size_t j=0; j<nblock; ++j) {
				Block* b = bg->blocks[j].load(std::memory_order_relaxed);
				if (b == NULL)
					continue;
				for (size_t n=0; n<b->nitem; ++n) {
					T* obj = (T*)b->items + n;
					obj->~T();
				}
				delete b;
			}
			delete bg;
		}
		memset(_block_groups, 0, sizeof(BlockGroup*) * RP_MAX_BLOCK_NGROUP);
		pthread_mutex_unlock(&_clear_mutex);
	}

private:
	bool pop_free_chunk(FreeChunk& c) {
		if (_free_chunks.empty()) {
			return false;
		}
		pthread_mutex_lock(&_free_chunks_mutex);
		if (_free_chunks.empty()) {
			pthread_mutex_unlock(&_free_chunks_mutex);
			return false;
		}
		DynamicFreeChunk* p = _free_chunks.back();
		_free_chunks.pop_back();
		pthread_mutex_unlock(&_free_chunks_mutex);
		c.nfree = p->nfree;
		memcpy(c.ids, p->ids, sizeof(*p->ids) * p->nfree);
		free(p);
		return true;
	}

	bool push_free_chunk(const FreeChunk& c) {
		DynamicFreeChunk* p = (DynamicFreeChunk*)malloc(
				sizeof(DynamicFreeChunk) + sizeof(*c.ids) * (c.nfree-1));
		if (p == NULL) {
			return false;
		}
		p->nfree = c.nfree;
		memcpy(p->ids, c.ids, sizeof(*c.ids) * c.nfree);
		pthread_mutex_lock(&_free_chunks_mutex);
		_free_chunks.push_back(p);
		pthread_mutex_unlock(&_free_chunks_mutex);
		return true;
	}

	static Block* add_block(size_t* index) {
		Block* const new_block = new(std::nothrow) Block();
		if (new_block == NULL) {
			return NULL;
		}
		size_t ngroup;
		do {
			ngroup = _ngroup.load(std::memory_order_acquire);
			if (ngroup > 0) {
				BlockGroup* g = _block_groups[ngroup-1].load(std::memory_order_acquire);
				size_t nblock = g->nblock.fetch_add(1, std::memory_order_relaxed);
				if (nblock < RP_GROUP_NBLOCK) {
					g->blocks[nblock].store(new_block, std::memory_order_release);
					*index = (ngroup - 1) * RP_GROUP_NBLOCK + nblock;
					return new_block;
				}
				// full
				g->nblock.fetch_sub(1, std::memory_order_relaxed);		
			}
		} while(add_block_group(ngroup));

		delete new_block;
		return NULL;
	}

	static bool add_block_group(size_t old_group) {
		if (old_group >= RP_MAX_BLOCK_NGROUP) {
			return false;
		}
		bool is_ok = true;
		pthread_mutex_lock(&_block_groups_mutex);
		size_t ngroup = _ngroup.load(std::memory_order_acquire);
		if (ngroup == old_group) {
			BlockGroup* g = new(std::nothrow) BlockGroup();
			if (g == NULL) {
				is_ok = false;
			}
			else {
				_block_groups[ngroup].store(g, std::memory_order_release);
				_ngroup.store(ngroup+1, std::memory_order_release);
			}
		}
		pthread_mutex_unlock(&_block_groups_mutex);
		return is_ok;
	}

	static ResourcePool* _singleton;
	static pthread_mutex_t _singleton_mutex;
	static thread_local LocalPool* _local_pool;
	static std::atomic<long> _nlocal;
	static pthread_mutex_t _clear_mutex;
	static thread_local ExitHelper _exit_helper;

	static std::atomic<size_t> _ngroup;
	static std::atomic<BlockGroup*> _block_groups[RP_MAX_BLOCK_NGROUP];
	static pthread_mutex_t _block_groups_mutex;
	
	std::vector<DynamicFreeChunk*> _free_chunks;
	pthread_mutex_t _free_chunks_mutex;

	static std::atomic<size_t> _global_nfree;

};


template <typename T>
ResourcePool<T>* ResourcePool<T>::_singleton = NULL;

template <typename T>
pthread_mutex_t ResourcePool<T>::_singleton_mutex = PTHREAD_MUTEX_INITIALIZER;

template <typename T>
thread_local typename ResourcePool<T>::LocalPool* ResourcePool<T>::_local_pool = NULL;

template <typename T>
thread_local typename ResourcePool<T>::ExitHelper ResourcePool<T>::_exit_helper;

template <typename T>
std::atomic<long> ResourcePool<T>::_nlocal = { 0 };

template <typename T>
pthread_mutex_t ResourcePool<T>::_clear_mutex = PTHREAD_MUTEX_INITIALIZER;

template <typename T>
std::atomic<size_t> ResourcePool<T>::_ngroup = { 0 };

template <typename T>
std::atomic<typename ResourcePool<T>::BlockGroup*> 
ResourcePool<T>::_block_groups[RP_MAX_BLOCK_NGROUP] = {};

template <typename T>
pthread_mutex_t ResourcePool<T>::_block_groups_mutex = PTHREAD_MUTEX_INITIALIZER;

template <typename T>
std::atomic<size_t> ResourcePool<T>::_global_nfree = { 0 };

#endif // RESOURCE_POOL_INL_H









