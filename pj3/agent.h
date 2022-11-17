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
		Node(const board& b): visittime(0), wintime(0), position(b){}
		
		
	};
	public:
		Node* root;
		vector<action::place> myspace;
		vector<action::place> oppospace;
		board::piece_type who;
		std::default_random_engine engine;
		MCTS(board::piece_type who, const board& b): myspace(board::size_x * board::size_y), oppospace(board::size_x * board::size_y){
			who = who;
			root = new Node(b);
			for (size_t i = 0; i < myspace.size(); i++)
				myspace[i] = action::place(i, who);
			for (size_t i = 0; i < oppospace.size(); i++)
				oppospace[i] = action::place(i, who);
			expand(root, true);
			cout << root->childs[0] << endl;
		}
		Node* select(Node* curnode){
			int bestvalue=-10000;
			Node* bestnode = NULL;
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
			vector <Node*> children;
			std::vector<action::place> tmpspace = (myturn)? myspace : oppospace;
			for(int i=0; i < (int) tmpspace.size();i++){
				action::place nextmove = tmpspace[i];
				board cur = node->position;
				if(nextmove.apply(cur) == board::legal){
					Node* child = new Node(cur);
					children.push_back(child);
				}
			}
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
			for(int i=0;i<simulations;i++){
				cout << "simulation # " << i << endl;
				sim(root);
			}
		}
		int sim(Node* node, bool myturn=true){
			int iswin;
			if(node->childs.empty()){
				cout << "empty\n";
				iswin = simulate(node->position, myturn);
				cout << "done rollout\n";
				expand(node, myturn);
				cout << "done expand\n";
				update(node,iswin);
				cout << "done update\n";
			}
			else{
				Node* next = select(node);
				iswin = sim(next, !myturn);
				update(node, iswin);
			}
			return iswin;
		}
		action::place bestaction(){
			int bestwin = 0;
			if(root->childs.empty()){
				pair<action::place, int> p  = rand_action(root->position, true);
				if(p.second){
					return p.first;
				}
			}
			else{
				Node* bestchild = root->childs[0];
				for(int i=0; i<(int)root->childs.size(); i++){
					Node* child = root->childs[i];
					if(child->wintime > bestwin){
						bestwin = child->wintime;
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
			return action();
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
		}

		virtual action take_action(const board& state) {
			cout << "start take action\n";
			MCTS mcts{who, state};
			int sim_time=1000;
			if (meta.find("simulation") != meta.end()){
				sim_time = int(meta["simulation"]);
			}
			mcts.mcts_simulate(sim_time);
			cout << "done a simulation\n";
			action::place move = mcts.bestaction();
			cout << "done a action\n";
			return move;
		}

	private:
		std::vector<action::place> space;
		board::piece_type who;
		std::default_random_engine engine;
	
};

