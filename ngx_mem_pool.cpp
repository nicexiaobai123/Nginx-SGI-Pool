#include "ngx_mem_pool.h"

// �ڴ�ع��캯��
NgxMemPool::NgxMemPool(size_t size)
{
	if (!ngx_create_pool(size)) {
		//cout << "error log" << endl;
		return;
	}
}

// �ڴ����������
NgxMemPool::~NgxMemPool()
{
	ngx_destroy_pool();
}

// ������
bool NgxMemPool::ngx_create_pool(size_t size)
{
	ngx_pool_t* p = (ngx_pool_t*)malloc(size);
	if (p == nullptr) return false;

	p->d.last = (u_char*)p + sizeof(ngx_pool_t);
	p->d.end = (u_char*)p + size;
	p->d.next = nullptr;
	p->d.failed = 0;

	// ���size-ngx_pool_t�Ĵ�СС��4095,��maxΪsize,����Ϊ4095
	size = size - sizeof(ngx_pool_t);
	p->max = (size < NGX_MAX_ALLOC_FROM_POOL) ? size : NGX_MAX_ALLOC_FROM_POOL;

	p->current = p;
	p->large = nullptr;
	p->cleanup = nullptr;

	// ��Ա����
	this->pool = p;
	return true;
}

// ���ٳ�
void NgxMemPool::ngx_destroy_pool()
{
	ngx_pool_t* p, * n;
	ngx_pool_large_t* l;
	ngx_pool_cleanup_t* c;

	// #1 ����cleaup�������õ��ͷ��ⲿ��Դ�ĺ���
	for (c = pool->cleanup; c; c = c->next) {
		if (c->handler) c->handler(c->data);
	}
	// #2 ����ڴ��ͷ� free
	for (l = pool->large; l; l = l->next) {
		if (l->alloc) free(l->alloc);
	}
	// #3 �ȴ���С�飬���һ��ͷ��Ϣ��С���ڴ����
	for (p = pool, n = pool->d.next; /* void */; p = n, n = n->d.next) {
		free(p);
		if (n == nullptr) break;
	}
}

// ���ڴ������С���ڴ�
void* NgxMemPool::ngx_palloc_small(size_t size, ngx_uint_t align)
{
	u_char* m;
	ngx_pool_t* p = pool->current;
	do {
		m = p->d.last;
		if (align) {
			// ��ַ�ǰ�ulong��С����ģ��÷��ϻ����ֳ�
			m = ngx_align_ptr(m, NGX_ALIGNMENT);
		}
		// ��ǰblockʣ����ڴ��ܹ�����size��С
		if ((size_t)(p->d.end - m) >= size) {
			p->d.last = m + size;
			return m;
		}
		p = p->d.next;
	} while (p);
	// β��һ��block
	return ngx_palloc_block(size);
}

// С���ڴ�ز���ʱ����һ��block
void* NgxMemPool::ngx_palloc_block(size_t size)
{
	u_char* m;
	size_t       psize;
	ngx_pool_t* p, * newmem;

	psize = (size_t)(pool->d.end - (u_char*)pool); // һ��block��С

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

// ���ڴ���������ڴ�
void* NgxMemPool::ngx_palloc_large(size_t size)
{
	void* p;
	ngx_uint_t         n;
	ngx_pool_large_t* large;

	p = malloc(size);
	if (p == nullptr) return nullptr;

	n = 0;
	for (large = pool->large; large; large = large->next) {
		// ��ǰ�������ڴ��ͷ��Ϣ,allocΪnull,������ǰ�Ĵ���ڴ��ͷ���,�����ظ�����ͷ��Ϣ
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

// �����ֽ��ڴ���룬���ڴ������size�ֽ�
void* NgxMemPool::ngx_palloc(size_t size)
{
	// ��ʹ���ڴ�������ڴ��ʱ����������Сsize<=max����С���ڴ棬�����������ڴ�
	if (size <= pool->max) {
		return ngx_palloc_small(size, 1);
	}
	return ngx_palloc_large(size);
}

// �������ֽ��ڴ���룬���ڴ������size�ֽ�
void* NgxMemPool::ngx_pnalloc(size_t size)
{
	if (size <= pool->max) {
		return ngx_palloc_small(size, 0);
	}
	return ngx_palloc_large(size);
}

// ���õ���ngx_palloc���һ���ڴ��ʼ��Ϊ0
void* NgxMemPool::ngx_pcalloc(size_t size)
{
	void* p = ngx_palloc(size);
	if (p) ngx_memzero(p, size);
	return p;
}

// �ͷŴ���ڴ�
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

// �ڴ������
void NgxMemPool::ngx_reset_pool()
{
	ngx_pool_t* p;
	ngx_pool_large_t* l;

	// ** Ӧ�û��ñ���һ��cleanup������ֹ�ڴ�й©(Դ��û��) **
	ngx_pool_cleanup_t* c;
	for (c = pool->cleanup; c; c = c->next) {
		if (c->handler) c->handler(c->data);
	}

	// ����ڴ�ֱ��free
	for (l = pool->large; l; l = l->next) {
		if (l->alloc) free(l->alloc);
	}

	// ��һ��block
	p = pool;
	p->d.last = (u_char*)p + sizeof(ngx_pool_t);
	p->d.failed = 0;

	// �����block
	for (p = p->d.next; p; p = p->d.next) {
		p->d.last = (u_char*)p + sizeof(ngx_pool_data_t);
		p->d.failed = 0;
	}

	pool->current = pool;
	pool->large = nullptr;
}

// ���������
ngx_pool_cleanup_t* NgxMemPool::ngx_pool_cleanup_add(size_t size)
{
	// ֻ�����ڴ�͹���,"������"�ⲿָ��
	ngx_pool_cleanup_t* c;

	// С���ڴ�洢ͷ��Ϣngx_pool_cleanup_t
	c = (ngx_pool_cleanup_t*)ngx_palloc(sizeof(ngx_pool_cleanup_t));
	if (c == nullptr) return nullptr;

	if (size) {
		// �����ڴ�,��Żص����������Ĵ�С(���ⲿ����Դ��ָ�롢�ļ�������)
		c->data = ngx_palloc(size);
		if (c->data == nullptr)  return nullptr;
	}
	else {
		c->data = nullptr;
	}

	// ����
	c->handler = NULL;
	c->next = pool->cleanup;
	pool->cleanup = c;

	return c;
}