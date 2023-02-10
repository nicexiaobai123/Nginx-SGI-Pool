#include "ngx_mem_pool.h"

// 内存池构造函数
NgxMemPool::NgxMemPool(size_t size)
{
	if (!ngx_create_pool(size)) {
		//cout << "error log" << endl;
		return;
	}
}

// 内存池析构函数
NgxMemPool::~NgxMemPool()
{
	ngx_destroy_pool();
}

// 创建池
bool NgxMemPool::ngx_create_pool(size_t size)
{
	ngx_pool_t* p = (ngx_pool_t*)malloc(size);
	if (p == nullptr) return false;

	p->d.last = (u_char*)p + sizeof(ngx_pool_t);
	p->d.end = (u_char*)p + size;
	p->d.next = nullptr;
	p->d.failed = 0;

	// 如果size-ngx_pool_t的大小小于4095,则max为size,否则为4095
	size = size - sizeof(ngx_pool_t);
	p->max = (size < NGX_MAX_ALLOC_FROM_POOL) ? size : NGX_MAX_ALLOC_FROM_POOL;

	p->current = p;
	p->large = nullptr;
	p->cleanup = nullptr;

	// 成员变量
	this->pool = p;
	return true;
}

// 销毁池
void NgxMemPool::ngx_destroy_pool()
{
	ngx_pool_t* p, * n;
	ngx_pool_large_t* l;
	ngx_pool_cleanup_t* c;

	// #1 调用cleaup链上设置的释放外部资源的函数
	for (c = pool->cleanup; c; c = c->next) {
		if (c->handler) c->handler(c->data);
	}
	// #2 大块内存释放 free
	for (l = pool->large; l; l = l->next) {
		if (l->alloc) free(l->alloc);
	}
	// #3 先大块后小块，大块一部头信息在小块内存池中
	for (p = pool, n = pool->d.next; /* void */; p = n, n = n->d.next) {
		free(p);
		if (n == nullptr) break;
	}
}

// 向内存池申请小块内存
void* NgxMemPool::ngx_palloc_small(size_t size, ngx_uint_t align)
{
	u_char* m;
	ngx_pool_t* p = pool->current;
	do {
		m = p->d.last;
		if (align) {
			// 地址是按ulong大小对齐的，得符合机器字长
			m = ngx_align_ptr(m, NGX_ALIGNMENT);
		}
		// 当前block剩余的内存能够分配size大小
		if ((size_t)(p->d.end - m) >= size) {
			p->d.last = m + size;
			return m;
		}
		p = p->d.next;
	} while (p);
	// 尾插一个block
	return ngx_palloc_block(size);
}

// 小块内存池不够时申请一个block
void* NgxMemPool::ngx_palloc_block(size_t size)
{
	u_char* m;
	size_t       psize;
	ngx_pool_t* p, * newmem;

	psize = (size_t)(pool->d.end - (u_char*)pool); // 一个block大小

	m = (u_char*)malloc(psize);
	if (m == nullptr) return nullptr;

	newmem = (ngx_pool_t*)m;
	newmem->d.end = m + psize;
	newmem->d.next = NULL;
	newmem->d.failed = 0;

	m += sizeof(ngx_pool_data_t);
	m = ngx_align_ptr(m, NGX_ALIGNMENT);
	newmem->d.last = m + size;

	for (p = pool->current; p->d.next; p = p->d.next) {
		if (p->d.failed++ > 4) {
			pool->current = p->d.next;
		}
	}

	p->d.next = newmem;
	return m;
}

// 向内存池申请大块内存
void* NgxMemPool::ngx_palloc_large(size_t size)
{
	void* p;
	ngx_uint_t         n;
	ngx_pool_large_t* large;

	p = malloc(size);
	if (p == nullptr) return nullptr;

	n = 0;
	for (large = pool->large; large; large = large->next) {
		// 以前申请大块内存的头信息,alloc为null,代表以前的大块内存释放了,可以重复利用头信息
		if (large->alloc == NULL) {
			large->alloc = p;
			return p;
		}
		if (n++ > 3) break;
	}

	large = (ngx_pool_large_t*)ngx_palloc_small(sizeof(ngx_pool_large_t), 1);
	if (large == nullptr) {
		free(p);
		return nullptr;
	}

	large->alloc = p;
	large->next = pool->large;
	pool->large = large;

	return p;
}

// 考虑字节内存对齐，向内存池申请size字节
void* NgxMemPool::ngx_palloc(size_t size)
{
	// 当使用内存池申请内存的时候，如果申请大小size<=max申请小块内存，否则申请大块内存
	if (size <= pool->max) {
		return ngx_palloc_small(size, 1);
	}
	return ngx_palloc_large(size);
}

// 不考虑字节内存对齐，向内存池申请size字节
void* NgxMemPool::ngx_pnalloc(size_t size)
{
	if (size <= pool->max) {
		return ngx_palloc_small(size, 0);
	}
	return ngx_palloc_large(size);
}

// 调用的是ngx_palloc，且会把内存初始化为0
void* NgxMemPool::ngx_pcalloc(size_t size)
{
	void* p = ngx_palloc(size);
	if (p) ngx_memzero(p, size);
	return p;
}

// 释放大块内存
void NgxMemPool::ngx_pfree(void* p)
{
	ngx_pool_large_t* l;
	for (l = pool->large; l; l = l->next) {
		if (p == l->alloc) {
			free(l->alloc);
			l->alloc = NULL;
		}
	}
}

// 内存池重置
void NgxMemPool::ngx_reset_pool()
{
	ngx_pool_t* p;
	ngx_pool_large_t* l;

	// ** 应该还得遍历一下cleanup链，防止内存泄漏(源码没有) **
	ngx_pool_cleanup_t* c;
	for (c = pool->cleanup; c; c = c->next) {
		if (c->handler) c->handler(c->data);
	}

	// 大块内存直接free
	for (l = pool->large; l; l = l->next) {
		if (l->alloc) free(l->alloc);
	}

	// 第一个block
	p = pool;
	p->d.last = (u_char*)p + sizeof(ngx_pool_t);
	p->d.failed = 0;

	// 后面的block
	for (p = p->d.next; p; p = p->d.next) {
		p->d.last = (u_char*)p + sizeof(ngx_pool_data_t);
		p->d.failed = 0;
	}

	pool->current = pool;
	pool->large = nullptr;
}

// 添加清理链
ngx_pool_cleanup_t* NgxMemPool::ngx_pool_cleanup_add(size_t size)
{
	// 只申请内存和挂链,"清理函数"外部指定
	ngx_pool_cleanup_t* c;

	// 小块内存存储头信息ngx_pool_cleanup_t
	c = (ngx_pool_cleanup_t*)ngx_palloc(sizeof(ngx_pool_cleanup_t));
	if (c == nullptr) return nullptr;

	if (size) {
		// 申请内存,存放回调函数参数的大小(如外部堆资源的指针、文件描述符)
		c->data = ngx_palloc(size);
		if (c->data == nullptr)  return nullptr;
	}
	else {
		c->data = nullptr;
	}

	// 挂链
	c->handler = NULL;
	c->next = pool->cleanup;
	pool->cleanup = c;

	return c;
}