#include "postinglist.cpp"

using namespace std;

class Query {
public:
	string queryfile, resultfile, indexfile, dictfile;
	int tokenizerType;
	int compressionType;
	unordered_map<string, pair<int, long long>> dictionary;
	unordered_map<string, pair<int, int>> docID;
	PostingsList pl;
	ifstream fd;
	ofstream out;

	Query(string queryfile , string resultfile, string indexfile, string dictfile){
		this->queryfile = queryfile;
		this->resultfile = resultfile;
		this->indexfile = indexfile;
		this->dictfile = dictfile;
		fd.open(indexfile, ios::binary);
		out.open(resultfile);
		pl.read_dictionary(dictfile, dictionary, tokenizerType, compressionType);
		pl.read_docID(fd, docID);
	}

	void parse(vector<pair<int, vector<string>>>& queries) {
		string text, line;
		ifstream in (queryfile);	
		if (!in) {cout << queryfile << " not found"; exit(1);}
		while (in.good() && getline(in, line)) text += (line+" ");
		in.close();
		
		size_t start = 0, pos;
		while(true) {
			pos = text.find("<num>", start);
			if(pos==string::npos) break;
			start = text.find("<title>", pos);
			int id = get_int(text.substr(pos, start-pos));

			start += string("<title>").size();
			pos = text.find("<desc>", start);
			string content = text.substr(start, pos-start);

			pos+= string("<desc>").size();
			start = text.find("<narr>", pos);
			content += " " + text.substr(pos, start-pos);
			size_t fi = content.find("Description:", 0);
			if(fi!=string::npos) content.erase(fi, string("Description:").size());

			Tokenizer tokenizer(tokenizerType);
			vector<string> terms;
			tokenizer.tokenize(content, terms);
			queries.emplace_back(id, terms);
		}
	}

	void score(pair<int, vector<string>>& query) {
		int num_documents = docID.size();
		vector<double> s(num_documents, 0.0);
		for(auto& term: query.second) {
			if(dictionary.find(term)==dictionary.end()) continue;

			vector<pair<int,int>> posting;
			pl.read_posting_list(this->fd, dictionary[term].second, posting, this->compressionType);
			int df = dictionary[term].first;
			double idf = log2(1.0 + (double)num_documents/df);
			
			for(auto &e : posting) {
				double f = e.second;
				double tf = 1.0 + log2(f);
				assert(e.first<num_documents);
				s[e.first] += (tf*idf*idf);
			}
		}
		vector<pair<double, string>> sort_s;
		for(auto& val: s) {
			sort_s.emplace_back(val, string(""));
		}
		for(auto&v : docID) {
			sort_s[v.second.first].second = v.first;
			if(v.second.second>1) sort_s[v.second.first].first /= log2(v.second.second);
		}
		auto func = []( const pair<double,string>& a, const pair<double,string>& b )
		{ return (a.first>b.first); };

		sort(sort_s.begin(), sort_s.end(), func);
		for(int i = 0;i<100;++i){
			out << query.first << " 0 " << sort_s[i].second << " " << sort_s[i].first <<'\n';
		}
		return;
	}

	int get_int(string text) {
		string res;
		for(char c:text)
			if(isdigit(c)) res+=c;
		return stoi(res);
	}
};

int main(int argc, char* argv[]) {
	string queryfile, resultfile, indexfile, dictfile;
	queryfile = argv[1];
	resultfile = argv[2];
	indexfile = argv[3];
	dictfile = argv[4];

	Query q(queryfile, resultfile, indexfile, dictfile);

	vector<pair<int, vector<string>>> queries;
	q.parse(queries);
	cout << "Total queries : " <<  queries.size() <<'\n';
	for(auto& v : queries){
		cout << "Processing " << v.first << " query...";
		q.score(v);
		cout << " done\n";
	}
	q.fd.close();
	q.out.close();
}