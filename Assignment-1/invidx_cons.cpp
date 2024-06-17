#include "postinglist.cpp"
#include <sys/stat.h>

using namespace std;

int main(int argc, char* argv[]){
	string collPath = argv[1];
	string indexFile = argv[2];
	int compressionType = atoi(argv[3]);
	int tokenizerType = atoi(argv[4]);

	PostingsList pl(collPath, indexFile, tokenizerType, compressionType);

	pl.get_pairs();
	pl.merge_all();
}