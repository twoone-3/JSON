﻿#include "json.h"
#include <charconv>

#define JSON_CHECK(expr) if (!(expr))return false;
#define JSON_SKIP JSON_CHECK(skipWhiteSpace())
#define JSON_CHECK_OUT_OF_BOUNDS if (cur_ == end_)return error("unexpected ending character")
#define JSON_ASSERT(expr) if (!(expr))exit(-1)

namespace json {
static void CodePointToUTF8(String& s, UInt u) {
	if (u <= 0x7F)
		s += static_cast<char>(u & 0xFF);
	else if (u <= 0x7FF) {
		s += static_cast<char>(0xC0 | (0xFF & (u >> 6)));
		s += static_cast<char>(0x80 | (0x3F & u));
	}
	else if (u <= 0xFFFF) {
		s += static_cast<char>(0xE0 | (0xFF & (u >> 12)));
		s += static_cast<char>(0x80 | (0x3F & (u >> 6)));
		s += static_cast<char>(0x80 | (0x3F & u));
	}
	else {
		s += static_cast<char>(0xF0 | (0xFF & (u >> 18)));
		s += static_cast<char>(0x80 | (0x3F & (u >> 12)));
		s += static_cast<char>(0x80 | (0x3F & (u >> 6)));
		s += static_cast<char>(0x80 | (0x3F & u));
	}
}
static UInt UTF8ToCodepoint(const char*& cur, const char* end) {
	constexpr UInt REPLACEMENT_CHARACTER = 0xFFFD;

	UInt firstByte = static_cast<unsigned char>(*cur);

	if (firstByte < 0x80)
		return firstByte;

	if (firstByte < 0xE0) {
		if (end - cur < 2)
			return REPLACEMENT_CHARACTER;

		UInt calculated =
			((firstByte & 0x1F) << 6) | (static_cast<UInt>(cur[1]) & 0x3F);
		cur += 1;
		// oversized encoded characters are invalid
		return calculated < 0x80 ? REPLACEMENT_CHARACTER : calculated;
	}

	if (firstByte < 0xF0) {
		if (end - cur < 3)
			return REPLACEMENT_CHARACTER;

		UInt calculated = ((firstByte & 0x0F) << 12) |
			((static_cast<UInt>(cur[1]) & 0x3F) << 6) |
			(static_cast<UInt>(cur[2]) & 0x3F);
		cur += 2;
		// surrogates aren't valid codepoints itself
		// shouldn't be UTF-8 encoded
		if (calculated >= 0xD800 && calculated <= 0xDFFF)
			return REPLACEMENT_CHARACTER;
		// oversized encoded characters are invalid
		return calculated < 0x800 ? REPLACEMENT_CHARACTER : calculated;
	}

	if (firstByte < 0xF8) {
		if (end - cur < 4)
			return REPLACEMENT_CHARACTER;

		UInt calculated = ((firstByte & 0x07) << 18) |
			((static_cast<UInt>(cur[1]) & 0x3F) << 12) |
			((static_cast<UInt>(cur[2]) & 0x3F) << 6) |
			(static_cast<UInt>(cur[3]) & 0x3F);
		cur += 3;
		// oversized encoded characters are invalid
		return calculated < 0x10000 ? REPLACEMENT_CHARACTER : calculated;
	}

	return REPLACEMENT_CHARACTER;
}
static void writeHex16Bit(String& s, UInt u) {
	constexpr const char hex2[513] =
		"000102030405060708090a0b0c0d0e0f"
		"101112131415161718191a1b1c1d1e1f"
		"202122232425262728292a2b2c2d2e2f"
		"303132333435363738393a3b3c3d3e3f"
		"404142434445464748494a4b4c4d4e4f"
		"505152535455565758595a5b5c5d5e5f"
		"606162636465666768696a6b6c6d6e6f"
		"707172737475767778797a7b7c7d7e7f"
		"808182838485868788898a8b8c8d8e8f"
		"909192939495969798999a9b9c9d9e9f"
		"a0a1a2a3a4a5a6a7a8a9aaabacadaeaf"
		"b0b1b2b3b4b5b6b7b8b9babbbcbdbebf"
		"c0c1c2c3c4c5c6c7c8c9cacbcccdcecf"
		"d0d1d2d3d4d5d6d7d8d9dadbdcdddedf"
		"e0e1e2e3e4e5e6e7e8e9eaebecedeeef"
		"f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff";
	const UInt hi = (u >> 8) & 0xFF;
	const UInt lo = u & 0xFF;
	s += hex2[2 * hi];
	s += hex2[2 * hi + 1];
	s += hex2[2 * lo];
	s += hex2[2 * lo + 1];
}

bool Value::Parser::parse(StringView str, Value& value, bool allow_comments) {
	cur_ = str.data();
	begin_ = str.data();
	end_ = str.data() + str.length();
	allow_comments_ = allow_comments;
	// empty string
	JSON_CHECK_OUT_OF_BOUNDS;
	// skip BOM
	if (cur_[0] == 0xEF && cur_[1] == 0xBB && cur_[2] == 0xBF)
		cur_ += 3;
	// skip whitespace
	JSON_SKIP;
	JSON_CHECK_OUT_OF_BOUNDS;
	return parseValue(value);
}
String Value::Parser::getError() {
	String err(err_);
	size_t line = 1;
	err += " in line ";
	// count the number of rows from begin_ to cur_
	for (const char* cur = begin_; cur != cur_; ++cur)
		if (*cur == '\n')++line;
	char buffer[21]{ 0 };
	std::to_chars(buffer, buffer + sizeof buffer, line);
	err += buffer;
	return err;
}
bool Value::Parser::parseValue(Value& value) {
	switch (*cur_) {
	case 'n':return parseNull(value);
	case 't':return parseTrue(value);
	case 'f':return parseFalse(value);
	case '[':return parseArray(value);
	case '{':return parseObject(value);
	case '"':return parseString(value);
	default:return parseNumber(value);
	}
}
bool Value::Parser::parseNull(Value& value) {
	if (cur_[1] == 'u' && cur_[2] == 'l' && cur_[3] == 'l') {
		cur_ += 4;
		value = nullptr;
		return true;
	}
	return error("Missing 'null'");
}
bool Value::Parser::parseTrue(Value& value) {
	if (cur_[1] == 'r' && cur_[2] == 'u' && cur_[3] == 'e') {
		cur_ += 4;
		value = true;
		return true;
	}
	return error("Missing 'true'");
}
bool Value::Parser::parseFalse(Value& value) {
	if (cur_[1] == 'a' && cur_[2] == 'l' && cur_[3] == 's' && cur_[4] == 'e') {
		cur_ += 5;
		value = false;
		return true;
	}
	return error("Missing 'false'");
}
bool Value::Parser::parseString(Value& value) {
	value = kString;
	return parseString(*value.data_.s);
}
bool Value::Parser::parseString(String& s) {
	while (true) {
		// "xxxx"
		// ^
		char c = *++cur_;
		switch (c) {
		case '\0':
			return error("missing '\"'");
		case '"':
			++cur_;
			return true;
		case '\\':
			switch (*++cur_) {
			case '"': s += '"'; break;
			case 'n': s += '\n'; break;
			case 'r': s += '\r'; break;
			case 't': s += '\t'; break;
			case 'f': s += '\f'; break;
			case 'b': s += '\b'; break;
			case '/': s += '/'; break;
			case '\\': s += '\\'; break;
			case 'u': {
				UInt u = 0;
				JSON_CHECK(parseHex4(u));
				if (u >= 0xD800 && u <= 0xDBFF) {
					// "x\u0123\u0234xx"
					//        ^
					if (cur_[1] == '\\' && cur_[2] == 'u') {
						cur_ += 2;
						UInt surrogatePair = 0;
						if (!parseHex4(surrogatePair))
							u = 0x10000 + ((u & 0x3FF) << 10) + (surrogatePair & 0x3FF);
						else
							return error("invalid character");
					}
					else
						return error("missing surrogate pair");
				}
				CodePointToUTF8(s, u);
			}break;
			default:
				return error("invalid escape");
			}
			break;
		default:
			if (static_cast<unsigned char>(c) < ' ')
				return error("The ASCII code of the characters in the string must be greater than 32");
			s += c;
			break;
		}
	}
}
bool Value::Parser::parseHex4(UInt& u) {
	//u = 0;
	char ch = 0;
	for (UInt i = 0; i < 4; ++i) {
		u <<= 4;
		// "x\u0123x"
		//    ^
		ch = *++cur_;
		JSON_CHECK_OUT_OF_BOUNDS;
		if (ch >= '0' && ch <= '9')
			u |= ch - '0';
		else if (ch >= 'a' && ch <= 'f')
			u |= ch - 'a' + 10;
		else if (ch >= 'A' && ch <= 'F')
			u |= ch - 'A' + 10;
		else
			return error("invalid character");
	}
	return true;
}
bool Value::Parser::parseArray(Value& value) {
	++cur_;
	value = kArray;
	JSON_SKIP;
	//empty array
	if (*cur_ == ']')
		return ++cur_, true;
	while (true) {
		value.data_.a->push_back(nullptr);
		JSON_SKIP;
		JSON_CHECK(parseValue(value.data_.a->back()));
		JSON_SKIP;
		char ch = *cur_;
		if (ch == ',')
			++cur_;
		else if (ch == ']')
			return ++cur_, true;
		else
			return error("missing ',' or ']'");
	}
}
bool Value::Parser::parseObject(Value& value) {
	++cur_;
	value = kObject;
	JSON_SKIP;
	//empty object
	if (*cur_ == '}')
		return ++cur_, true;
	while (true) {
		String key;
		JSON_SKIP;
		if (*cur_ != '"')
			return error("missing '\"'");
		JSON_CHECK(parseString(key));
		JSON_SKIP;
		if (*cur_ != ':')
			return error("missing ':'");
		++cur_;
		JSON_CHECK_OUT_OF_BOUNDS;
		JSON_SKIP;
		JSON_CHECK(parseValue(value.data_.o->operator[](key)));
		JSON_SKIP;
		char ch = *cur_;
		if (ch == ',')
			++cur_;
		else if (ch == '}')
			return ++cur_, true;
		else
			return error("missing ',' or '}'");
	}
}
bool Value::Parser::parseNumber(Value& value) {
	// Refer to https://www.json.org/img/number.png
	// the last character
	const char* end = cur_;
	char c = *end;
	// minus sign
	if (c == '-')
		c = *++end;
	// first '0'
	if (c == '0') {
		c = *++end;
		if (c >= '0' && c <= '9')
			return error("the first character of the number cannot be '0'");
	}
	// integral part
	while (c >= '0' && c <= '9')
		c = *++end;
	// fractional part
	if (c == '.') {
		c = *++end;
		while (c >= '0' && c <= '9')
			c = *++end;
	}
	// exponential part
	if (c == 'e' || c == 'E') {
		c = *++end;
		if (c == '+' || c == '-')
			c = *++end;
		while (c >= '0' && c <= '9')
			c = *++end;
	}
	// check out of range
	if (c == '\0')
		return error("unexpected ending character");
	if (end == cur_)
		return error("missing character");
	double num;
	std::from_chars(cur_, end, num);
	cur_ = end;
	value = num;
	return true;
}
bool Value::Parser::skipWhiteSpace() {
	while (true) {
		switch (*cur_) {
		case '\0':return error("unexpected ending character");
		case '\t':
		case '\n':
		case '\r':
		case ' ':
			break;
		case '/':
			// If it is true, it is allowed to parse comments in the style of'//' or'/**/'
			if (!allow_comments_)
				return error("comments are not allowed here");
			++cur_;
			if (*cur_ == '/') {
				while (*++cur_ != '\n') {
					JSON_CHECK_OUT_OF_BOUNDS;
				}
				++cur_;
				break;
			}
			else if (*cur_ == '*') {
				while (*++cur_ != '*') {
					JSON_CHECK_OUT_OF_BOUNDS;
				}
				if (*++cur_ == '/') {
					++cur_;
					break;
				}
			}
			else { return error("invalied comment style"); }
		default:
			return true;
		}
		++cur_;
		JSON_CHECK_OUT_OF_BOUNDS;
	}
	return true;
}
bool Value::Parser::error(const char* err) {
	err_ = err;
	return false;
}

Value::Writer::Writer(UInt indent_count, bool emit_utf8)
	:out_(), depth_of_indentation_(0), indent_count_(indent_count), emit_utf8_(emit_utf8) {
}
void Value::Writer::writePrettyValue(const Value& value) {
	switch (value.type_) {
	case kNull:return writeNull();
	case kBoolean:return writeBool(value);
	case kInteger:return writeInt(value);
	case kUInteger:return writeUInt(value);
	case kReal:return writeDouble(value);
	case kString:return writeString(*value.data_.s);
	case kArray:return writePrettyArray(value);
	case kObject:return writePrettyObject(value);
	}
}
void Value::Writer::writeValue(const Value& value) {
	switch (value.type_) {
	case kNull:return writeNull();
	case kBoolean:return writeBool(value);
	case kInteger:return writeInt(value);
	case kUInteger:return writeUInt(value);
	case kReal:return writeDouble(value);
	case kString:return writeString(*value.data_.s);
	case kArray:return writeArray(value);
	case kObject:return writeObject(value);
	}
}
String Value::Writer::getOutput() {
	return out_;
}
void Value::Writer::writeIndent() {
	out_.append(depth_of_indentation_ * indent_count_, ' ');
}
void Value::Writer::writeNull() {
	out_.append("null", 4);
}
void Value::Writer::writeBool(const Value& value) {
	value.data_.b ? out_.append("true", 4) : out_.append("false", 5);
}
void Value::Writer::writeInt(const Value& value) {
	char buffer[21]{ 0 };
	std::to_chars(buffer, buffer + sizeof buffer, value.data_.i64);
	out_.append(buffer);
}
void Value::Writer::writeUInt(const Value& value) {
	char buffer[21]{ 0 };
	std::to_chars(buffer, buffer + sizeof buffer, value.data_.u64);
	out_.append(buffer);
}
void Value::Writer::writeDouble(const Value& value) {
	char buffer[21]{ 0 };
	std::to_chars(buffer, buffer + sizeof buffer, value.data_.d);
	out_.append(buffer);
}
void Value::Writer::writeString(StringView str) {
	const char* cur = str.data();
	const char* end = cur + str.length();
	out_ += '"';
	while (cur < end) {
		char c = *cur;
		switch (c) {
		case '"':out_ += "\\\""; break;
		case '\\':out_ += "\\\\"; break;
		case '\b':out_ += "\\b"; break;
		case '\f':out_ += "\\f"; break;
		case '\n':out_ += "\\n"; break;
		case '\r':out_ += "\\r"; break;
		case '\t':out_ += "\\t"; break;
		default:
			if (emit_utf8_) {
				if (uint8_t(c) < 0x20) {
					out_ += "\\u";
					writeHex16Bit(out_, c);
				}
				else {
					out_ += c;
				}
			}
			else {
				UInt codepoint = UTF8ToCodepoint(cur, end); // modifies `c`
				if (codepoint < 0x20) {
					out_ += "\\u";
					writeHex16Bit(out_, codepoint);
				}
				else if (codepoint < 0x80) {
					out_ += static_cast<char>(codepoint);
				}
				else if (codepoint < 0x10000) {
					// Basic Multilingual Plane
					out_ += "\\u";
					writeHex16Bit(out_, codepoint);
				}
				else {
					// Extended Unicode. Encode 20 bits as a surrogate pair.
					codepoint -= 0x10000;
					out_ += "\\u";
					writeHex16Bit(out_, 0xD800 + ((codepoint >> 10) & 0x3FF));
					writeHex16Bit(out_, 0xDC00 + (codepoint & 0x3FF));
				}
				break;
			}
		}
		++cur;
	}
	out_ += '"';
}
void Value::Writer::writeArray(const Value& value) {
	out_ += '[';
	if (!value.data_.a->empty()) {
		for (auto& val : *value.data_.a) {
			writeValue(val);
			out_ += ',';
		}
		out_.pop_back();
	}
	out_ += ']';

}
void Value::Writer::writeObject(const Value& value) {
	out_ += '{';
	if (!value.data_.o->empty()) {
		for (auto& [key, val] : *value.data_.o) {
			writeString(key);
			out_ += ':';
			writeValue(val);
			out_ += ',';
		}
		out_.pop_back();
	}
	out_ += '}';
}
void Value::Writer::writePrettyArray(const Value& value) {
	out_ += '[';
	if (!value.data_.a->empty()) {
		out_ += '\n';
		++depth_of_indentation_;
		for (auto& val : *value.data_.a) {
			writeIndent();
			writePrettyValue(val);
			out_ += ',';
			out_ += '\n';
		}
		--depth_of_indentation_;
		out_.pop_back();
		out_.pop_back();
		out_ += '\n';
		writeIndent();
	}
	out_ += ']';
}
void Value::Writer::writePrettyObject(const Value& value) {
	out_ += '{';
	if (!value.data_.o->empty()) {
		out_ += '\n';
		++depth_of_indentation_;
		for (auto& [key, val] : *value.data_.o) {
			writeIndent();
			writeString(key);
			out_ += ':';
			out_ += ' ';
			writePrettyValue(val);
			out_ += ',';
			out_ += '\n';
		}
		--depth_of_indentation_;
		out_.pop_back();
		out_.pop_back();
		out_ += '\n';
		writeIndent();
	}
	out_ += '}';
}

Value::ValueData::ValueData() {}
Value::ValueData::ValueData(bool value) : b(value) {}
Value::ValueData::ValueData(Int value) : i(value) {}
Value::ValueData::ValueData(UInt value) : u(value) {}
Value::ValueData::ValueData(Int64 value) : i64(value) {}
Value::ValueData::ValueData(UInt64 value) : u64(value) {}
Value::ValueData::ValueData(Float value) : f(value) {}
Value::ValueData::ValueData(Double value) : d(value) {}
Value::ValueData::ValueData(StringView value) : s(new (std::nothrow)String(value)) {}
Value::ValueData::ValueData(ValueType type) {
	switch (type) {
	case json::kString:
		s = new (std::nothrow)String;
		break;
	case json::kArray:
		a = new (std::nothrow)Array;
		break;
	case json::kObject:
		o = new (std::nothrow)Object;
		break;
	}
}
Value::ValueData::ValueData(const ValueData& other, ValueType type) {
	switch (type) {
	case kNull:
		break;
	case kBoolean:
		b = other.b;
		break;
	case kInteger:
		i64 = other.i64;
		break;
	case kUInteger:
		u64 = other.u64;
		break;
	case kReal:
		d = other.d;
		break;
	case kString:
		s = new (std::nothrow)String(*other.s);
		break;
	case kArray:
		a = new (std::nothrow)Array(*other.a);
		break;
	case kObject:
		o = new (std::nothrow)Object(*other.o);
		break;
	default:
		break;
	}

}
Value::ValueData::ValueData(ValueData&& other) {
	u64 = other.u64;
}
void Value::ValueData::assign(const ValueData& other, ValueType type) {
	operator=(ValueData(other, type));
}
void Value::ValueData::operator=(ValueData&& other) {
	u64 = other.u64;
}
void Value::ValueData::destroy(ValueType type) {
	switch (type) {
	case json::kString:
		delete s;
		break;
	case json::kArray:
		delete a;
		break;
	case json::kObject:
		delete o;
		break;
	}
}

Value::Value() : data_(), type_(kNull) {}
Value::Value(nullptr_t) : data_(), type_(kNull) {}
Value::Value(bool value) : data_(value), type_(kBoolean) {}
Value::Value(Int value) : data_(value), type_(kInteger) {}
Value::Value(UInt value) : data_(value), type_(kUInteger) {}
Value::Value(Int64 value) : data_(value), type_(kInteger) {}
Value::Value(UInt64 value) : data_(value), type_(kUInteger) {}
Value::Value(Float value) : data_(value), type_(kReal) {}
Value::Value(Double value) : data_(value), type_(kReal) {}
Value::Value(StringView value) : data_(value), type_(kString) {}
Value::Value(ValueType type) : data_(type), type_(type) {};
Value::Value(const Value& other) :data_(other.data_, other.type_), type_(other.type_) {};
Value::Value(Value&& other)noexcept :data_(move(other.data_)), type_(other.type_) {
	other.type_ = kNull;
	other.data_.u64 = 0;
};
Value::~Value() {
	data_.destroy(type_);
	type_ = kNull;
}
Value& Value::operator=(const Value& other) {
	return operator=(Value(other));
};
Value& Value::operator=(Value&& other)noexcept {
	swap(other);
	return *this;
};
bool Value::equal(const Value& other)const {
	if (type_ != other.type_)
		return false;
	switch (type_) {
	case kNull:
		return true;
	case kBoolean:
		return data_.b == other.data_.b;
	case kInteger:
		return data_.i64 == other.data_.i64;
	case kUInteger:
		return data_.u64 == other.data_.u64;
	case kReal:
		return data_.d == other.data_.d;
	case kString:
		return *data_.s == *other.data_.s;
	case kArray:
		return *data_.a == *other.data_.a;
	case kObject:
		return *data_.o == *other.data_.o;
	default:
		break;
	}
	return false;
}
bool Value::operator==(const Value& other)const {
	return equal(other);
}
Value& Value::operator[](const String& index) {
	JSON_ASSERT(isObject() || isNull());
	if (isNull())
		*this = kObject;
	return data_.o->operator[](index);
}
Value& Value::operator[](size_t index) {
	JSON_ASSERT(isArray() || isNull());
	if (isNull())
		*this = kArray;
	auto begin = data_.a->begin();
	while (begin != data_.a->end()) {
		--index;
		if (!index) {
			return *begin;
		}
		++begin;
	}
	return data_.a->operator[](index);
}
void Value::insert(const String& index, Value&& value) {
	JSON_ASSERT(isObject() || isNull());
	if (isNull())
		*this = kObject;
	data_.o->emplace(index, value);
}
bool Value::asBool()const { JSON_ASSERT(isBool()); return data_.b; }
Int Value::asInt()const { JSON_ASSERT(isNumber()); return data_.i; }
UInt Value::asUInt()const { JSON_ASSERT(isNumber()); return data_.u; }
Int64 Value::asInt64()const { JSON_ASSERT(isNumber()); return data_.i64; }
UInt64 Value::asUInt64()const { JSON_ASSERT(isNumber()); return data_.u64; }
Float Value::asFloat()const { JSON_ASSERT(isNumber()); return data_.f; }
Double Value::asDouble()const { JSON_ASSERT(isNumber()); return data_.d; }
String Value::asString()const { JSON_ASSERT(isString());	return *data_.s; }
Array Value::asArray()const { JSON_ASSERT(isArray()); return *data_.a; }
Object Value::asObject()const { JSON_ASSERT(isObject()); return *data_.o; }
void Value::swap(Value& other) {
	std::swap(data_, other.data_);
	std::swap(type_, other.type_);
}
bool Value::remove(const String& index) {
	if (isObject()) {
		data_.o->erase(index);
		return true;
	}
	return false;
}
bool Value::remove(size_t index) {
	if (isArray() && index < size()) {
		data_.a->erase(data_.a->begin() + index);
		return true;
	}
	return false;
}
void Value::append(const Value& value) {
	append(Value(value));
}
void Value::append(Value&& value) {
	JSON_ASSERT(isArray() || isNull());
	if (isNull())
		*this = kArray;
	data_.a->push_back(move(value));
}
size_t Value::size()const {
	switch (type_) {
	case kNull:
		return 0;
	case kBoolean:
		break;
	case kInteger:
		break;
	case kUInteger:
		break;
	case kReal:
		break;
	case kString:
		return data_.s->size();
	case kArray:
		return data_.a->size();
	case kObject:
		return data_.o->size();
	default:
		break;
	}
	return 1;
}
bool Value::empty()const {
	switch (type_) {
	case kNull:
		return true;
	case kBoolean:
		break;
	case kInteger:
		break;
	case kUInteger:
		break;
	case kReal:
		break;
	case kString:
		return data_.s->empty();
	case kArray:
		return data_.a->empty();
	case kObject:
		return data_.o->empty();
	default:
		break;
	}
	return false;
}
bool Value::contains(const String& key)const {
	if (isObject())
		return data_.o->find(key) != data_.o->end();
	return false;
}
void Value::clear() {
	switch (type_) {
	case kNull:
		break;
	case kBoolean:
		break;
	case kInteger:
		break;
	case kUInteger:
		break;
	case kReal:
		break;
	case kString:
		data_.s->clear();
		break;
	case kArray:
		data_.a->clear();
		break;
	case kObject:
		data_.o->clear();
		break;
	default:
		break;
	}
	type_ = kNull;
}
String Value::dump(UInt indent_count, bool emit_utf8)const {
	Writer w(indent_count, emit_utf8);
	if (indent_count > 0)
		w.writePrettyValue(*this);
	else
		w.writeValue(*this);
	return w.getOutput();
}
std::ostream& operator<<(std::ostream& os, const Value& value) {
	return os << value.dump();
}
Value Parse(StringView str, String* err, bool allow_comments) {
	Value value;
	Value::Parser parser;
	bool success = parser.parse(str, value, allow_comments);
	if (err && !success) {
		*err = parser.getError();
	}
	return value;
}
} // namespace Json