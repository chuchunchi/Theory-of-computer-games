/**
 * Framework for Threes! and its variants (C++ 11)
 * weight.h: Lookup table template for n-tuple network
 *
 * Author: Theory of Computer Games
 *         Computer Games and Intelligence (CGI) Lab, NYCU, Taiwan
 *         https://cgilab.nctu.edu.tw/
 */

#pragma once
#include <iostream>
#include <vector>
#include <utility>

class weight {
public:
	typedef float type;

public:
	weight() {}
	weight(size_t len) : value(len) {}
	weight(weight&& f) : value(std::move(f.value)) {}
	weight(const weight& f) = default;

	weight& operator =(const weight& f) = default;
	type& operator[] (size_t i) { return value[i]; }
	const type& operator[] (size_t i) const { return value[i]; }
	size_t size() const { return value.size(); }

public:
	friend std::ostream& operator <<(std::ostream& out, const weight& w) {
		auto& value = w.value;
		uint64_t size = value.size();
		out.write(reinterpret_cast<const char*>(&size), sizeof(uint64_t));
		out.write(reinterpret_cast<const char*>(value.data()), sizeof(type) * size);
		return out;
	}
	friend std::istream& operator >>(std::istream& in, weight& w) {
		auto& value = w.value;
		uint64_t size = 0;
		in.read(reinterpret_cast<char*>(&size), sizeof(uint64_t));
		value.resize(size);
		in.read(reinterpret_cast<char*>(value.data()), sizeof(type) * size);
		return in;
	}

protected:
	std::vector<type> value;
};
