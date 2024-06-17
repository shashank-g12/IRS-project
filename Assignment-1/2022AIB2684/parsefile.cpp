#include "header.h"
using namespace std;

class Parser {
public:
	void getFile( string& filename, string& text );
	void getData( const string &text, vector<string>& docName, vector<string>& collection ); 
	void stripTags( string& text );
	string stripSpaces ( string& text);
	void parse ( string filename, vector<string>& docName, vector<string>& collection );
};

void Parser::getFile (string& filename, string& text) {
	string line;
	ifstream in (filename);	
	if (!in) {cout << filename << " not found"; exit(1);}
	while (in.good() && getline(in, line)) text += (line+" ");
	in.close();
	return;
}

void Parser::getData (const string &text, vector<string>& docName, vector<string>& collection) {
	size_t pos = 0, start;
	while(true){
		start = text.find("<DOCID>",pos);
		if(start==string::npos) break;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        
		start += string("<DOCID>").size();

		pos = text.find("</DOCID>", start);
		string doc_cand = text.substr(start, pos-start);
		docName.emplace_back(stripSpaces(doc_cand));

		start = text.find("<TITLE>", pos);
		start += string("<TITLE>").size();

		pos = text.find("</TITLE>", start);
		string content = text.substr(start, pos-start);

		start = text.find("<CONTENT>", pos);
		start += string("<CONTENT>").size();

		pos = text.find("</CONTENT>", start);
		content += " " + text.substr(start, pos-start);
		collection.emplace_back(move(content));
	}
}

void Parser::stripTags (string &text){
	size_t start = 0, pos;
	while(start<text.size()){
		start = text.find("<", start);
		if(start == string::npos) break;
		pos = text.find(">", start);
		text.erase(start, pos-start+1);
	}
}

string Parser::stripSpaces(string& text) {
	string name;
	for(auto& c: text){
		if(c!=' ') name+= c;
	}
	return name;
}

void Parser::parse (string filename, vector<string>& docName, vector<string>& collection){
	auto func = []( char lhs, char rhs ){
		return (lhs == rhs) && (lhs == ' ');
	};

	string text;
	getFile(filename, text);
	getData(text, docName, collection);
	for(string &s: collection){
		stripTags(s);
		auto new_end = unique(s.begin(), s.end(), func);
		s.erase(new_end, s.end());
	}
}