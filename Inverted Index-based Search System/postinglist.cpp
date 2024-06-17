#include "parsefile.cpp"
#include "tokenizer.cpp"
#include "vb.cpp"
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
using namespace std;

typedef pair<string, int> term_doc_pair;

struct cmp {
    bool operator()(const pair<term_doc_pair,int> & a, const pair<term_doc_pair,int> & b){
    	return ((a.first.first>b.first.first) || 
    		(a.first.first==b.first.first && a.first.second>b.first.second));
    }
};

class PostingsList{
	//implemented blocked sort-based indexing;
public:
	string path;
	string indexfile;
	int tokenizerType;
	int compressionType;
	VBEncoding encoder;
	int count = 0;
	int save_steps = 300;
	unordered_map<string, pair<int,int>> docID;
	unordered_map<string, pair<int, long long>> dictionary;
	priority_queue<pair<term_doc_pair,int>,vector<pair<term_doc_pair,int>>,cmp> pq;

	PostingsList() {
	}

	PostingsList(string path, string indexfile, int tokenizerType, int compressionType){
		this->path = move(path);
		this->indexfile = move(indexfile);
		this->tokenizerType = tokenizerType;
		this->compressionType = compressionType;
	}

	static void get_filenames(string fpath, vector<string>& filenames){
		const char *dir = fpath.c_str();
		DIR *dirp = opendir(dir);
		if(!dirp) return; 

		for(struct dirent *dent; (dent=readdir(dirp))!=NULL;) {
			const char * nm = dent->d_name;
			if(strcmp(nm,".")==0 || strcmp(nm, "..")==0) continue;
			filenames.emplace_back(string(fpath)+"/"+string(nm));
		}
		closedir(dirp);
		return;
	}

	static bool compare(const term_doc_pair& a, const term_doc_pair& b){
		return ((a.first<b.first) || (a.first==b.first && a.second<b.second));
	}

	void get_pairs(){
		vector<string> filenames;
		for(int i = 1;i<=100;++i){
			get_filenames(this->path+"/f"+to_string(i), filenames);
		}
		cout << "Total files : " << filenames.size() << '\n';

		Tokenizer tokenizer(tokenizerType);

		vector<term_doc_pair> term_docID;
		vector<string> docName, collection;
		Parser par;
		for(size_t k = 0;k<filenames.size();++k){
			
			par.parse(filenames[k], docName, collection);
			
			for(size_t i = 0;i<docName.size();++i){
				int curr_id = docID.size();
				string& text = collection[i];
				if(text.empty()) {
					cout << "No text in doc : "<< curr_id << '\n';
					continue;
				}
				
				vector<string> terms;
				tokenizer.tokenize(text, terms);
				docID[docName[i]] = {curr_id, terms.size()};
				for(auto& t: terms){
					term_docID.emplace_back(move(t), curr_id);
				}
			}
			docName.clear();
			collection.clear();
			if((int)(k+1)%save_steps==0) {
				save(term_docID);
				term_docID.clear();
			}
		}
		if(!term_docID.empty()) save(term_docID);
	}

	void save(vector<term_doc_pair>& term_docID){
		sort(term_docID.begin(), term_docID.end(), compare);
		string bin_name = to_string(++count);
		writeFile(bin_name, term_docID);
	}

	void merge_all(){
		ifstream fd[count+1];
		ofstream out(this->indexfile+".idx", ios::binary);
		
		string prev;
		map<int, int> element;

		write_docID(out); // write doc-id mapping
		
		for(int i = 1;i<=count;++i){
			fd[i].open(to_string(i), ios::binary);
			term_doc_pair curr;
			read_pair(fd[i], curr);
			pq.push({curr, i});
		}

		while(!pq.empty()){
			pair<term_doc_pair,int> mi = pq.top();pq.pop();
			//process current term_doc_pair
			if(mi.first.first==prev){
				element[mi.first.second]+=1;
			}
			else {
				if(!element.empty() && !prev.empty()) {
					dictionary[prev] = {element.size(), out.tellp()};
					write_posting_list(out, element);
				}
				element.clear();
				prev.clear();
				prev = mi.first.first;
				element[mi.first.second]+=1;
			}
			
			//add a new term_doc_pair to priority queue
			if(!fd[mi.second].eof()) {
				term_doc_pair curr;
				read_pair(fd[mi.second], curr);
				pq.push({curr, mi.second});
			}
			else remove(to_string(mi.second).c_str());
		}
		if(!element.empty() && !prev.empty()) {
			dictionary[prev] = {element.size(), out.tellp()};
			write_posting_list(out, element);
		}
		out.close();
		write_dictionary();
		for(int i = 1;i<=count;++i)
			fd[i].close();
		
		return;
	}

	void delta_encoding(map<int,int>& element, vector<pair<int,int>>& posting) {
		int prev_value;
		posting.reserve(element.size());
		for(auto& v:element){
			if(posting.size()>=1){
				assert(v.first-prev_value>=0);
				posting.emplace_back(v.first-prev_value, v.second);
				prev_value = v.first;
			}
			else {
				prev_value = v.first;
				posting.emplace_back(v.first,v.second);
			}
		}
		return;
	}

	void write_posting_list(ofstream& fd, map<int,int>& element) {
		vector<pair<int,int>> posting;
		delta_encoding(element, posting);
		
		size_t sz = posting.size();
		fd.write(reinterpret_cast<char *>(&sz), sizeof(sz));
		for(auto& v:posting) {
			int docid = v.first;
			assert(docid<(int)docID.size());
			int tf = v.second;
			if(this->compressionType) {
				vector<cbyte> byte_rep;
				encoder.encode(docid, byte_rep);
				for(cbyte& cb: byte_rep){
					fd.write(reinterpret_cast<char *>(&cb), sizeof(cb));
				}
			}
			else fd.write(reinterpret_cast<char *>(&docid), sizeof(docid));
			if(this->compressionType) {
				vector<cbyte> byte_rep;
				encoder.encode(tf, byte_rep);
				for(cbyte& cb: byte_rep){
					fd.write(reinterpret_cast<char *>(&cb), sizeof(cb));
				}
			}
			else fd.write(reinterpret_cast<char *>(&tf), sizeof(tf));
		}
	}

	void read_posting_list(ifstream& fd, long long pos, vector<pair<int,int>>& curr, int cType){
		fd.seekg(pos, fd.beg);
		size_t sz;
		fd.read(reinterpret_cast<char *>(&sz), sizeof(sz));
		curr.reserve(sz);
		for(size_t i = 0;i<sz;++i){
			int docid, tf;
			if(cType) {
				vector<cbyte> byte_rep;
				cbyte curr;
				while(true) {
					fd.read(reinterpret_cast<char *>(&curr), sizeof(curr));
					byte_rep.push_back(curr);
					if(curr>=128) break;
				}
				docid = encoder.decode(byte_rep);
			}
			else fd.read(reinterpret_cast<char *>(&docid), sizeof(docid));
			if(cType) {
				vector<cbyte> byte_rep;
				cbyte curr;
				while(true) {
					fd.read(reinterpret_cast<char *>(&curr), sizeof(curr));
					byte_rep.push_back(curr);
					if(curr>=128) break;
				}
				tf = encoder.decode(byte_rep);
			}
			else fd.read(reinterpret_cast<char *>(&tf), sizeof(tf));
			curr.emplace_back(docid, tf);
		}
		//for delta encoding
		int c = 0;
		for(auto& v:curr){
			c += v.first;
			v.first = c;
		}
	}

	void write_docID(ofstream& fd){
		size_t full_size = docID.size();
		size_t sz;
		fd.write(reinterpret_cast<char *>(&full_size),sizeof(full_size));
		for(auto& v:docID){
			sz = v.first.size();
			fd.write(reinterpret_cast<char *>(&sz), sizeof(sz));
			fd.write(v.first.c_str(), sz);
			fd.write(reinterpret_cast<char *>(&v.second.first), sizeof(int));
			fd.write(reinterpret_cast<char *>(&v.second.second), sizeof(int));
		}
		return;
	}

	void read_docID(ifstream& fd, unordered_map<string, pair<int,int>>& docID_map){
		fd.seekg(0, fd.beg);
		size_t full_size, sz;
		char str[1000];
		fd.read(reinterpret_cast<char *>(&full_size), sizeof(full_size));
		for(size_t i = 0;i<full_size;++i){
			fd.read(reinterpret_cast<char *>(&sz), sizeof(sz));
			fd.read(str, sz);
			str[sz]='\0';
			int id, num;
			fd.read(reinterpret_cast<char *>(&id), sizeof(id));
			fd.read(reinterpret_cast<char *>(&num), sizeof(num));
			docID_map[string(str)] = {id, num};
		}
		return;
	}

	void write_dictionary() {
		ofstream out(this->indexfile+".dict", ios::binary);
		out.write(reinterpret_cast<char *>(&this->tokenizerType),sizeof(this->tokenizerType));
		out.write(reinterpret_cast<char *>(&this->compressionType),sizeof(this->compressionType));
		size_t full_size=dictionary.size(), sz;
		out.write(reinterpret_cast<char *>(&full_size),sizeof(full_size));
		for(auto& v:dictionary){
			sz = v.first.size();
			out.write(reinterpret_cast<char *>(&sz), sizeof(sz));
			out.write(v.first.c_str(), sz);
			int df = v.second.first;
			long long pos = v.second.second;
			out.write(reinterpret_cast<char *>(&df), sizeof(df));
			out.write(reinterpret_cast<char *>(&pos), sizeof(pos));
		}
		out.close();
		return;
	}

	void read_dictionary(string filename, unordered_map<string, pair<int,long long>>& dict, 
						int& tType, int& cType) {
		ifstream in(filename, ios::binary);
		in.read(reinterpret_cast<char *>(&tType), sizeof(tType));
		in.read(reinterpret_cast<char *>(&cType), sizeof(cType));
		size_t full_size, sz;
		char str[1000];
		in.read(reinterpret_cast<char *>(&full_size), sizeof(full_size));
		for(size_t i = 0;i<full_size;++i){
			in.read(reinterpret_cast<char *>(&sz), sizeof(sz));
			in.read(str, sz);
			str[sz]='\0';
			int df;
			long long pos;
			in.read(reinterpret_cast<char *>(&df), sizeof(df));
			in.read(reinterpret_cast<char *>(&pos), sizeof(pos));
			dict[string(str)] = {df, pos};
		}
		in.close();
		return;
	}

	static void read_pair(ifstream& fd, term_doc_pair& item){
		char str[1000];
		int id;
		size_t sz;
		fd.read(reinterpret_cast<char *>(&sz), sizeof(sz));
		fd.read(str, sz);
		str[sz] = '\0';
		fd.read(reinterpret_cast<char *>(&id), sizeof(id));
		item = make_pair(string(str), id);
		return;
	}

	static void write_pair(ofstream& fd, term_doc_pair& item){
		string t = item.first;
		int id = item.second;
		size_t sz = t.size();
		fd.write(reinterpret_cast<char *>(&sz),sizeof(sz));
		fd.write(t.c_str(), sz);
		fd.write(reinterpret_cast<char *>(&id), sizeof(id));
	}


	static void writeFile(string& name, vector<term_doc_pair>& term_docID){
		ofstream out(name, ios::binary);
		if(!out){ cout << "failed to write\n"; return;}
	
		for(auto& pair: term_docID){
			write_pair(out, pair);
		}
		out.close();
	}
};