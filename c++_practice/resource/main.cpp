#include "resource_pool.h"
#include <iostream>
#include <stdio.h>
#include <thread>
#include <chrono>

struct Object {
	int value = 0;
	char a[10];
};

using namespace std;

void allocate_resource(const int tid) {
	ResourceId<Object> id;
	Object *obj, *t;
	for (int i=0; i < 3; ++i) {
		obj = get_resource<Object>(&id);
		//cout << "thread: " << tid << " obj: " << obj << " id: " << id.value << endl;
		t = address_resource<Object>(id);
		//printf("thread: %d obj: %p id: %d t: %p\n", tid, obj, id.value, t);
		this_thread::sleep_for(chrono::milliseconds(1));
		return_resource<Object>(id);
	}
	clear_resource<Object>();
}

void test_allocate() {
	int nthread = 3;
	vector<thread> a;
	int i = 0;
	for (i=0; i<nthread; ++i) {
		a.push_back(thread(allocate_resource, i+1));
	}
	for (auto& t : a) {
		t.join();
	}
}

int main() {
	test_allocate();
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	return 0;
}


