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

class Mcts {
	private:
		struct Node{
			int visittime;
			int wintime;
			board position;
			int rvisit;
			int rwin;
			vector<Node*> mapActionToChild;
			vector<board::point> legal;
			vector<Node*> childs;
			std::default_random_engine engine;
			board::point fromWhichMove;
			Node(): visittime(0), wintime(0), rvisit(0), rwin(0){}
			Node(board b): visittime(0), wintime(0), position(b), rvisit(0), rwin(0){
				int size_xy = board::size_x * board::size_y;
				mapActionToChild.resize(size_xy, NULL);
				for (int i = 0; i < size_xy; i++) {
					board::point move(i);
					board tem = b;
					if (tem.place(move) == board::legal)
						legal.push_back(move);
				}
				std::shuffle(legal.begin(), legal.end(), engine);
			}
		};

	public:
		Mcts() : blackspace(board::size_x * board::size_y),
									whitespace(board::size_x * board::size_y) {
			for (int i = 0; i < (int)blackspace.size(); i++)
				blackspace[i] = action::place(i, board::black);
			for (int i = 0; i < (int)whitespace.size(); i++)
				whitespace[i] = action::place(i, board::white);
			int size_xy = board::size_x * board::size_y;
			traverseHistory.resize(size_xy, 0);
		}
		void setWho(board::piece_type type) {
			who = type;
		}
		void setRoot(const board& b) {
			root = new Node(b);
		}

		void mctsopen_episode(const std::string& flag = "") {
			sims_count = 0;
		}

		void mcts_simulate(){
			clock_t start;
			start = clock();
			sims_count++;
			float clocktime;
			if(sims_count<=3) clocktime = 4;
			else if(sims_count<=8) clocktime = 7;
			else if(sims_count<=15) clocktime = 10;
			else if(sims_count<=20) clocktime = 11;
			else if(sims_count<=25) clocktime = 9;
			else if(sims_count<=30) clocktime = 5;
			else clocktime = 3;
			while((float) (clock()-start)/CLOCKS_PER_SEC<clocktime){
				// cout << "simulation ############ " << endl;
				sim(root);
				// traverse(root);
				for (int i = 0; i < (int)traverseHistory.size(); i++){
					traverseHistory[i] = 0;
				}
			}
			
			//cout << "elapsed time: " << elapsed_seconds.count() << endl;
		}

		action::place bestaction(){
			int best = 0;
			if(root->childs.empty()){
				//cout << "weird" << endl;
				//return action();
				action::place p  = rand_action(root->position, true);
				board tmp = root->position;
				if(p.apply(tmp)==board::legal){
					return p;
				}
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
				std::vector<action::place>& tmpspace = (who==board::black)? blackspace:whitespace;
				for(int i=0;i<(int)tmpspace.size();i++){
					action::place next = tmpspace[i];
					board& nextboard = bestchild->position;
					board curposition = root->position;
					if(next.apply(curposition) == board::legal && nextboard == curposition){
						return next;
					}
				}
			}
			// cout << "no available action\n";
			return action();
		}
		void del_tree(Node* node=nullptr) {
			if (node == nullptr)
				node = root;
			for (int i = 0; i < (int)node->childs.size(); i++)
				del_tree(node->childs[i]);
			delete node;
		}

	private:
		

		Node* select(Node* curnode, bool myturn){
			float bestvalue=-10000;
			//Node* bestnode = new Node();
			int bestchild = 0;
			for(int i=0;i<(int)curnode->childs.size();i++){
				//Node* child = curnode->childs[i];
				//double val = uctvalue(*child, curnode->visittime);
				double val = uctvalue(*curnode->childs[i], curnode->visittime, myturn);
				if(bestvalue < val){
					bestvalue = val;
					//bestnode = child;
					bestchild = i;
				}
				
			}
			return curnode->childs[bestchild];
		}
		bool isblack(bool myturn){
			if((myturn && who==board::black)||(!myturn && who==board::white)){
				return 1;
			}
			else return 0;
		}
		void expand(Node* node, bool myturn) {
			std::vector<Node*> children;
			std::vector<action::place>& tmpspace = (isblack(myturn)) ? blackspace : whitespace;
			for(int i=0; i < (int) tmpspace.size();i++){
				//cout << "inside 155 for loop\n";
				action::place& nextmove = tmpspace[i];
				board cur = node->position;
				if (nextmove.apply(cur) == board::legal){
					Node* child = new Node(cur);
					if(children.size()==0){
						child->fromWhichMove = nextmove.position();
						// cout << nextmove.position().i << endl;
						node->mapActionToChild[nextmove.position().i] = child;
					}
					children.push_back(child);
				}
			}
			node->childs = children;
			// return children[0];
		}


		action::place rand_action(board& state, bool myturn){

			std::vector<action::place> tmpspace = isblack(myturn)? blackspace : whitespace;
			std::shuffle(tmpspace.begin(), tmpspace.end(), engine);
			for (const action::place& move : tmpspace) {
				board after = state;
				if (move.apply(after) == board::legal){
					return move;
				}
			}
			action::place t = tmpspace[0];
			tmpspace.clear();
			tmpspace.shrink_to_fit();
			return t; //illegal move
		}
		int simulate(const board& state, bool myturn){
			int iswin = 1;
			board tmp = state;
			action::place p = rand_action(tmp,myturn);

			while(p.apply(tmp)==board::legal){
				myturn = !myturn;
				p = rand_action(tmp, myturn);
				iswin ++;
				iswin = iswin % 2;
				//cout << myturn << " " << iswin << endl;
			}
			return !myturn;
			//TODO: should return win or lose
		}
		
		

		void update(Node* node, int iswin){
			node->visittime++;
			node->wintime += iswin;
			for (int i = 0; i < (int)traverseHistory.size(); i++) {
				if (traverseHistory[i] && node->mapActionToChild[i] != NULL) {
					node->mapActionToChild[i]->rvisit++;
					node->mapActionToChild[i]->rwin += iswin;
				}
			}
		}
		double uctvalue(Node& child, int cur_visittime, bool myturn){
			if(child.visittime==0){
				return 100000000;  // devide by zero
			}
			/*float exploitation = (float)child.wintime / (float)(child.visittime);
			if(!myturn){
				exploitation = 1 - exploitation;
			}
			float exploration = sqrt(log(cur_visittime) / (float)(child.visittime));*/

			float c = 1.414;
			float b = 0.025;
			float beta = (float)child.rvisit /
                ((float)child.visittime + (float)child.rvisit+ 4 * (float)child.visittime * (float)child.rvisit * b * b);
			float winRate = (float)child.wintime/ (float)(child.visittime + 1);
			float raveWinRate = (float)child.rwin / (float)(child.rvisit + 1);

			float exploitation;
			if(myturn)
				exploitation = (1 - beta) * winRate + beta * raveWinRate;
			else
				exploitation = (1 - beta) * (1 - winRate) + beta * (1 - raveWinRate);
			float exploration = sqrt(log(cur_visittime) / (float)(child.visittime + 1));
			return exploitation + c * exploration;
		}

		int sim(Node* node, bool myturn=true){
			int iswin;
			if(node->childs.empty()){
				iswin = simulate(node->position, myturn);
				// cout << "start expand" << endl;
				expand(node, myturn);
				update(node,iswin);
			}
			else{
				Node* next = select(node, myturn);
				traverseHistory[next->fromWhichMove.i] = 1;
				iswin = sim(next, !myturn);
				update(node, iswin);
			}
			return iswin;
		}

	private:
		int sims_count = 0;
		std::vector<int> traverseHistory;
		Node* root;
		std::vector<action::place> blackspace;
		std::vector<action::place> whitespace;
		board::piece_type who;
		std::default_random_engine engine;
	};



class player : public random_agent {
public:
	player(const std::string& args = "") : random_agent("name=random role=unknown " + args),
		space(board::size_x * board::size_y), who(board::empty), mcts() {
		if (name().find_first_of("[]():; ") != std::string::npos)
			throw std::invalid_argument("invalid name: " + name());
		if (role() == "black") who = board::black;
		if (role() == "white") who = board::white;
		if (who == board::empty)
			throw std::invalid_argument("invalid role: " + role());
		for (size_t i = 0; i < space.size(); i++)
			space[i] = action::place(i, who);
		mcts.setWho(who);
	}
	virtual void open_episode(const std::string& flag = "") {
        mcts.mctsopen_episode(flag);
    }
	virtual action take_action(const board& state) {
		if (std::string(meta["type"]) == "mcts") {
		    return mctsAction(state);
		} else {
		    return randomAction(state);
		}
	}

	action randomAction(const board& state) {
        std::shuffle(space.begin(), space.end(), engine);
        for (const action::place& move : space) {
            board after = state;
            if (move.apply(after) == board::legal)
                return move;
        }
        return action();
	}

	action mctsAction(const board& state) {
	    mcts.setRoot(state);
	    mcts.mcts_simulate();
	    action::place move = mcts.bestaction();
	    mcts.del_tree();
	    return move;
	}

private:
	std::vector<action::place> space;
	board::piece_type who;
	Mcts mcts;
};

