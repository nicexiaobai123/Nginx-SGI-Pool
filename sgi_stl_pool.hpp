#pragma once
#include <iostream>
#include <mutex>

// ��malloc��free�������ٴη�װ
// �����˿���ʹ��oom_handler(�û��������ûص���������һЩ��Դ��֤malloc�ٴε���ʱ�ܹ���ö��ڴ�)
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
    // �����ڴ�
    static void* allocate(size_t __n)
    {
        void* __result = malloc(__n);
        if (0 == __result) __result = _S_oom_malloc(__n);
        return __result;
    }
    // �ͷ��ڴ�
    static void deallocate(void* __p, size_t /* __n */)
    {
        free(__p);
    }
    // �ڴ�����&����
    static void* reallocate(void* __p, size_t /* old_sz */, size_t __new_sz)
    {
        void* __result = realloc(__p, __new_sz);
        if (0 == __result) __result = _S_oom_realloc(__p, __new_sz);
        return __result;
    }
    // ����oom_handler
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
// SGI-STL�����ռ�����������������ʵ�ֵ��ڴ��
template <typename _Ty>
class myallocator
{
public:
    // ��ҪC++STL����allocatorһЩ�̶���ʽ���ܽ�myallocatorʹ����������
    using value_type = _Ty;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    constexpr myallocator() noexcept {}

    constexpr myallocator(const myallocator&) noexcept = default;
    template <class _Other>          // ���캯��ģ�壬��ת�����������͹���
    constexpr myallocator(const myallocator<_Other>&) noexcept {}
     ~myallocator() = default;
     myallocator& operator=(const myallocator&) = default;
public:
    // �ṩ���ⲿ�Ľӿ�  ģ��Ϊ��׼�ռ�������
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
    static _Obj* volatile _S_free_list[_NFREELISTS];  // С���ڴ������ڴ�أ�������������

    static char* _S_start_free;     // �����ڴ濪ʼλ��
    static char* _S_end_free;       // �����ڴ����λ��
    static size_t _S_heap_size;     // �������ϵͳ������Ķ��ٿռ䲹����ڴ��
    static std::mutex mtx;          // Ϊ�̰߳�ȫ

    static size_t _S_round_up(size_t __bytes) { return (((__bytes)+(size_t)_ALIGN - 1) & ~((size_t)_ALIGN - 1)); }
    static  size_t _S_freelist_index(size_t __bytes) { return (((__bytes)+(size_t)_ALIGN - 1) / (size_t)_ALIGN - 1); }

    // ����һ��chunk�飬���ڷ����chunk�д�������������
    static void* _S_refill(size_t __n);
    // ����һ��chunk��
    static char* _S_chunk_alloc(size_t __size, int& __nobjs);
private:
	// �����ڴ�
    _Ty* myallocate(size_t __n)
    {
        void* __ret = 0;
        // ����128�ֽڣ���������Ǵ���ڴ棬ʹ��malloc
        if (__n > (size_t)_MAX_BYTES) 
        {
            __ret = malloc_alloc::allocate(__n);
        }
        // 128�ֽ����ڣ������������ڴ����ʽ����
        else 
        {   
            _Obj* volatile * __my_free_list = _S_free_list + _S_freelist_index(__n);
            // ��������֤�̰߳�ȫ
            std::unique_lock<std::mutex>(mtx);
            //�����ǰ��������ڵ�Ϊnull,��ָ��������һ���ڴ�,����ڴ�ͨ����̬����������һ��
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

	// �ͷ��ڴ�
    void mydeallocate(void* __p, size_t __n)
    {
        // ����128�ֽ�(���Ǵӳ������ڴ�),ֱ��free
        if (__n > (size_t)_MAX_BYTES)
            malloc_alloc::deallocate(__p, __n);
        else 
        {
            // ��λ�����������Ӧ�ڵ�,���ͷŵ��ڴ���ص��侲̬������
            _Obj* volatile* __my_free_list = _S_free_list + _S_freelist_index(__n);
            _Obj* __q = (_Obj*)__p;
            // ��֤�̰߳�ȫ
            std::unique_lock<std::mutex>(mtx);
            __q->_M_free_list_link = *__my_free_list;
            *__my_free_list = __q;
        }
    }

	// �ڴ�����&����
    void* reallocate(void* __p, size_t __old_sz, size_t __new_sz)
    {
        void* __result;
        size_t __copy_sz;
        // ԭ���Ŀռ�����Ҫ��չ���¿ռ��С ����128�ֽ�,ֱ��ʹ��realloc,��ʹ�ó�
        if (__old_sz > (size_t)_MAX_BYTES && __new_sz > (size_t)_MAX_BYTES) 
        {
            return (realloc(__p, __new_sz));
        }
        // ԭ����С���´�С��ͬһ����(13��15),ֱ�ӷ���
        if (_S_round_up(__old_sz) == _S_round_up(__new_sz)) return(__p);
        // û����������򣺻�ȡ�����ڴ�,����,�黹���ڴ�
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

// ����һ��chunk�飬���ڷ����chunk�д�������������
template<typename _Ty>
void* myallocator<_Ty>::_S_refill(size_t __n)
{
    int __nobjs = 20;
    // ����һ��chunk��
    char* __chunk = _S_chunk_alloc(__n, __nobjs); // jobs���ô���
    _Obj* volatile* __my_free_list;
    _Obj* __result;
    _Obj* __current_obj;
    _Obj* __next_obj;
    int __i;

    if (1 == __nobjs) return(__chunk);
    __my_free_list = _S_free_list + _S_freelist_index(__n);

    // �ڷ����chunk�д�������������
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

// С���ڴ治��������chunk��
template<typename _Ty>
inline char* myallocator<_Ty>::_S_chunk_alloc(size_t __size, int& __nobjs)
{
    // ÿ����һ�����붼������__size * __nobjs��2����Ϊ�����ڴ�
    char* __result;
    size_t __total_bytes = __size * __nobjs;

    // �ؼ�����__bytes_left��Ϊʣ�౸�õ��ڴ棬�����ڴ����_S_end_free- _S_start_free;
    size_t __bytes_left = _S_end_free - _S_start_free;

    // ��������ڴ���ڵ�������Ҫ�����chunk��(20��)��ֱ�Ӹ�����С�����ڴ淶Χ
    if (__bytes_left >= __total_bytes)
    {
        __result = _S_start_free;
        _S_start_free += __total_bytes;
        return(__result);
    }

    // ��������ڴ���ڵ���Ҫ�����ڴ���͵�chunk��(1��),�������ĸ�����С�����ڴ淶Χ
    else if (__bytes_left >= __size)
    {
        __nobjs = (int)(__bytes_left / __size);     // ��༸��,__nobjs
        __total_bytes = __size * __nobjs;
        __result = _S_start_free;
        _S_start_free += __total_bytes;
        return(__result);
    }
    else 
    {
        size_t __bytes_to_get = 2 * __total_bytes + _S_round_up(_S_heap_size >> 4);

        // ���ж�ı����ڴ棬���˷ѣ����ǹ��ص���Ӧ�ֽ����ľ�̬������
        if (__bytes_left > 0) 
        {
            _Obj* volatile* __my_free_list = _S_free_list + _S_freelist_index(__bytes_left);
            ((_Obj*)_S_start_free)->_M_free_list_link = *__my_free_list;
            *__my_free_list = (_Obj*)_S_start_free;
        }
        _S_start_free = (char*)malloc(__bytes_to_get);

        // �����ڴ��쳣
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