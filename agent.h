/**
 * Framework for Threes! and its variants (C++ 11)
 * agent.h: Define the behavior of variants of agents including players and environments
 *
 * Author: Theory of Computer Games
 *         Computer Games and Intelligence (CGI) Lab, NYCU, Taiwan
 *         https://cgilab.nctu.edu.tw/
 */

#pragma once
#include <string>
#include <random>
#include <sstream>
#include <map>
#include <type_traits>
#include <algorithm>
#include <fstream>
#include "board.h"
#include "action.h"
#include "weight.h"
#include <vector>
using namespace std;
class agent {
public:
	agent(const std::string& args = "") {
		std::stringstream ss("name=unknown role=unknown " + args);
		for (std::string pair; ss >> pair; ) {
			std::string key = pair.substr(0, pair.find('='));
			std::string value = pair.substr(pair.find('=') + 1);
			meta[key] = { value };
		}
	}
	virtual ~agent() {}
	virtual void open_episode(const std::string& flag = "") {}
	virtual void close_episode(const std::string& flag = "") {}
	virtual action take_action(const board& b) { return action(); }
	virtual bool check_for_win(const board& b) { return false; }

public:
	virtual std::string property(const std::string& key) const { return meta.at(key); }
	virtual void notify(const std::string& msg) { meta[msg.substr(0, msg.find('='))] = { msg.substr(msg.find('=') + 1) }; }
	virtual std::string name() const { return property("name"); }
	virtual std::string role() const { return property("role"); }

protected:
	typedef std::string key;
	struct value {
		std::string value;
		operator std::string() const { return value; }
		template<typename numeric, typename = typename std::enable_if<std::is_arithmetic<numeric>::value, numeric>::type>
		operator numeric() const { return numeric(std::stod(value)); }
	};
	std::map<key, value> meta;
};

/**
 * base agent for agents with randomness
 */
class random_agent : public agent {
public:
	random_agent(const std::string& args = "") : agent(args) {
		if (meta.find("seed") != meta.end())
			engine.seed(int(meta["seed"]));
	}
	virtual ~random_agent() {}

protected:
	std::default_random_engine engine;
};

/**
 * base agent for agents with weight tables and a learning rate
 */
class weight_agent : public agent {
public:
	weight_agent(const std::string& args = "") : agent(args), alpha(0), trained(0) {
		if (meta.find("init") != meta.end())
			init_weights(meta["init"]);
		if (meta.find("load") != meta.end())
			load_weights(meta["load"]);
		if (meta.find("alpha") != meta.end())
			alpha = float(meta["alpha"]); 
	}
	virtual ~weight_agent() {
		if (meta.find("save") != meta.end()){
			save_weights(meta["save"]);
			trained = 1;
		}
	}

protected:
	virtual void init_weights(const std::string& info) {
		std::string res = info; // comma-separated sizes, e.g., "65536,65536"
		for (char& ch : res)
			if (!std::isdigit(ch)) ch = ' ';
		std::stringstream in(res);
		for (size_t size; in >> size; net.emplace_back(size));
	}
	virtual void load_weights(const std::string& path) {
		std::ifstream in(path, std::ios::in | std::ios::binary);
		if (!in.is_open()) std::exit(-1);
		uint32_t size;
		in.read(reinterpret_cast<char*>(&size), sizeof(size));
		net.resize(size);
		for (weight& w : net) in >> w;
		in.close();
	}
	virtual void save_weights(const std::string& path) {
		std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!out.is_open()) std::exit(-1);
		uint32_t size = net.size();
		out.write(reinterpret_cast<char*>(&size), sizeof(size));
		for (weight& w : net) out << w;
		out.close();
	}

protected:
	std::vector<weight> net;
	float alpha;
	int trained;
};

/**
 * default random environment, i.e., placer
 * place the hint tile and decide a new hint tile
 */
class random_placer : public random_agent {
public:
	random_placer(const std::string& args = "") : random_agent("name=place role=placer " + args) {
		spaces[0] = { 12, 13, 14, 15 };
		spaces[1] = { 0, 4, 8, 12 };
		spaces[2] = { 0, 1, 2, 3};
		spaces[3] = { 3, 7, 11, 15 };
		spaces[4] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
	}

	virtual action take_action(const board& after) {
		std::vector<int> space = spaces[after.last()];
		std::shuffle(space.begin(), space.end(), engine);
		for (int pos : space) {
			if (after(pos) != 0) continue;

			int bag[3], num = 0;
			for (board::cell t = 1; t <= 3; t++)
				for (size_t i = 0; i < after.bag(t); i++)
					bag[num++] = t;
			std::shuffle(bag, bag + num, engine);

			board::cell tile = after.hint() ?: bag[--num];
			board::cell hint = bag[--num];

			return action::place(pos, tile, hint);
		}
		return action();
	}

private:
	std::vector<int> spaces[5];
};

/**
 * random player, i.e., slider
 * select a legal action randomly
 */
class random_slider : public random_agent {
public:
	random_slider(const std::string& args = "") : random_agent("name=slide role=slider " + args),
		opcode({ 0, 1, 2, 3 }) {}

	virtual action take_action(const board& before) {
		std::shuffle(opcode.begin(), opcode.end(), engine);
		for (int op : opcode) {
			board::reward reward = board(before).slide(op);
			if (reward != -1) return action::slide(op);
		}
		return action();
	}

private:
	std::array<int, 4> opcode;
};

class reward_slider : public random_agent {
public:
	reward_slider(const std::string& args = "") : random_agent("name=slide role=slider " + args),
		                opcode({ 0, 1, 2, 3 }) {}

	virtual action take_action(const board& before) {
		board::reward best_reward = -1;
		int best_op = 0;
	        for (int op : opcode) {
	                board::reward reward = board(before).slide(op);
			if(reward > best_reward){
				best_reward = reward;
				best_op = op;
			}
			/*if (reward != -1) return action::slide(op);*/
		}
		return action::slide(best_op);
                return action();
        }
private:
        std::array<int, 4> opcode;
};

class reward2_slider : public random_agent {
public:
	reward2_slider(const std::string& args = "") : random_agent("name=slide role=slider " + args), opcode({0,1,2,3}) {}
	virtual action take_action(const board& before) {
		board::reward best_reward = -1;
		int best_op = 0;
		for (int op : opcode) {
			board board1 = board(before);
			board::reward reward1 = board1.slide(op);
			if(reward1 != -1){
				for (int op2 : opcode) {
					board board2 = board(board1);
					board::reward reward2 = board2.slide(op2);
					if(reward1+reward2 > best_reward){
						best_reward = reward1+reward2;
						best_op = op;
					}
				}
			}
		}
		return action::slide(best_op);
		return action();
	}	
private:
	std::array<int, 4> opcode;
};	
int n=8;
int tuple[8][4] = { {0, 1, 2, 3},
                    {4, 5, 6, 7},
                    {8, 9, 10, 11},
                    {12, 13, 14, 15},
                    {0, 4, 8, 12},
                    {1, 5, 9, 13},
                    {2, 6, 10, 14},
                    {3, 7, 11, 15}};

class weight_slider : public weight_agent {
public:
	weight_slider(const std::string& args = "") : weight_agent("name=slide role=slider " + args) {}
	virtual action take_action(const board& before) {
		board::reward reward1;
		double bestval=-1;
		int bestop=0;
		for(int op=0;op<4;op++){
			board board1 = before;
			board::reward reward1 =board1.slide(op);
			if(reward1 != -1){
				double value = get_value(board1);
				if(reward1+value>bestval){
					bestval = reward1+value;
					bestop=op;
				}
			}
		}
		cout << "here\n";
		next = before;
		prev = before;
		board::reward nextreward = next.slide(bestop);
		if(trained==0){
			TDlearn(nextreward);
		}
		return action::slide(bestop);
		return action();
	}	
	double get_value(board& b){
		double value=0;
		vector<string> feat = b2feature(b);
		for(int i=0;i<n;i++){
			value+=net[i][stoi(feat[i])];
		}
		return value;
	}
	void TDlearn(double reward){
		double TDerr = alpha*(reward+get_value(next)-get_value(prev));
		vector<string> feat = b2feature(prev);
		for(int i=0;i<n;i++){
			net[i][stoi(feat[i])] += TDerr;
		}
	}
	vector<string> b2feature(board b){//board to feaature
		vector<string> ret(n); 
		for(int ro=0;ro<5;ro+=4){
			for(int i=0;i<4;i++){
				board::row tmprow = b[i];
				for(int j=0;j<4;j++){
					board::cell tmpcell = tmprow[j];
					switch(tmpcell){
						case 1: case 2: case 3:
							ret[i+ro]+=to_string(tmpcell);
							break;
						case 6:
							ret[i+ro]+=to_string(4);
							break;
						case 12:
							ret[i+ro]+=to_string(5);
							break;
						case 24:
							ret[i+ro]+=to_string(6);
							break;
						case 48:
							ret[i+ro]+=to_string(7);
							break;
						case 96:
							ret[i+ro]+=to_string(8);
							break;
						case 192:
							ret[i+ro]+=to_string(9);
							break;
						case 384:
							ret[i+ro]+=to_string(10);
							break;
						case 768:
							ret[i+ro]+=to_string(11);
							break;
						case 1536:
							ret[i+ro]+=to_string(12);
							break;
						case 3072:
							ret[i+ro]+=to_string(13);
							break;
					}
					ret[i+ro]+=to_string(0);
				}
			}
			b.transpose();
		}
		return ret;
	}
private:
	board next;
	board prev;
};	
