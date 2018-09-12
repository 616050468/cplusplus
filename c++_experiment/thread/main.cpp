#include <stdio.h>
#include <thread>

struct A {
	int a;
};

void test(A& a) {
	a.a = 10;
	printf("in test\n");
}

void f() {
	A a = {0};
	std::thread t(test, std::ref(a));
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	if (t.joinable()) {
		printf("thread joinable\n");
		t.join();
	}
	printf("A.a: %d\n", a.a);
}

void some_function() {
	printf("some function\n");
}

std::thread create_thread() {
	std::thread t(some_function);
	return t;
}

void receive_thread(std::thread t) {
	t.join();
}

int main() {
	//f();
	std::thread t1, t2;
	t1 = create_thread();
	receive_thread(std::move(t1));
	//t1.join();
	return 0;
}
