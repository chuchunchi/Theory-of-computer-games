/**
 * Framework for Threes! and its variants (C++ 11)
 * episode.h: Data structure for storing an episode
 *
 * Author: Theory of Computer Games
 *         Computer Games and Intelligence (CGI) Lab, NYCU, Taiwan
 *         https://cgilab.nctu.edu.tw/
 */

#pragma once
#include <list>
#include <vector>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <chrono>
#include <numeric>
#include "board.h"
#include "action.h"
#include "agent.h"

class episode {
public:
	episode() : ep_state(initial_state()), ep_score(0), ep_time(0) { ep_moves.reserve(10000); }

public:
	board& state() { return ep_state; }
	const board& state() const { return ep_state; }
	board::score score() const { return ep_score; }

	void open_episode(const std::string& tag) {
		ep_open = { tag, millisec() };
	}
	void close_episode(const std::string& tag) {
		ep_close = { tag, millisec() };
	}
	bool apply_action(action move) {
		board::reward reward = move.apply(state());
		if (reward == -1) return false;
		ep_moves.emplace_back(move, reward, millisec() - ep_time);
		ep_score += reward;
		return true;
	}
	agent& take_turns(agent& slide, agent& place) {
		ep_time = millisec();
		return step() >= 9 && (step() - 8) % 2 ? slide : place;
	}
	agent& last_turns(agent& slide, agent& place) {
		return step() >= 9 ? take_turns(place, slide) : place;
	}

public:
	size_t step(unsigned who = -1u) const {
		size_t size = ep_moves.size();
		switch (who) {
		case action::slide::type: return size > 9 ? (size) / 2 - 4 : 0;
		case action::place::type: return size > 9 ? (size - 1) / 2 + 5 : size;
		default:                  return size;
		}
	}

	time_t time(unsigned who = -1u) const {
		time_t time = 0;
		size_t i = 9;
		switch (who) {
		case action::place::type:
			if (ep_moves.size())
				for (i = 0; i < 8; i++) time += ep_moves[i].time;
			// no break;
		case action::slide::type:
			while (i < ep_moves.size()) time += ep_moves[i].time, i += 2;
			break;
		default:
			time = ep_close.when - ep_open.when;
			break;
		}
		return time;
	}

	std::vector<action> actions(unsigned who = -1u) const {
		std::vector<action> res;
		size_t i = 9;
		switch (who) {
		case action::place::type:
			if (ep_moves.size())
				for (i = 0; i < 8; i++) res.push_back(ep_moves[i]);
			// no break;
		case action::slide::type:
			while (i < ep_moves.size()) res.push_back(ep_moves[i]), i += 2;
			break;
		default:
			res.assign(ep_moves.begin(), ep_moves.end());
			break;
		}
		return res;
	}

public:

	friend std::ostream& operator <<(std::ostream& out, const episode& ep) {
		out << ep.ep_open << '|';
		for (const move& mv : ep.ep_moves) out << mv;
		out << '|' << ep.ep_close;
		return out;
	}
	friend std::istream& operator >>(std::istream& in, episode& ep) {
		ep = {};
		std::string token;
		std::getline(in, token, '|');
		std::stringstream(token) >> ep.ep_open;
		std::getline(in, token, '|');
		for (std::stringstream moves(token); !moves.eof(); moves.peek()) {
			ep.ep_moves.emplace_back();
			moves >> ep.ep_moves.back();
			ep.ep_score += action(ep.ep_moves.back()).apply(ep.ep_state);
		}
		std::getline(in, token, '|');
		std::stringstream(token) >> ep.ep_close;
		return in;
	}

protected:

	struct move {
		action code;
		board::reward reward;
		time_t time;
		move(action code = {}, board::reward reward = 0, time_t time = 0) : code(code), reward(reward), time(time) {}

		operator action() const { return code; }
		friend std::ostream& operator <<(std::ostream& out, const move& m) {
			out << m.code;
			if (m.reward) out << '[' << std::dec << m.reward << ']';
			if (m.time) out << '(' << std::dec << m.time << ')';
			return out;
		}
		friend std::istream& operator >>(std::istream& in, move& m) {
			in >> m.code;
			m.reward = 0;
			m.time = 0;
			if (in.peek() == '[') {
				in.ignore(1);
				in >> std::dec >> m.reward;
				in.ignore(1);
			}
			if (in.peek() == '(') {
				in.ignore(1);
				in >> std::dec >> m.time;
				in.ignore(1);
			}
			return in;
		}
	};

	struct meta {
		std::string tag;
		time_t when;
		meta(const std::string& tag = "N/A", time_t when = 0) : tag(tag), when(when) {}

		friend std::ostream& operator <<(std::ostream& out, const meta& m) {
			return out << m.tag << "@" << std::dec << m.when;
		}
		friend std::istream& operator >>(std::istream& in, meta& m) {
			return std::getline(in, m.tag, '@') >> std::dec >> m.when;
		}
	};

	static board initial_state() {
		return {};
	}
	static time_t millisec() {
		auto now = std::chrono::system_clock::now().time_since_epoch();
		return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
	}

private:
	board ep_state;
	board::score ep_score;
	std::vector<move> ep_moves;
	time_t ep_time;

	meta ep_open;
	meta ep_close;
};
