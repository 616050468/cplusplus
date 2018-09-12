#include <stdio.h>
#include <thread>

class Singleton {
public:
	Singleton() {
		printf("Singleton: %p\n", this);
	}
	~Singleton() {
		printf("~Singleton: %p\n", this);
	}
};

static thread_local Singleton l_singleton;

static Singleton singleton;

Singleton* get_singleton() {
	static Singleton singleton;   // init first called;
	return &singleton;
}

int main() {
	printf("before get_singleton\n");
	printf("singleton: %p\n", get_singleton());
	printf("thread_local singleton: %p\n", &l_singleton);
	printf("global singleton: %p\n", &singleton);
	return 0;
}
