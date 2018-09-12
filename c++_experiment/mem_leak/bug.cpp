#include <stdio.h>
#include <thread>

struct Obj {
public:
	Obj() {

	}
	int a;
};

void memory_leak() {
	Obj* a = new Obj();
	Obj* b = new Obj();
	Obj* c = new Obj();
	Obj* d = new Obj();
	delete a;
	delete b;
	//delete c;
	//delete d;
	printf("111 %p %p %p %p\n",a,b,c,d);
}

void use_after_free() {
	Obj*a = new Obj();
	delete a;
	printf("use_after_free %d\n", a->a);
}

int heap_buff_overflow(int n) {
	int *array = new int[100];
	array[0] = 0;
	int res = array[100+n];
	return res;
}

int main() {
	memory_leak();
	//use_after_free();
	//heap_buff_overflow(1);
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	return 0;
}
