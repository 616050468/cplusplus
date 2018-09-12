#include<iostream>

using namespace std;

class A
{
public:
	int a;
	char* s;
public:
	~A()
	{
		cout << "destruct" << endl;
	}

	A(): a(0),s(nullptr) {
	}
	A(int a) {
		this->a = a;
	}

	A(const A& other): a(other.a) {
		cout << "copy construction" << endl;
	}

	A& operator=(const A& other)
	{
		a = other.a;
		cout << "copy=" << endl;
		return *this;
	}

	A& operator=(A&& other)
	{
		a = other.a;
		s = other.s;
		other.s = nullptr;
		cout << "move=" << endl;
		return *this;
	}

	A(A&& other) {
		a = other.a;
		s = other.s;
		other.s = nullptr;
		cout << "move construction, from " << &other << " to " << this << endl;
	}
};

A add(A& a, A& b, A& c, A& d, A& e)
{	
	int t = 0;
	t += a.a;
	a.a = t + e.a;
	cout << "add" << endl;
	return a;
}

/*A&& add_and_move(A& a, A& b)
{
	cout << "add_and_move" << endl;
	A tmp(0);
	tmp.a = a.a + b.a;
	return std::move(tmp);
}*/

void test_1()
{
	A a(1), b(2);
	//A c;
	//c = add_and_move(a, b);
	//A d = add_and_move(a, b);
	//c = add(a, b);
	A d = add(a, b, a, a, b);
	cout << "d.a: " << d.a << endl;
}

void test_2()
{
	A a(1);
	A b = std::move(a);
	cout <<"a.a: "<<a.a<<" addr "<<&a<<endl;
	cout <<"b.a: "<<b.a<<" addr "<<&b<< endl;
}

int main(int argc, char** argv)
{
	test_2();	
	return 0;
}

