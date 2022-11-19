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
    struct Node {
        int visittime;
        int wintime;
        board position;
        std::vector<Node*> childs;
        Node(board b) : visittime(0), wintime(0), position(b) {}
    };

public:
    Mcts() : blackspace(board::size_x * board::size_y),
                                   whitespace(board::size_x * board::size_y) {
        srand(time(NULL));
        engine.seed(rand() % 100000);
        for (int i = 0; i < (int)blackspace.size(); i++)
            blackspace[i] = action::place(i, board::black);
        for (int i = 0; i < (int)whitespace.size(); i++)
            whitespace[i] = action::place(i, board::white);
    }
    Mcts(board::piece_type type) : Mcts() {
        setWho(type);
    }

    void setWho(board::piece_type type) {
        who = type;
    }

    void setupRoot(const board& b) {
        root = new Node(b);
    }

    void resetMcts(Node* node=nullptr) {
        if (node == nullptr)
            node = root;
        for (int i = 0; i < (int)node->childs.size(); i++)
            resetMcts(node->childs[i]);
        delete node;
    }

    void mcts_simulate(int simulations){
		clock_t start;
		start = clock();
		while((float) (clock()-start)/CLOCKS_PER_SEC<0.8){
			//cout << "simulation # " << endl;
			sim(root);
		}
			
		//cout << "elapsed time: " << elapsed_seconds.count() << endl;
	}

    action::place chooseAction() {
        if (root->childs.empty())
            return action::place(0, who);
        int bestCount = 0;
        Node* bestNode = root->childs[0];
        for (int i = 0; i < (int)root->childs.size(); i++) {
            if (bestCount < root->childs[i]->visittime) {
                bestCount = root->childs[i]->visittime;
                bestNode = root->childs[i];
            }
        }
        return findActionByNextBoard(bestNode->position);
    }

private:  // After testing, it should be private
    int sim(Node* node, bool myturn=true){
		int iswin;
		if(node->childs.empty()){
			iswin = simulate(node->position, myturn);
			expand(node, myturn);
			update(node,iswin);
		}
		else{
			Node* next = select(node);
			iswin = sim(next, !myturn);
			update(node, iswin);
		}
		return iswin;
	}

    Node* select(Node* curnode){
		int bestvalue=-10000;
		//Node* bestnode = new Node();
		int bestchild = 0;
		for(int i=0;i<(int)curnode->childs.size();i++){
			//Node* child = curnode->childs[i];
			//double val = uctvalue(*child, curnode->visittime);
			double val = uctvalue(*curnode->childs[i], curnode->visittime);
			if(bestvalue < val){
				bestvalue = val;
				//bestnode = child;
				bestchild = i;
			}
				
		}
		//TODO: handle leaf node??
		//if(bestnode==NULL) cout << "error: no child to select\n";
		//return bestnode;
		return curnode->childs[bestchild];
	}

    int simulate(const board& state, bool myturn){
		action::place move;
		
		int iswin = 0;
		board tmp = state;
		action::place p = rand_action(tmp,myturn);
		while(p.apply(tmp)==board::legal){
			myturn = !myturn;
			p = rand_action(tmp, myturn);
			iswin ++;
			iswin = iswin % 2;
			cout << myturn << " " << iswin << endl;
		}
		return iswin;
		//TODO: should return win or lose
	}

    void expand(Node* node, bool myturn) {
        std::vector<Node*> childs;
        std::vector<action::place>& nextSpace = (isblack(myturn)) ? blackspace : whitespace;
        for (action::place& move : nextSpace) {
            board curPosition = node->position;
            if (move.apply(curPosition) == board::legal)
                childs.push_back(new Node(curPosition));
        }
        node->childs = childs;
    }

    void update(Node* node, int result) {
        node->visittime++;
        node->wintime += result;
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

    bool isblack(bool myturn){
		if((myturn && who==board::black)||(!myturn && who==board::white)){
			return 1;
		}
		else return 0;
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

    action::place findActionByNextBoard(const board& nextBoard) {
        std::vector<action::place>& temSpace = (who == board::black)? blackspace : whitespace;
        for (action::place& move : temSpace) {
            board position = root->position;
            if (move.apply(position) == board::legal && position == nextBoard)
                return move;
        }
        std::cerr << "find action error" << std::endl;
        exit(0);  // Error with call
    }

    std::string appendPath(std::string path, const action::place& move) {
        std::string moveCode = std::to_string(move.position().x) + std::to_string(move.position().y);
        return path + "_" + moveCode;
    }

private:
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

	virtual action take_action(const board& state) {
		if (std::string(meta["name"]) == "mcts") {
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
	    mcts.setupRoot(state);
	    mcts.mcts_simulate(int(meta["simulation"]));
	    action::place move = mcts.chooseAction();
	    mcts.resetMcts();
	    return move;
	}

private:
	std::vector<action::place> space;
	board::piece_type who;
	Mcts mcts;
};

