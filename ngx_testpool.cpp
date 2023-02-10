#include "ngx_mem_pool.h"

typedef struct Data stData;
struct Data
{
    char* ptr;
    FILE* pfile;
};

void func1(void* p1)
{
    char* p = (char*)p1;
    printf("free ptr mem!\n");
    free(p);
}
void func2(void* p1)
{
    FILE* pf = (FILE*)p1;
    printf("close file!\n");
    fclose(pf);
}

#if 0

int main()
{
    // 512 - sizeof(ngx_pool_t) - 4095   =>   max
    NgxMemPool pool(512);

    stData* p1 = (stData*)pool.ngx_palloc(128); // 从小块内存池分配的
    if (p1 == nullptr)
    {
        printf("ngx_palloc 128 bytes fail...");
        return -1;
    }

    p1->ptr = (char*)malloc(12);
    strcpy(p1->ptr, "world hello");

    ngx_pool_cleanup_t* c0 = pool.ngx_pool_cleanup_add(sizeof(char*));
    c0->handler = func1;
    c0->data = p1->ptr;

    // =================================================

    stData* p2 = (stData*)pool.ngx_palloc(512); // 从大块内存池分配的
    if (p2 == nullptr)
    {
        printf("ngx_palloc 512 bytes fail...");
        return -1;
    }

    p2->ptr = (char*)malloc(12);
    strcpy(p2->ptr, "hello world");
    p2->pfile = fopen("data.txt", "w");

    ngx_pool_cleanup_t* c1 = pool.ngx_pool_cleanup_add(sizeof(char*));
    c1->handler = func1;
    c1->data = p2->ptr;

    ngx_pool_cleanup_t* c2 = pool.ngx_pool_cleanup_add(sizeof(FILE*));
    c2->handler = func2;
    c2->data = p2->pfile;

    return 0;
}
#endif