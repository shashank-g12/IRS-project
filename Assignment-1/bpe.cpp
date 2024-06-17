#include "header.h"
#include "parsefile.cpp"
#include "tokenizer.cpp"
#include <dirent.h>
using namespace std;

typedef pair<string, string> bigram;
typedef vector<string> sequence;
struct pair_hash {
    template <typename T1, typename T2>
    size_t operator () (const std::pair<T1, T2>& pair) const {
      const size_t h1 = hash<T1>()(pair.first);
      const size_t h2 = hash<T2>()(pair.second);
      return h1 ^ h2;
    }
};

struct mod {
	int j, freq;
	sequence& word;
	sequence old_word;
	mod (int j_, sequence& word_, sequence&& old_word_, int freq_) 
	: j(j_),word(word_), old_word(old_word_), freq(freq_) {}
};

using bigram_collection = unordered_set<bigram, pair_hash>;

class BPELearner{
public:
	string path;
	int iter;
	unordered_map<string, int> list;
	vector<pair<string, string>> merge_order;
	ofstream out;

	BPELearner(string path, int k){
		this->path = move(path);
		this->iter = k;
	}

	void get_filenames(string fpath, vector<string>& filenames){
		const char *dir = fpath.c_str();
		DIR *dirp = opendir(dir);
		if(!dirp) return;

		for(struct dirent *dent; (dent=readdir(dirp))!=NULL;) {
			const char * nm = dent->d_name;
			if(strcmp(nm,".")==0 || strcmp(nm, "..")==0) continue;
			filenames.push_back(string(fpath)+"/"+string(nm));
		}
		closedir(dirp);
		return;
	}

	void getWords(){
		vector<string> filenames;
		for(int i = 1;i<=100;++i){
			get_filenames(this->path+"/f"+to_string(i), filenames);
		}

		random_device rd;
		mt19937 gen(rd());
		discrete_distribution<> d({75,25});

		vector<string> dataset;
		for(auto &s: filenames)
			if(d(gen)) dataset.emplace_back(s);

		Tokenizer tokenizer(0);

		Parser par;
		cout << "Processing " << dataset.size() << " files to learn bpe\n"; 
		for(auto &s: dataset){
			vector<string> docName, collection;
			par.parse(s, docName, collection);
			for(string& text: collection){
				if(text.empty()) continue;
				vector<string> terms;
				tokenizer.tokenize(text, terms);
				for(string &v: terms){
					list[v]+=1;
				}
			}
		}
		cout << "Unique words in dataset : " << list.size() << '\n';
	}

	static const bigram* get_bigram(bigram_collection& collection, 
							const string& a, const string& b){
		return &(*collection.emplace(a,b).first);
	}

	static void get_pair_statistics(bigram_collection& collection,
                    	const vector<pair<int, sequence>>& sorted_list,
                      	unordered_map<const bigram*, int>& stats,
						unordered_map<const bigram*, unordered_map<int, int>>& indices){
		for(size_t i = 0;i<sorted_list.size();++i){
			const int freq = sorted_list[i].first;
			const sequence& word = sorted_list[i].second;
			for(size_t j = 1;j<word.size();++j){
				const bigram* pair = get_bigram(collection, word[j-1], word[j]);
				stats[pair]+=freq;
				indices[pair][i] += 1;
			}
		}
	}

	static vector<pair<int, sequence>> get_sorted_list(unordered_map<string, int> &list){
		vector<pair<int,sequence>> sorted_list;
		multimap<int, sequence> char_list;
		for(const auto& pair: list){
			const string& token = pair.first;
			const int freq = pair.second;
			sequence chars;
			for(auto& c: token){
				chars.emplace_back(string(1,c));
			}
			chars.emplace_back(string("</w>"));
			char_list.emplace(-freq, move(chars));
		}
		for(auto& pair: char_list){
			const int freq = pair.first;
			sequence& chars = pair.second;
			sorted_list.emplace_back(-freq, move(chars));
		}
		return sorted_list;
	}

	static pair<const bigram*, int> get_most_frequent(unordered_map<const bigram*, int>& stats) {
		auto func = [](const pair<const bigram*, int>& a,
						const pair<const bigram*, int>& b){
							return (a.second<b.second || (a.second==b.second && *a.first<*b.first));
						};
		return *max_element(stats.begin(), stats.end(), func);
	}

	static vector<mod> merge_pair(const bigram* mst,
				vector<pair<int,sequence>>& sorted_list,
				unordered_map<const bigram*, unordered_map<int,int>>& indices){
		auto& mst_indices=indices[mst];
		vector<mod> modifs;
		modifs.reserve(mst_indices.size());
		for(auto& p:mst_indices){
			if(p.second<1)
				continue;
			sequence& word = sorted_list[p.first].second;
			sequence wordcopy = word;
			for(size_t i = 0;i<word.size();++i){
				if(word[i]==mst->first && word[i+1]==mst->second){
					word[i].append(word[i+1]);
					word.erase(word.begin()+i+1);
				}
			}
			modifs.emplace_back(p.first, word, move(wordcopy), sorted_list[p.first].first);
		}
		return modifs;
	}

	static void update( bigram_collection& collection,const bigram* p, vector<mod>& modifs, 
						unordered_map<const bigram*, int>& stats,
						unordered_map<const bigram*, unordered_map<int,int>>& indices ) {
		stats[p]=0;
		indices[p]=unordered_map<int,int>();
		for(auto& mod: modifs) {
			const int j = mod.j;
			const sequence& word = mod.word;
			const sequence& old_word = mod.old_word;
			const int freq = mod.freq;

			bool skip = true;
			for(auto it1 = word.begin(),it2 = old_word.begin(); 
				it2!=prev(old_word.end(),1) && it2!=old_word.end();){
				auto next2 = next(it2,1);
				if(*it1== (*it2+*next2)){
					if(!skip) {
						const bigram* correct = get_bigram(collection, *prev(it2,1), *it2);
						stats[correct] -= freq;
						indices[correct][j] -= 1;
					}
					if(next2!=prev(old_word.end(),1)) {
						const bigram* correct = get_bigram(collection, *next2, *next(next2,1));
						stats[correct] -= freq;
						indices[correct][j] -= 1;
					}
					it1++;it2++;it2++;
					skip = true;
				}
				else {
					it1++;it2++;
					skip = false;
				}
			}

			skip = true;
			for(auto it1 = word.begin(),it2 = old_word.begin(); 
				it2!=prev(old_word.end(),1) && it2!=old_word.end();){
				auto next2 = next(it2,1);
				if(*it1== (*it2+*next2)){
					if(!skip) {
						const bigram* correct = get_bigram(collection, *prev(it1,1), *it1);
						stats[correct] += freq;
						indices[correct][j] += 1;
					}
					if(next2!=prev(old_word.end(),1)) {
						const bigram* correct = get_bigram(collection, *it1, *next(it1,1));
						stats[correct] += freq;
						indices[correct][j] += 1;
					}
					it1++;it2++;it2++;
					skip = true;
				}
				else {
					it1++;it2++;
					skip = false;
				}
			}
		}
		return;
	}

	void write(ofstream& fd, pair<string, string> mf){
		size_t sz;
		sz = mf.first.size();
		fd.write(reinterpret_cast<char *>(&sz), sizeof(sz));
		fd.write(mf.first.c_str(),sz);
		sz = mf.second.size();
		fd.write(reinterpret_cast<char *>(&sz), sizeof(sz));
		fd.write(mf.second.c_str(),sz);
	}

	void learn(){
		auto start = chrono::high_resolution_clock::now();
		
		getWords();
		out.open("merge_order", ios::binary);
		vector<pair<int,sequence>> sorted_list = get_sorted_list(list);
		bigram_collection collection;
		unordered_map<const bigram*, int> stats;
		unordered_map<const bigram*, unordered_map<int,int>> indices;
		get_pair_statistics(collection, sorted_list, stats, indices);

		for(int i = 0;i<iter;++i){	
			pair<const bigram*, int> most_frequent = get_most_frequent(stats);
			if(most_frequent.second<=0) {
				cout << "No more bigrams can be formed from the dataset\n";
				break;
			}
			if((i+1)%1000==0) cout << (i+1) << "/" << iter << " iterations completed\n";
			merge_order.emplace_back(*most_frequent.first);
			write(out, merge_order.back());
			
			vector<mod> modifs =  merge_pair(most_frequent.first, sorted_list, indices);
			update(collection, most_frequent.first, modifs, stats, indices);
		}
		
		auto end = chrono::high_resolution_clock::now();
		double time_taken = chrono::duration_cast<chrono::nanoseconds>(end - start).count();
 		time_taken *= 1e-9;
 		cout << "Time taken for BPE learning : " << fixed << time_taken; 
 		cout << " sec" << endl;
	}
};

int main(int argc, char *argv[]){
	string path = argv[1];
	int k = atoi(argv[2]);
	BPELearner bpe(path, k);
	bpe.learn();
}