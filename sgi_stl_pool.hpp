#pragma once
#include <iostream>
#include <mutex>

// 对malloc和free进行了再次封装
// 包含了可以使用oom_handler(用户可以设置回调函数清理一些资源保证malloc再次调用时能够获得堆内存)
template <int __inst>
class __malloc_alloc_template {
private:
    static void (*__malloc_alloc_oom_handler)();
private:
    static void* _S_oom_malloc(size_t __n)
    {
        void (*__my_malloc_handler)();
        void* __result;
        for (;;) 
        {
            __my_malloc_handler = __malloc_alloc_oom_handler;
            if (0 == __my_malloc_handler) { throw std::bad_alloc(); }
            (*__my_malloc_handler)();
            __result = malloc(__n);
            if (__result) return(__result);
        }
    }
    static void* _S_oom_realloc(void* __p, size_t __n)
    {
        void (*__my_malloc_handler)();
        void* __result;
        for (;;) 
        {
            __my_malloc_handler = __malloc_alloc_oom_handler;
            if (0 == __my_malloc_handler) { throw std::bad_alloc(); }
            (*__my_malloc_handler)();
            __result = realloc(__p, __n);
            if (__result) return(__result);
        }
    }
public:
    // 申请内存
    static void* allocate(size_t __n)
    {
        void* __result = malloc(__n);
        if (0 == __result) __result = _S_oom_malloc(__n);
        return __result;
    }
    // 释放内存
    static void deallocate(void* __p, size_t /* __n */)
    {
        free(__p);
    }
    // 内存扩容&缩容
    static void* reallocate(void* __p, size_t /* old_sz */, size_t __new_sz)
    {
        void* __result = realloc(__p, __new_sz);
        if (0 == __result) __result = _S_oom_realloc(__p, __new_sz);
        return __result;
    }
    // 设置oom_handler
    static void (*__set_malloc_handler(void (*__f)()))()
    {
        void (*__old)() = __malloc_alloc_oom_handler;
        __malloc_alloc_oom_handler = __f;
        return(__old);
    }
};
template <int __inst>
void (*__malloc_alloc_template<__inst>::__malloc_alloc_oom_handler)() = 0;
typedef __malloc_alloc_template<0> malloc_alloc;

// ------------------------------------------------------------------
// 
// SGI-STL二级空间配置器，自由链表实现的内存池
template <typename _Ty>
class myallocator
{
public:
    // 需要C++STL库中allocator一些固定格式才能将myallocator使用在容器上
    using value_type = _Ty;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    constexpr myallocator() noexcept {}

    constexpr myallocator(const myallocator&) noexcept = default;
    template <class _Other>          // 构造函数模板，可转换成任意类型构造
    constexpr myallocator(const myallocator<_Other>&) noexcept {}
     ~myallocator() = default;
     myallocator& operator=(const myallocator&) = default;
public:
    // 提供给外部的接口  模样为标准空间配置器
    void construct(_Ty* __p, const _Ty& __val) { new(__p) _Ty(__val); }
    void destroy(_Ty* __p) { __p->~_Ty(); }
    _Ty* allocate(size_t __n) { return myallocate(__n * sizeof(_Ty)); }
    void deallocate(void* __p, size_t __n) { mydeallocate(__p, __n * sizeof(_Ty)); }
private:
    union _Obj
    {
        union _Obj* _M_free_list_link;
        char _M_client_data[1];
    };
    enum { _ALIGN = 8 };
    enum { _MAX_BYTES = 128 };
    enum { _NFREELISTS = 16 };
    static _Obj* volatile _S_free_list[_NFREELISTS];  // 小块内存分配的内存池，自由链表数组

    static char* _S_start_free;     // 备用内存开始位置
    static char* _S_end_free;       // 备用内存结束位置
    static size_t _S_heap_size;     // 共向操作系统申请过的多少空间补充给内存池
    static std::mutex mtx;          // 为线程安全

    static size_t _S_round_up(size_t __bytes) { return (((__bytes)+(size_t)_ALIGN - 1) & ~((size_t)_ALIGN - 1)); }
    static  size_t _S_freelist_index(size_t __bytes) { return (((__bytes)+(size_t)_ALIGN - 1) / (size_t)_ALIGN - 1); }

    // 分配一个chunk块，并在分配的chunk中创建成自由链表
    static void* _S_refill(size_t __n);
    // 分配一个chunk块
    static char* _S_chunk_alloc(size_t __size, int& __nobjs);
private:
	// 申请内存
    _Ty* myallocate(size_t __n)
    {
        void* __ret = 0;
        // 大于128字节，则申请的是大块内存，使用malloc
        if (__n > (size_t)_MAX_BYTES) 
        {
            __ret = malloc_alloc::allocate(__n);
        }
        // 128字节以内，用自由链表内存池形式管理
        else 
        {   
            _Obj* volatile * __my_free_list = _S_free_list + _S_freelist_index(__n);
            // 自由链表保证线程安全
            std::unique_lock<std::mutex>(mtx);
            //如果当前自由链表节点为null,则指向新申请一块内存,这块内存通过静态链表连接在一起
            _Obj* __result = *__my_free_list;
            if (__result == 0) 
            {
                __ret = _S_refill(_S_round_up(__n));
            }
            else 
            {
                *__my_free_list = __result->_M_free_list_link;
                __ret = __result;
            }
        }
        return static_cast<_Ty*>(__ret);
    }

	// 释放内存
    void mydeallocate(void* __p, size_t __n)
    {
        // 超过128字节(不是从池中拿内存),直接free
        if (__n > (size_t)_MAX_BYTES)
            malloc_alloc::deallocate(__p, __n);
        else 
        {
            // 定位到自由链表对应节点,将释放的内存挂载到其静态链表上
            _Obj* volatile* __my_free_list = _S_free_list + _S_freelist_index(__n);
            _Obj* __q = (_Obj*)__p;
            // 保证线程安全
            std::unique_lock<std::mutex>(mtx);
            __q->_M_free_list_link = *__my_free_list;
            *__my_free_list = __q;
        }
    }

	// 内存扩容&缩容
    void* reallocate(void* __p, size_t __old_sz, size_t __new_sz)
    {
        void* __result;
        size_t __copy_sz;
        // 原本的空间且想要扩展的新空间大小 大于128字节,直接使用realloc,不使用池
        if (__old_sz > (size_t)_MAX_BYTES && __new_sz > (size_t)_MAX_BYTES) 
        {
            return (realloc(__p, __new_sz));
        }
        // 原本大小与新大小在同一区间(13与15),直接返回
        if (_S_round_up(__old_sz) == _S_round_up(__new_sz)) return(__p);
        // 没有上述情况则：获取池中内存,拷贝,归还池内存
        __result = myallocate(__new_sz);
        __copy_sz = __new_sz > __old_sz ? __old_sz : __new_sz;
        memcpy(__result, __p, __copy_sz);
        mydeallocate(__p, __old_sz);
        return(__result);
    }
};
template <typename _Ty>
typename myallocator<_Ty>::_Obj* volatile myallocator<_Ty>::_S_free_list[_NFREELISTS] = { nullptr };

template <typename _Ty>
char* myallocator<_Ty>:: _S_start_free = nullptr;

template <typename _Ty>
char* myallocator<_Ty>::_S_end_free = nullptr;

template <typename _Ty>
size_t myallocator<_Ty>::_S_heap_size = 0;

template <typename _Ty>
std::mutex myallocator<_Ty>::mtx;

// ------------------------------------------------------------------

// 分配一个chunk块，并在分配的chunk中创建成自由链表
template<typename _Ty>
void* myallocator<_Ty>::_S_refill(size_t __n)
{
    int __nobjs = 20;
    // 分配一个chunk块
    char* __chunk = _S_chunk_alloc(__n, __nobjs); // jobs引用传递
    _Obj* volatile* __my_free_list;
    _Obj* __result;
    _Obj* __current_obj;
    _Obj* __next_obj;
    int __i;

    if (1 == __nobjs) return(__chunk);
    __my_free_list = _S_free_list + _S_freelist_index(__n);

    // 在分配的chunk中创建成自由链表
    __result = (_Obj*)__chunk;
    *__my_free_list = __next_obj = (_Obj*)(__chunk + __n);
    for (__i = 1; ; __i++) 
    {
        __current_obj = __next_obj;
        __next_obj = (_Obj*)((char*)__next_obj + __n);
        if (__nobjs - 1 == __i) 
        {
            __current_obj->_M_free_list_link = 0;
            break;
        }
        else 
        {
            __current_obj->_M_free_list_link = __next_obj;
        }
    }
    return(__result);
}

// 小块内存不够，申请chunk块
template<typename _Ty>
inline char* myallocator<_Ty>::_S_chunk_alloc(size_t __size, int& __nobjs)
{
    // 每个第一次申请都是申请__size * __nobjs的2倍作为备用内存
    char* __result;
    size_t __total_bytes = __size * __nobjs;

    // 关键变量__bytes_left，为剩余备用的内存，备用内存等于_S_end_free- _S_start_free;
    size_t __bytes_left = _S_end_free - _S_start_free;

    // 如果备用内存大于等于所需要申请的chunk块(20份)，直接给，减小备用内存范围
    if (__bytes_left >= __total_bytes)
    {
        __result = _S_start_free;
        _S_start_free += __total_bytes;
        return(__result);
    }

    // 如果备用内存大于等于要申请内存最低的chunk块(1份),按能最大的给，减小备用内存范围
    else if (__bytes_left >= __size)
    {
        __nobjs = (int)(__bytes_left / __size);     // 最多几份,__nobjs
        __total_bytes = __size * __nobjs;
        __result = _S_start_free;
        _S_start_free += __total_bytes;
        return(__result);
    }
    else 
    {
        size_t __bytes_to_get = 2 * __total_bytes + _S_round_up(_S_heap_size >> 4);

        // 还有多的备用内存，不浪费，而是挂载到对应字节数的静态链表上
        if (__bytes_left > 0) 
        {
            _Obj* volatile* __my_free_list = _S_free_list + _S_freelist_index(__bytes_left);
            ((_Obj*)_S_start_free)->_M_free_list_link = *__my_free_list;
            *__my_free_list = (_Obj*)_S_start_free;
        }
        _S_start_free = (char*)malloc(__bytes_to_get);

        // 申请内存异常
        if (0 == _S_start_free) 
        {	
            size_t __i;
            _Obj* volatile* __my_free_list;
            _Obj* __p;
            for (__i = __size; __i <= (size_t)_MAX_BYTES; __i += (size_t)_ALIGN) 
            {
                __my_free_list = _S_free_list + _S_freelist_index(__i);
                __p = *__my_free_list;
                if (0 != __p) 
                {
                    *__my_free_list = __p->_M_free_list_link;
                    _S_start_free = (char*)__p;
                    _S_end_free = _S_start_free + __i;
                    return(_S_chunk_alloc(__size, __nobjs));
                }
            }
            _S_end_free = 0;
            _S_start_free = (char*)malloc_alloc::allocate(__bytes_to_get);
        }
        _S_heap_size += __bytes_to_get;
        _S_end_free = _S_start_free + __bytes_to_get;
        return(_S_chunk_alloc(__size, __nobjs));
    }
}