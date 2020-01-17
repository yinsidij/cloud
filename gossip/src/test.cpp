
#include <iostream>
using namespace std;
enum Type {
	t0,
	t1,
	t2,
	tx
};

typedef struct MessageHdr {
	enum Type msgType;
} MessageHdr;

int main() {
	cout << sizeof(Type) << endl;
	cout << sizeof(char) << endl;
	MessageHdr* ptr = (MessageHdr*) malloc(sizeof(MessageHdr));
	char* ptr_next = (char*)(ptr+1);
	cout << "ptr address: " << endl;
	cout << ptr << endl;
	cout << "ptr_next address: " << endl;
	cout << ptr_next << endl;
}
