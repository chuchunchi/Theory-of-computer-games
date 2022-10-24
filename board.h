/**
 * Framework for Threes! and its variants (C++ 11)
 * board.h: Define the game state and basic operations of the game of Threes!
 *
 * Author: Theory of Computer Games
 *         Computer Games and Intelligence (CGI) Lab, NYCU, Taiwan
 *         https://cgilab.nctu.edu.tw/
 */

#pragma once
#include <array>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cmath>

/**
 * array-based board for Threes!
 *
 * index (1-d form):
 *  (0)  (1)  (2)  (3)
 *  (4)  (5)  (6)  (7)
 *  (8)  (9) (10) (11)
 * (12) (13) (14) (15)
 *
 */
class board {
public:
	typedef uint32_t cell;
	typedef std::array<cell, 4> row;
	typedef std::array<row, 4> grid;
	typedef uint64_t data;
	typedef uint64_t score;
	typedef int reward;

public:
	board() : tile(), attr(0) { reset(); }
	board(const grid& b, data v = 0) : tile(b), attr(v) {}
	board(const board& b) = default;
	board& operator =(const board& b) = default;

	operator grid&() { return tile; }
	operator const grid&() const { return tile; }
	row& operator [](unsigned i) { return tile[i]; }
	const row& operator [](unsigned i) const { return tile[i]; }
	cell& operator ()(unsigned i) { return tile[i / 4][i % 4]; }
	const cell& operator ()(unsigned i) const { return tile[i / 4][i % 4]; }

	cell* begin() { return &(operator()(0)); }
	const cell* begin() const { return &(operator()(0)); }
	cell* end() { return begin() + 16; }
	const cell* end() const { return begin() + 16; }

	data info() const { return attr; }
	data info(data dat) { data old = attr; attr = dat; return old; }

private:
	data info4(size_t i) const { return (info() >> (4 * i)) & 0x0fu; }
	data info4(size_t i, data dat) { data old = info4(i); info(info() ^ ((old ^ dat) << (4 * i))); return old; }

public:
	static unsigned itot(unsigned i) { return i >= 3 ? 3 * (1 << (i - 3)) : i; }
	static unsigned ttoi(unsigned t) { return t >= 3 ? std::log2(t / 3) + 3 : t; }
	static unsigned itov(unsigned i) { return ttov(itot(i)); }
	static unsigned ttov(unsigned t) { return t >= 3 ? std::pow(3, std::log2(t / 3) + 1) : 0; }

	cell hint() const { return info4(0); }
	cell hint(cell t) { return info4(0, t); }
	unsigned last() const { return info4(1); }
	unsigned last(unsigned a) { return info4(1, a); }
	unsigned bag(cell t) const { return info4(t + 1); }
	unsigned bag(cell t, unsigned n) { return info4(t + 1, n); }

	void reset() {
		hint(0);
		last(4);
		reset_bag();
	}
	void reset_bag() {
		for (cell t = 1; t <= 3; t++) bag(t, 1);
	}
	bool extract_hint_from_bag(cell t) {
		if (bag(t) < 1) return false;
		bag(t, bag(t) - 1);
		if (bag(1) + bag(2) + bag(3) == 0) reset_bag();
		hint(t);
		return true;
	}
	unsigned value() const {
		score v = 0;
		for (cell t : *this) v += board::itov(t);
		return v;
	}

public:
	bool operator ==(const board& b) const { return tile == b.tile; }
	bool operator < (const board& b) const { return tile <  b.tile; }
	bool operator !=(const board& b) const { return !(*this == b); }
	bool operator > (const board& b) const { return b < *this; }
	bool operator <=(const board& b) const { return !(b < *this); }
	bool operator >=(const board& b) const { return !(*this < b); }

public:

	/**
	 * place a tile (index value) to the specific position (1-d index)
	 * return >= 0 if the action is valid, or -1 if not
	 */
	reward place(unsigned pos, cell tile, cell hint_tile) {
		data bak = info();
		if (pos >= 16 || operator()(pos)) return -1;
		if (hint() == 0 && !extract_hint_from_bag(tile)) return -1;
		if (hint() != tile) return info(bak), -1;
		if (!extract_hint_from_bag(hint_tile)) return info(bak), -1;
		operator()(pos) = tile;
		last(4);
		return itov(tile);
	}

	/**
	 * apply an action to the board
	 * return the reward of the action, or -1 if the action is illegal
	 */
	reward slide(unsigned opcode) {
		reward r = -1;
		switch (opcode & 0b11) {
		case 0: r = slide_up(); break;
		case 1: r = slide_right(); break;
		case 2: r = slide_down(); break;
		case 3: r = slide_left(); break;
		}
		if (r != -1) last(opcode & 0b11);
		return r;
	}

	reward slide_left() {
		bool moved = false;
		reward score = 0;
		for (int r = 0; r < 4; r++) {
			auto& row = tile[r];
			for (int c = 1; c < 4; c++) {
				auto& t0 = row[c - 1];
				auto& t1 = row[c];
				if (t0 == 0) {
					t0 = t1;
					t1 = 0;
					moved |= (t0 != 0);
				} else if (t1 != 0 && ((t0 + t1 == 3) || (t0 == t1 && t0 >= 3 && t0 < 14))) {
					t0 = std::max(t0, t1) + 1;
					t1 = 0;
					score += itov(t0) - itov(t0 - 1) * 2;
					moved = true;
				}
			}
		}
		return (moved) ? score : -1;
	}
	reward slide_right() {
		reflect_horizontal();
		reward score = slide_left();
		reflect_horizontal();
		return score;
	}
	reward slide_up() {
		rotate_clockwise();
		reward score = slide_right();
		rotate_counterclockwise();
		return score;
	}
	reward slide_down() {
		rotate_clockwise();
		reward score = slide_left();
		rotate_counterclockwise();
		return score;
	}

	void rotate(int clockwise_count = 1) {
		switch (((clockwise_count % 4) + 4) % 4) {
		default:
		case 0: break;
		case 1: rotate_clockwise(); break;
		case 2: reverse(); break;
		case 3: rotate_counterclockwise(); break;
		}
	}

	void rotate_clockwise() { transpose(); reflect_horizontal(); }
	void rotate_counterclockwise() { transpose(); reflect_vertical(); }
	void reverse() { reflect_horizontal(); reflect_vertical(); }

	void reflect_horizontal() {
		for (int r = 0; r < 4; r++) {
			std::swap(tile[r][0], tile[r][3]);
			std::swap(tile[r][1], tile[r][2]);
		}
	}

	void reflect_vertical() {
		for (int c = 0; c < 4; c++) {
			std::swap(tile[0][c], tile[3][c]);
			std::swap(tile[1][c], tile[2][c]);
		}
	}

	void transpose() {
		for (int r = 0; r < 4; r++) {
			for (int c = r + 1; c < 4; c++) {
				std::swap(tile[r][c], tile[c][r]);
			}
		}
	}

public:
	friend std::ostream& operator <<(std::ostream& out, const board& b) {
		out << "+------------------------+" << std::endl;
		for (int i = 0; i < 4; i++) {
			auto& row = b[i];
			out << "|" << std::dec;
			for (auto t : row) out << std::setw(6) << itot(t);
			out << "|";
			switch (i) {
			case 0: out << " Hint: " << "X123+"[b.hint()]; break;
			case 1: out << " Last: " << "URDLX"[b.last()]; break;
			}
			out << std::endl;
		}
		out << "+------------------------+" << std::endl;
		return out;
	}
	friend std::istream& operator >>(std::istream& in, board& b) {
		for (int i = 0; i < 16; i++) {
			while (!std::isdigit(in.peek()) && in.good()) in.ignore(1);
			in >> b(i);
			b(i) = ttoi(b(i));
		}
		return in;
	}

private:
	grid tile;
	data attr; // (#3-tile:4-bit) (#2-tile:4-bit) (#1-tile:4-bit) (last_action:4-bit) (hint_tile:4-bit)
};
