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
#include <algorithm>
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
	weight_agent(const std::string& args = "") : agent(args), alpha(0.1/32), lambda(0), trained(0) {
		if (meta.find("init") != meta.end())
			init_weights(meta["init"]);
		if (meta.find("load") != meta.end())
			load_weights(meta["load"]);
		if (meta.find("alpha") != meta.end())
			alpha = float(meta["alpha"]); 
		if (meta.find("lambda") != meta.end())
			lambda = float(meta["lambda"]); 
		if (meta.find("seed") != meta.end())
			engine.seed(int(meta["seed"]));
	}
	virtual ~weight_agent() {
		if (meta.find("save") != meta.end()){
			save_weights(meta["save"]);
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
	float lambda;
	int trained;
	std::default_random_engine engine;
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
int n=4;
int tup[4][6] = { {0, 1, 2, 3, 4, 5},
                    {4, 5, 6, 7, 8, 9},
                    {5, 6, 7, 9, 10, 11},
                    {9, 10, 11, 13, 14, 15}};
int margin[4][4] = { {12, 13, 14, 15},
						{0, 4, 8, 12},
						{0, 1, 2, 3},
						{3, 7, 11, 15} };

class weight_slider : public weight_agent {
public:
	weight_slider(const std::string& args = "") : weight_agent("name=slide role=slider " + args) {}
	virtual void open_episode(const std::string& flag = "") {
        trained = 0;
    }
	virtual action take_action(const board& before) {
		
		int bestop=-1;
		if(!trained) prev=before; // if the first step
		/*
		double bestval=-10000000;
		for(int op=0;op<4;op++){
			board board1 = before;
			//cout << board1.hint() << endl;
			board::reward reward1 =board1.slide(op);
			// choose the best operation base on current weight table.
			if(reward1 != -1){
				double value = get_value(board1);
				if(reward1+value>bestval){
					bestval = reward1+value;
					bestop=op;
				}
			}
		}*/
		bestop = expectimax(before);
		
		if(bestop!=-1){
			prev = next;
			return action::slide(bestop);
		}
		else{ // bestop==-1 -> no available move
			return action();
		}
		
	}	
	int expectimax(const board& before){
		int final_bestop = -1;
		double final_bestval = -1e15;
		int bestop2[4] = {-1,-1,-1,-1};
		for(int op=0;op<4;op++){
			board board1 = board(before);
			board::reward reward1 = board1.slide(op);
			
			int poscount=4;
			double valcount=0;
			for(int i=0;i<4;i++){
				board board2 = board(board1);
				int p = margin[op][i];
				//generate random hint
				int bag[3], num = 0;
				for (board::cell t = 1; t <= 3; t++)
					for (size_t it = 0; it < board2.bag(t); it++)
						bag[num++] = t;
				std::shuffle(bag, bag + num, engine);
				board::cell hint = bag[--num];
				int val = board2.place(p, board2.hint(), hint);
				if(val<0){
					poscount--;
					continue;
				}
				double bestval=-10000000;
				for(int op2=0;op2<4;op2++){
					board board3 = board(board2);
					board::reward reward2 = board3.slide(op2);
					// choose the best operation base on current weight table.
					if(reward2 != -1){
						double value = get_value(board3);
						if(reward2+value>bestval){
							bestval = reward2+value;
							bestop2[op] = op2;
						}
					}
				}
				if (bestval > -10000000) valcount += bestval;
				//else poscount--;
			}
			
			double opvalue=0;
			if(poscount!=0){
				opvalue = reward1 + get_value(board1) + valcount / poscount;
				//cout << opvalue << endl;
			}
			//double opvalue = reward1 + get_value(board1) + Expectimax(board1, op, 1);
			if(reward1 != -1){

				if(opvalue > final_bestval){
					final_bestval = opvalue;
					final_bestop = op;
				}
			}
		}
		next = before;
		board::reward nextreward = next.slide(final_bestop);
		if(trained>=2) TDlearn(nextreward); // if not the first step
		trained += 1;
		if(lambda!=0){
			nextnext = next;
			board::reward nextreward2 = nextnext.slide(bestop2[final_bestop]);
			if(trained>=2) TD2step(nextreward,nextreward2); // if not the first step
			trained += 1;
		}
		return final_bestop;
	}
	
	double get_value(board& b){
		double value=0;
		//for 8*4 tuple
		/*
		for(int i=0;i<n;i++){
			value += net[i][b2feature(b,i];
		}*/

		//for 4*6 tuple
		for(int i=0;i<2;i++){
			for(int j=0;j<4;j++){
				for(int f=0;f<n;f++){
					value += net[f][b2feature(b,f)];
				}
				b.rotate_clockwise();
			}
			b.reflect_vertical();
		}
		return value;
	}

	void TDlearn(double reward){
		double TDerr;
		if(reward==-1) TDerr = alpha*(-get_value(prev));
		else TDerr = alpha*(reward+get_value(next)-get_value(prev));
		
		/* for 8*4-tuple
		for(int i=0;i<n;i++){
			net[i][b2feature(prev,i)] += TDerr;
		}*/
		// for 4*6-tuple
		for(int i=0;i<2;i++){
			for(int j=0;j<4;j++){
				for(int f=0;f<n;f++){
					net[f][b2feature(prev,f)] += TDerr;
				}
				prev.rotate_clockwise();
			}
			prev.reflect_vertical();
		}
	}
	void TD2step(double reward, double reward2){
		double TDerr;
		if(reward==-1) TDerr = -get_value(prev);
		else{
			TDerr = reward+get_value(next)-get_value(prev);
			//Add 2 step TD
			if(reward2!=-1){
				TDerr = (1-lambda)*TDerr + lambda * (1-lambda) * ( reward + reward2 + get_value(nextnext) - get_value(next) );
			}
			else{
				TDerr = (1-lambda)*TDerr + lambda * (1-lambda) * ( reward + reward2 - get_value(next) );
			}
		} 
		
		TDerr *= alpha;

		/* for 8*4-tuple
		for(int i=0;i<n;i++){
			net[i][b2feature(prev,i)] += TDerr;
		}*/
		// for 4*6-tuple
		for(int i=0;i<2;i++){
			for(int j=0;j<4;j++){
				for(int f=0;f<n;f++){
					net[f][b2feature(prev,f)] += TDerr;
				}
				prev.rotate_clockwise();
			}
			prev.reflect_vertical();
		}
	}
	long long int  b2feature(board& b,int f){//board to feature
		long long int ret=0;
		int weight[6] = {1,15,225,3375,50625,759375};
		for(int c=0;c<6;c++){
			int cell_index = tup[f][c];
			board::cell tmpcell=b(cell_index);
			ret+=weight[c]*tmpcell;
		}
		return ret;
	}

private:
	board next;
	board prev;
	board nextnext;
};	