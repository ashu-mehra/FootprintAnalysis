#include <iostream>
#include <fstream>

using namespace std;

int main(int argc, char *argv[]) {
	if (argc != 3) {
		cout << "Insufficient arguments\n";
		return -1;
	}
	pid_t pid = atoi(argv[1]);
	string vaddr_str = argv[2];
	uint64_t vaddr = stoull(vaddr_str, NULL, 16);
	cout << "PID: " << pid << "\n";
	cout << "Virtual address: " << hex << vaddr << dec << "\n";
	ifstream pagemap;
	string filename("/proc/");
	filename.append(argv[1]).append("/pagemap");
	cout << "pagemap file: " << filename;
	pagemap.open(filename, ios::in | ios::binary);
	pagemap.seekg (0, ios::beg);
	streampos offset = (vaddr/4096) * sizeof(uint64_t);
	cout << "pagemap Offset: " << offset << "\n";
	pagemap.seekg(offset);
	unsigned char value[8];
	//uint64_t value;
	pagemap.read((char *)value, sizeof(uint64_t));
	cout << "vlaue: ";
	for (int i = 0; i < 8; i++) {
		unsigned int val = value[i];
		cout << hex << val << " ";
	}
	pagemap.close();
	return 0;
}
