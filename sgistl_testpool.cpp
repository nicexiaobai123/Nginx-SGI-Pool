#include "sgi_stl_pool.hpp"
#include <iostream>
#include <vector>
using namespace std;
/*
关于sgi_stl内存池不释放是否叫做内存泄漏：

1. 申请的内存没有被及时释放 不等于 内存泄漏：
在单线程中，由于该Allocator中记录内存池起始的指针是静态类型，
所以只要是你在同一个线程中，无论你创建多少个Allocator，记录内存池的变量都是同一个，
换句话说，当下次再创建Vector时，还是使用上一次使用的那个。也就是说他的存在时有意义的，这也是cache或memory pool的意义所在！

2. 该内存池不会疯狂野生长：
这个内存池的空间其实是很小的，因为大于128Byte的内存申请都直接转调了malloc，
从源码中也可以看出，内存池每次重新灌水的新容量是2*total_size + round_up(heap_size >> 4)。
内存池的存在是为了避免大量内存碎片的产生，代价是管理内存所需要多付出的时间和空间消耗。

以上就是内存池一种存在直至程序退出的原因。
*/
int main()
{
	// sgi_stl内存池一旦创建，只会在程序结束时由系统回收堆资源
	vector<int, myallocator<int>> vc1(5);
	for (int i = 0; i < 100; i++)
	{
		vc1.push_back(rand() % 1000);
	}
	vc1.pop_back();

	for (int val : vc1)
	{
		cout << val << " ";
	}
	cout << endl;
	return 0;
}