#pragma execution_character_set("utf-8")
#include <filesystem>
#include <fstream>
#include <chrono>
#include "json.h"
//#include "json.hpp"

using namespace std;
using namespace json;

constexpr auto testjson = R"(
{
	"az": {/* 
		"key":"value\n"*/
	},
	"edf":0.2,
	"edef":"\u3467����",//ע��
	"wes":5058e100
}
)";
static void init() {
	using namespace filesystem;
	using namespace std::chrono;
	auto start = system_clock::now();
#if 1
	for (auto& x : directory_iterator("test")) {
		cout << "�����ļ���" << x << endl;
		ifstream f(x);
		string s(istreambuf_iterator<char>(f), {});
		cout << "ԭ�ļ���\n" << s << endl;
		string err;
		Value value(Parse(testjson, &err, true));
		cout << err << endl;
		cout << "����JSON��\n" << value.dump(4, true) << "\n\n";
	}
#else
	for (auto& x : directory_iterator("test")) {
		//cout << "�����ļ���" << x << endl;
		ifstream f(x);
		//string s(istreambuf_iterator<char>(f), {});
		//cout << "ԭ�ļ���\n" << s << endl;
		try {
			nlohmann::json value(nlohmann::json::parse(f));
			value.dump(4);
		}
		catch (const std::exception& e) {
			cerr << e.what() << endl;
		}
		//cout << "����JSON��\n" << value.dump(4) << "\n\n";
	}
#endif

	auto end = system_clock::now();
	auto duration = duration_cast<microseconds>(end - start);
	cout << "������" << duration.count() << "΢��" << endl;
}
int main() {

	system("chcp 65001");
	init();
	return 0;
}
