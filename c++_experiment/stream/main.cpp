#include <sstream>
#include <iostream>

class LogMessageVoidify {
public:
	LogMessageVoidify() {}
	void operator&(std::basic_ostream<char>& s) {
	}
};

class LogMessage {
public:
	LogMessage() {
		_stream = new std::ostringstream();	
	}
	std::ostringstream& stream() {
		return *_stream;
	}
private:
	std::ostringstream* _stream;
};

#define CLOG(stream, condition) \
	!(condition) ? (void)0 : LogMessageVoidify() & stream

int main() {	
	std::ostringstream s;
	s << 111 << std::endl;
	s << 222 << std::endl;
	CLOG(s, 11) << "test" << 111;
	//LogMessageVoidify() & s << "test" << 111;
	std::cout << s.str() << std::endl;
	return 0;
}
