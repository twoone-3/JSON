#pragma execution_character_set("utf-8")
#include <filesystem>
#include <fstream>
#include <chrono>
#include "json.h"

using namespace std;

constexpr auto testjson = R"(
{
	"az": {
		"key":"value\n"
	},
	"edf":0.2,
	"edef":"\u3467����",//ע��
	"wes":5058e100
}
)";
static void init() {
	using namespace json;
	using namespace filesystem;
	using namespace std::chrono;
	auto start = system_clock::now();

	for (auto& x : directory_iterator("test")) {
		cout << "�����ļ���" << x << endl;
		ifstream f(x);
		string s(istreambuf_iterator<char>(f), {});
		cout << "ԭ�ļ���\n" << s << endl;
		Value value(Parse(testjson));
		cout << "����JSON��\n" << value << "\n\n";
	}
	//std::string a;
	//for (unsigned i = 0; i != 1000000; ++i) {
	//	a += "az";
	//	a.append("az", 2);
	//	//a += 'a';
	//	//a += 'z';
	//}

	auto end = system_clock::now();
	auto duration = duration_cast<microseconds>(end - start);
	cout << "������" << duration.count() << "΢��" << endl;
}
int main() {

	system("chcp 65001");
	for (size_t i = 0; i < 1; i++) {
		init();
	}
	return 0;
}
