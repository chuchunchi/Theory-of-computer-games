/**
 * Framework for NoGo and similar games (C++ 11)
 * agent.h: Define the behavior of variants of the player
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
#include <vector>
#include <chrono>
#include <ctime> 
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
 * random player for both side
 * put a legal piece randomly
 */
class player : public random_agent {
public:
	player(const std::string& args = "") : random_agent("name=random role=unknown " + args),
		space(board::size_x * board::size_y), who(board::empty) {
		if (name().find_first_of("[]():; ") != std::string::npos)
			throw std::invalid_argument("invalid name: " + name());
		if (role() == "black") who = board::black;
		if (role() == "white") who = board::white;
		if (who == board::empty)
			throw std::invalid_argument("invalid role: " + role());
		for (size_t i = 0; i < space.size(); i++)
			space[i] = action::place(i, who);
	}

	virtual action take_action(const board& state) {
		std::shuffle(space.begin(), space.end(), engine);
		for (const action::place& move : space) {
			board after = state;
			if (move.apply(after) == board::legal)
				return move;
		}
		return action();
	}

private:
	std::vector<action::place> space;
	board::piece_type who;
};

class MCTS{
	private:
		struct Node{
			int visittime;
			int wintime;
			vector<Node*> childs;
			board position;
			Node(): visittime(0), wintime(0){}
			Node(const board& b): visittime(0), wintime(0), position(b){}
		};
	public:
		Node* root;
		vector<action::place> myspace;
		vector<action::place> oppospace;
		board::piece_type who;
		std::default_random_engine engine;
		MCTS(board::piece_type who): myspace(board::size_x * board::size_y), oppospace(board::size_x * board::size_y){
			cout << "mcts constructor\n";
			//who = who;
			//root = new Node(b);
			for (size_t i = 0; i < myspace.size(); i++)
				myspace[i] = action::place(i, who);
			for (size_t i = 0; i < oppospace.size(); i++)
				oppospace[i] = action::place(i, who);
		}
		void setroot(const board& state){
			root = new Node(state);
			expand(root, true);
		}
		Node* select(Node* curnode){
			int bestvalue=-10000;
			Node* bestnode = new Node();
			for(int i=0;i<(int)curnode->childs.size();i++){
				Node* child = curnode->childs[i];
				double val = uctvalue(*child, curnode->visittime);
				if(bestvalue < val){
					bestvalue = val;
					bestnode = child;
				}
			}
			//TODO: handle leaf node??
			if(bestnode==NULL) cout << "error: no child to select\n";
			return bestnode;
		}
		void expand(Node* node, bool myturn){
			//cout << "in expand\n";
			vector <Node*> children;
			std::vector<action::place>& tmpspace = (myturn)? myspace : oppospace;
			for(int i=0; i < (int) tmpspace.size();i++){
				//cout << "inside 155 for loop\n";
				action::place& nextmove = tmpspace[i];
				//cout << 157 << endl;
				board cur = node->position;
				//cout << 159 << endl;
				if(nextmove.apply(cur) == board::legal){
					//cout << "161" << endl;
					Node* child = new Node(cur);
					children.push_back(child);
				}
			}
			shuffle(children.begin(), children.end(), engine);
			//cout << children.size() << endl;
			node->childs = children;
		}
		pair<action::place, int> rand_action(board& state, bool myturn){

			std::vector<action::place> tmpspace = (myturn)? myspace : oppospace;
			std::shuffle(tmpspace.begin(), tmpspace.end(), engine);
			for (const action::place& move : tmpspace) {
				board after = state;
				if (move.apply(after) == board::legal){
					return make_pair(move, 1);
				}
			}
			return make_pair(tmpspace[0], 0); //illegal move
		}
		int simulate(const board& state, bool myturn){
			action::place move;
			
			int iswin = 0;
			board tmp = state;
			pair<action::place, int> p = rand_action(tmp,myturn);
			int islegal = p.second;

			while(p.first.apply(tmp)==board::legal){
				myturn = !myturn;
				p = rand_action(tmp, myturn);
				iswin ++;
				iswin = iswin % 2;
				islegal = p.second;
			}
			return iswin;
			//TODO: should return win or lose
		}
			
		void update(Node* node, int iswin){
			node->visittime++;
			node->wintime += iswin;
		}
		double uctvalue(Node& child, int cur_visittime){
			if(child.visittime==0){
				return 100000000;  // devide by zero
			}
			float c = 1.414;
			float exploitation = (float)child.wintime / (float)(child.visittime);
			float exploration = sqrt(log(cur_visittime) / (float)(child.visittime));
			return exploitation + c * exploration;
		}

		void mcts_simulate(int simulations){
			auto start = std::chrono::system_clock::now();
			auto end = std::chrono::system_clock::now();
			std::chrono::duration<double> elapsed_seconds;
			while(elapsed_seconds.count()<0.999){
				//cout << "simulation # " << endl;
				sim(root);
				end = std::chrono::system_clock::now();
				elapsed_seconds = end-start;
			}
			
			
			//cout << "elapsed time: " << elapsed_seconds.count() << endl;
		}
		int sim(Node* node, bool myturn=true){
			int iswin;
			if(node->childs.empty()){
				expand(node, myturn);
				iswin = simulate(node->position, myturn);
				
				update(node,iswin);
			}
			else{
				Node* next = select(node);
				iswin = sim(next, !myturn);
				update(node, iswin);
			}
			return iswin;
		}
		action::place bestaction(){
			int best = 0;
			if(root->childs.empty()){
				return action::place(0,who);
				/*pair<action::place, int> p  = rand_action(root->position, true);
				if(p.second){
					return p.first;
				}*/
			}
			else{
				Node* bestchild = root->childs[0];
				for(int i=0; i<(int)root->childs.size(); i++){
					Node* child = root->childs[i];
					if(child->visittime > best){
						best = child->visittime;
						bestchild = child;
					}
				}
				std::vector<action::place> tmpspace = myspace;
				for(int i=0;i<(int)tmpspace.size();i++){
					action::place next = tmpspace[i];
					board nextboard = root->position;
					if(next.apply(nextboard) == board::legal && nextboard == bestchild->position){
						return next;
					}
				}
			}
			cout << "no available action\n";
			return action();
		}
		void del_tree(Node* node=nullptr) {
			if (node == nullptr)
				node = root;
			for (int i = 0; i < (int)node->childs.size(); i++)
				del_tree(node->childs[i]);
			delete node;
		}
};

class mctsplayer : public agent {
	public:
		mctsplayer(const std::string& args = "") : agent("name=mcts role=unknown " + args),
			space(board::size_x * board::size_y), who(board::empty) {
			if (name().find_first_of("[]():; ") != std::string::npos)
				throw std::invalid_argument("invalid name: " + name());
			if (role() == "black") who = board::black;
			if (role() == "white") who = board::white;
			if (who == board::empty)
				throw std::invalid_argument("invalid role: " + role());
			for (size_t i = 0; i < space.size(); i++)
				space[i] = action::place(i, who);
			mcts = new MCTS(who);
		}

		virtual action take_action(const board& state) {
			//cout << "take action\n";
			mcts->who = who;
			mcts->setroot(state);
			int sim_time=1000;
			if (meta.find("simulation") != meta.end()){
				sim_time = int(meta["simulation"]);
			}
			mcts->mcts_simulate(sim_time);
			action::place move = mcts->bestaction();
			mcts->del_tree(mcts->root);
			return move;
		}

	private:
		std::vector<action::place> space;
		board::piece_type who;
		std::default_random_engine engine;
		MCTS* mcts;
	
};

