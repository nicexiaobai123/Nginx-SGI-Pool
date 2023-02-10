#pragma once
#include <iostream>
using namespace std;

// linux64��longΪ8�ֽڣ�32��Ϊ4�ֽ�
// Windows32-64��long��Ϊ4�ֽ�

// windowsƽ̨��32λ64λ����
#if defined(_WIN64)
typedef unsigned __int64 LONG_PTR, * PLONG_PTR;
#else
typedef unsigned long LONG_PTR, * PLONG_PTR;
#endif

// �����ض���
using u_char = unsigned char;
using ngx_uint_t = LONG_PTR;
struct ngx_pool_t;
struct ngx_pool_large_t;

// cleanup��
typedef void (*ngx_pool_cleanup_pt)(void* data);
struct ngx_pool_cleanup_t {
    ngx_pool_cleanup_pt     handler; // ������
    void*                   data;    // �ⲿ��Դ(����������)
    ngx_pool_cleanup_t*     next;    // ��
};
// ����ڴ��ͷ��Ϣ
struct ngx_pool_large_t {
    ngx_pool_large_t* next;    // ��
    void* alloc;               // ����ڴ���ʼ
};
// �ڶ�����ʼС���ڴ��ͷ��Ϣ
struct ngx_pool_data_t{
    u_char*         last;       // С���ڴ�ɷ�����ʼ
    u_char*         end;        // С���ڴ�ɷ����β
    ngx_pool_t*     next;       // ��
    ngx_uint_t      failed;     // ��ʶ��ǰ�ڴ�ط���С���ڴ�ʧ�ܴ���
} ;
// ��һ��С���ڴ��ͷ��Ϣ
struct ngx_pool_t {
    ngx_pool_data_t     d;
    size_t              max;     // �洢С���ڴ�ֽ���
    ngx_pool_t*         current; // ָ���ĸ�С���ڴ��(����)
    ngx_pool_large_t*   large;   // ָ�����ڴ��(����)
    ngx_pool_cleanup_t* cleanup; // ָ��cleanup��(�����ⲿ��Դ)
};

// ��ֵn����Ϊalign�ı���
#define ngx_align(n, align) (((n) + (align - 1)) & ~(align - 1)) 
// ָ���������a�ֽ����ı����������ϵ���
#define ngx_align_ptr(p, align) (u_char *) (((ngx_uint_t) (p) + ((ngx_uint_t) align - 1)) & ~((ngx_uint_t) align - 1))
// �ڴ�����
#define ngx_memzero(buf, n)       (void) memset(buf, 0, n)
// ����
#define NGX_ALIGNMENT   sizeof(ngx_uint_t)    /* platform word */

// ��x86 ��NGX_MAX_ALLOC_FROM_POOL Ϊ 4095
const int ngx_pagesize = 4096;
const int NGX_MAX_ALLOC_FROM_POOL = (ngx_pagesize - 1);
// Ĭ�ϳش�С
const int  NGX_DEFAULT_POOL_SIZE = (16 * 1024);
// �ֽڶ�����
const int NGX_POOL_ALIGNMENT = 16;
// ��С�ش�С��һ��ngx_pool_s��С + 2��ngx_pool_large_s��С��16�ֽڶ������ϵ���
const int NGX_MIN_POOL_SIZE = ngx_align((sizeof(ngx_pool_t) + 2 * sizeof(ngx_pool_large_t)), NGX_POOL_ALIGNMENT);

// �ڴ��
class NgxMemPool
{
public:
    NgxMemPool(size_t size);
   ~NgxMemPool();
   // �����ֽ��ڴ���룬���ڴ������size�ֽ�
   void* ngx_palloc(size_t size);
   // �������ֽ��ڴ���룬���ڴ������size�ֽ�
   void* ngx_pnalloc(size_t size);
   // ���õ���ngx_palloc���һ���ڴ��ʼ��Ϊ0
   void* ngx_pcalloc(size_t size);
   // �ͷŴ���ڴ�
   void ngx_pfree(void* p);
   // �ڴ������
   void ngx_reset_pool();
   // ���������
   ngx_pool_cleanup_t* ngx_pool_cleanup_add(size_t size);
private:
    // ����ָ����С���ڴ��
    bool ngx_create_pool(size_t size);
    // �����ڴ��(�������������ͷŴ���ڴ�أ��ͷ�С���ڴ��)
    void ngx_destroy_pool();
    // ���ڴ������С���ڴ�
    void* ngx_palloc_small(size_t size, ngx_uint_t align);
    // С���ڴ�ز���ʱ����һ��block
    void* ngx_palloc_block(size_t size);
    // ���ڴ���������ڴ�
    void* ngx_palloc_large(size_t size);
private:
    ngx_pool_t* pool;
};