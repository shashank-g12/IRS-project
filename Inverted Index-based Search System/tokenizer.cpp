#include "header.h"

using namespace std;

struct pair_hash_t {
    template <typename T1, typename T2>
    size_t operator () (const std::pair<T1, T2>& pair) const {
      const size_t h1 = hash<T1>()(pair.first);
      const size_t h2 = hash<T2>()(pair.second);
      return h1 + h2;
    }
};

typedef pair<string, string> bigram_token;
using bigram_collection_token = unordered_set<bigram_token, pair_hash_t>;

class Tokenizer {
public:
	int type;
	set<char> delimiter{' ', ',', '.', ':', ';', '"', '\'', '\n'};
	bigram_collection_token collection;
	unordered_map<const bigram_token *, int> score;

	Tokenizer(int t) {
		this->type = t;
		if(type) read_merge_order();
	}

	static const bigram_token* get_bigram(bigram_collection_token& collection, 
							const string& a, const string& b){
		return &(*collection.emplace(a,b).first);
	}

	void read_merge_order() {
		ifstream fd;
		fd.open("merge_order", ios::binary);
		if (!fd.is_open()) {
        	std::cerr << "Failed to open merge_order file." << std::endl;
        	return;
    	}
    	while(!fd.eof()){
    		char str[1000];size_t sz; string f,s;
    		fd.read(reinterpret_cast<char *>(&sz), sizeof(sz));
    		fd.read(str, sz); str[sz] = '\0';
    		f = string(str);
    		fd.read(reinterpret_cast<char *>(&sz), sizeof(sz));
    		fd.read(str, sz); str[sz] = '\0';
    		s = string(str);
    		const bigram_token* p = get_bigram(collection, f, s);
    		score[p] = score.size();
    	}
	}

	void tokenize(string& text, vector<string>& terms) {
		if(type) bpe_tokenizer(text, terms);
		else simple_tokenizer(text, terms);
	}

	void simple_tokenizer(string& text, vector<string>& terms) {
		string word;
		for(char& t:text){
			char c = (char)tolower(t);
			if(!ispunct(c) && !isdigit(c) && delimiter.find(c)==delimiter.end())
				word += c;
			else {
				if(!word.empty()) terms.emplace_back(move(word));
				word = "";
			}
		}
		if(!word.empty()) terms.emplace_back(move(word));
		return;
	}

	pair<string,string> find_best(vector<string>& chars){
		pair<string,string> best;
		int mi = (int)1e9;
		for(size_t i = 1;i<chars.size();++i) {
			if(collection.find({chars[i-1],chars[i]})!=collection.end()){
				auto it = collection.find({chars[i-1],chars[i]});
				const bigram_token* p = &(*it);
				if(score[p]<mi){
					best = *it;
					mi = score[p];
				}
			}
		}
		return best;
	}

	void bpe_tokenizer(string& text, vector<string>& terms) {
		simple_tokenizer(text, terms);
		vector<string> copy_terms = terms;
		terms.clear();
		for(auto&word: copy_terms){
			vector<string> chars;
			for(auto& c:word){
				chars.emplace_back(string(1, c));
			}
			chars.emplace_back(string("</w>"));
			while(true){
				pair<string,string> best;
				best = find_best(chars);
				if(best.first.empty() && best.second.empty()) break;
				for(size_t i = 1;i<chars.size();++i){
					if(chars[i-1]==best.first && chars[i]==best.second){
						chars[i-1]+=chars[i];
						chars.erase(chars.begin()+i);
					}
				}
			}
			for(auto& t: chars){
				terms.emplace_back(move(t));
			}
		}
		return;
	}
};