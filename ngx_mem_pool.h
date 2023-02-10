#pragma once
#include <iostream>
using namespace std;

// linux64下long为8字节，32下为4字节
// Windows32-64下long都为4字节

// windows平台的32位64位区分
#if defined(_WIN64)
typedef unsigned __int64 LONG_PTR, * PLONG_PTR;
#else
typedef unsigned long LONG_PTR, * PLONG_PTR;
#endif

// 类型重定义
using u_char = unsigned char;
using ngx_uint_t = LONG_PTR;
struct ngx_pool_t;
struct ngx_pool_large_t;

// cleanup链
typedef void (*ngx_pool_cleanup_pt)(void* data);
struct ngx_pool_cleanup_t {
    ngx_pool_cleanup_pt     handler; // 清理函数
    void*                   data;    // 外部资源(清理函数参数)
    ngx_pool_cleanup_t*     next;    // 链
};
// 大块内存池头信息
struct ngx_pool_large_t {
    ngx_pool_large_t* next;    // 链
    void* alloc;               // 大块内存起始
};
// 第二个开始小块内存池头信息
struct ngx_pool_data_t{
    u_char*         last;       // 小块内存可分配起始
    u_char*         end;        // 小块内存可分配结尾
    ngx_pool_t*     next;       // 链
    ngx_uint_t      failed;     // 标识当前内存池分配小块内存失败次数
} ;
// 第一个小块内存池头信息
struct ngx_pool_t {
    ngx_pool_data_t     d;
    size_t              max;     // 存储小块内存分界线
    ngx_pool_t*         current; // 指向哪个小块内存池(链表)
    ngx_pool_large_t*   large;   // 指向大块内存池(链表)
    ngx_pool_cleanup_t* cleanup; // 指向cleanup链(清理外部资源)
};

// 数值n调整为align的倍数
#define ngx_align(n, align) (((n) + (align - 1)) & ~(align - 1)) 
// 指针调整，已a字节数的倍数进行向上调整
#define ngx_align_ptr(p, align) (u_char *) (((ngx_uint_t) (p) + ((ngx_uint_t) align - 1)) & ~((ngx_uint_t) align - 1))
// 内存清零
#define ngx_memzero(buf, n)       (void) memset(buf, 0, n)
// 对齐
#define NGX_ALIGNMENT   sizeof(ngx_uint_t)    /* platform word */

// 在x86 上NGX_MAX_ALLOC_FROM_POOL 为 4095
const int ngx_pagesize = 4096;
const int NGX_MAX_ALLOC_FROM_POOL = (ngx_pagesize - 1);
// 默认池大小
const int  NGX_DEFAULT_POOL_SIZE = (16 * 1024);
// 字节对齐数
const int NGX_POOL_ALIGNMENT = 16;
// 最小池大小：一个ngx_pool_s大小 + 2个ngx_pool_large_s大小按16字节对齐向上调整
const int NGX_MIN_POOL_SIZE = ngx_align((sizeof(ngx_pool_t) + 2 * sizeof(ngx_pool_large_t)), NGX_POOL_ALIGNMENT);

// 内存池
class NgxMemPool
{
public:
    NgxMemPool(size_t size);
   ~NgxMemPool();
   // 考虑字节内存对齐，向内存池申请size字节
   void* ngx_palloc(size_t size);
   // 不考虑字节内存对齐，向内存池申请size字节
   void* ngx_pnalloc(size_t size);
   // 调用的是ngx_palloc，且会把内存初始化为0
   void* ngx_pcalloc(size_t size);
   // 释放大块内存
   void ngx_pfree(void* p);
   // 内存池重置
   void ngx_reset_pool();
   // 添加清理链
   ngx_pool_cleanup_t* ngx_pool_cleanup_add(size_t size);
private:
    // 创建指定大小的内存池
    bool ngx_create_pool(size_t size);
    // 销毁内存池(遍历清理链，释放大块内存池，释放小块内存池)
    void ngx_destroy_pool();
    // 向内存池申请小块内存
    void* ngx_palloc_small(size_t size, ngx_uint_t align);
    // 小块内存池不够时申请一个block
    void* ngx_palloc_block(size_t size);
    // 向内存池申请大块内存
    void* ngx_palloc_large(size_t size);
private:
    ngx_pool_t* pool;
};