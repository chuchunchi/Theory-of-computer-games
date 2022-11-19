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
/*class player : public random_agent {
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
};*/
class Mcts {
private:
    struct Node {
        int visitCount;
        int wins;
        board position;
        std::vector<Node*> childs;
        Node(board b) : visitCount(0), wins(0), position(b) {}
    };

public:
    Mcts() : blackSpace(board::size_x * board::size_y),
                                   whiteSpace(board::size_x * board::size_y) {
        srand(time(NULL));
        engine.seed(rand() % 100000);
        for (int i = 0; i < (int)blackSpace.size(); i++)
            blackSpace[i] = action::place(i, board::black);
        for (int i = 0; i < (int)whiteSpace.size(); i++)
            whiteSpace[i] = action::place(i, board::white);
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

    void search(int timesOfMcts) {
        for (int i = 0; i < timesOfMcts; i++)
            traverse(root);
    }

    action::place chooseAction() {
        if (root->childs.empty())
            return action::place(0, who);
        int bestCount = 0;
        Node* bestNode = root->childs[0];
        for (int i = 0; i < (int)root->childs.size(); i++) {
            if (bestCount < root->childs[i]->visitCount) {
                bestCount = root->childs[i]->visitCount;
                bestNode = root->childs[i];
            }
        }
        return findActionByNextBoard(bestNode->position);
    }

private:  // After testing, it should be private
    int traverse(Node* node, bool isOpponent=false) {
        if (node->childs.empty()) {  // expand and simulate
            int result = simulate(node->position, isOpponent);
            expand(node, isOpponent);
            update(node, result);
            return result;
        } else {
            Node* nextNode = select(node);
//            std::cout << nextNode->position << std::endl;
            int result = traverse(nextNode, !isOpponent);
            update(node, result);
            return result;
        }
    }

    Node* select(Node* node) {
        float bestScore = 0;
        std::vector<Node*> nextNodes;
        for (Node* child : node->childs) {
            float score = uct(*child, node->visitCount);
            if (bestScore < score) {
                bestScore = score;
                nextNodes.clear();
                nextNodes.push_back(child);
            } else if (bestScore == score) {
                nextNodes.push_back(child);
            }
        }
        std::shuffle(nextNodes.begin(), nextNodes.end(), engine);
        if (nextNodes.empty()) {
            std::cerr << "select error" << std::endl;
            exit(0);
        }
        return nextNodes[0];
    }

    int simulate(const board& position, bool isOpponent) {
        std::string test;
        board curPosition = position;
        action::place randomMove = getRandomAction(curPosition, isOpponent);
        while (randomMove.apply(curPosition) == board::legal) {
//            std::cout << curPosition << std::endl;
//            std::cin >> test;
            isOpponent = !isOpponent;
            randomMove = getRandomAction(curPosition, isOpponent);
        }
        return isOpponent;
    }

    void expand(Node* node, bool isOpponent) {
        std::vector<Node*> childs;
        std::vector<action::place>& nextSpace = (isBlackTurn(isOpponent)) ? blackSpace : whiteSpace;
        for (action::place& move : nextSpace) {
            board curPosition = node->position;
            if (move.apply(curPosition) == board::legal)
                childs.push_back(new Node(curPosition));
        }
        node->childs = childs;
    }

    void update(Node* node, int result) {
        node->visitCount++;
        node->wins += result;
    }

    action::place getRandomAction(const board& position, bool isOpponent) {
        std::vector<action::place> temSpace = (isBlackTurn(isOpponent))? blackSpace : whiteSpace;
        std::shuffle(temSpace.begin(), temSpace.end(), engine);
        for (action::place& move : temSpace) {
            board nextBoard = position;
            if (move.apply(nextBoard) == board::legal) {
                return move;
            }
        }
        return temSpace[0];
    }

    bool isBlackTurn(bool isOpponent) {
        return (!isOpponent && who == board::black) || (isOpponent && who == board::white);
    }

    float uct(Node& node, int parentVisitCount) {
        float c = 1.5;
        float exploitation = (float)node.wins / (float)(node.visitCount + 1);
        float exploration = sqrt(log(parentVisitCount) / (float)(node.visitCount + 1));
        return exploitation + c * exploration;
    }

    action::place findActionByNextBoard(const board& nextBoard) {
        std::vector<action::place>& temSpace = (who == board::black)? blackSpace : whiteSpace;
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
    std::vector<action::place> blackSpace;
    std::vector<action::place> whiteSpace;
    board::piece_type who;
    std::default_random_engine engine;
};

/*class MCTS{
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
			//expand(root, true);
		}
		Node* select(Node* curnode){
			while(!curnode->childs.empty()){
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
				curnode = curnode->childs[bestchild];
				cout << "select till leaf\n";
			}
			//TODO: handle leaf node??
			//if(bestnode==NULL) cout << "error: no child to select\n";
			//return bestnode;
			cout << "break\n";
			return curnode;
		}
		void expand(Node* node, bool myturn){
			//cout << "in expand\n";
			//vector <Node*> children;
			
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
					node->childs.push_back(child);
				}
			}
			//shuffle(children.begin(), children.end(), engine);
			//cout << children.size() << endl;
			//node->childs = children;
			//children.clear();
			//children.shrink_to_fit();

		}
		action::place rand_action(board& state, bool myturn){

			std::vector<action::place> tmpspace = (myturn)? myspace : oppospace;
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
			action::place move;
			
			int iswin = 0;
			board tmp = state;
			action::place p = rand_action(tmp,myturn);

			while(p.apply(tmp)==board::legal){
				myturn = !myturn;
				p = rand_action(tmp, myturn);
				iswin ++;
				iswin = iswin % 2;
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
			clock_t start;
			start = clock();
			while((float) (clock()-start)/CLOCKS_PER_SEC<0.8){
				//cout << "simulation # " << endl;
				sim(root);
			}
			
			
			//cout << "elapsed time: " << elapsed_seconds.count() << endl;
		}
		int sim(Node* node, bool myturn=true){
			int iswin;
			if(node->childs.empty()){
				//cout << "empty\n";
				expand(node, myturn);
				iswin = simulate(node->position, myturn);
				
				update(node,iswin);
			}
			else{
				//cout << "select root\n";
				Node* next = select(node);
				iswin = sim(next, !myturn);
				update(node, iswin);
			}
			return iswin;
		}
		action::place bestaction(){
			int best = 0;
			if(root->childs.empty()){
				cout << 250 << endl;
				return action::place(0,who);
				/*pair<action::place, int> p  = rand_action(root->position, true);
				if(p.second){
					return p.first;
				}*//*
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
						tmpspace.clear();
						tmpspace.shrink_to_fit();
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
			for (int i = 0; i < (int)node->childs.size(); i++){
				del_tree(node->childs[i]);
				
			}
			node->childs.clear();
			node->childs.shrink_to_fit();	
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
	
};*/

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
	    mcts.search(int(meta["simulation"]));
	    action::place move = mcts.chooseAction();
	    mcts.resetMcts();
	    return move;
	}

private:
	std::vector<action::place> space;
	board::piece_type who;
	Mcts mcts;
};

