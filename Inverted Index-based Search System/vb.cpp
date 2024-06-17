#include "header.h"

using namespace std;

typedef unsigned char cbyte;

class VBEncoding {
public:
	void encode(int n, vector<cbyte>& byte_rep){
		vector<int> rep;
		while(n>0) {
			if(n&1) rep.push_back(1);
			else rep.push_back(0);
			n>>=1;
		}
		if(rep.size()==0) rep.push_back(0);
		while(((int)rep.size()%7)!=0){
			rep.push_back(0);
		}

		for(int i = 0;i<rep.size();i+=7){
			cbyte tot = 0;
			int mul = 1;
			for(int j = 0;j<7;++j){
				tot += (mul*(rep[i+j]==1));
				mul *= 2;
			}
			if(i==0) tot+=128;
			byte_rep.emplace_back(tot);
		}
		reverse(byte_rep.begin(), byte_rep.end());
	}

	int decode(vector<cbyte>& byte_rep){
		vector<int> rep;
		reverse(byte_rep.begin(), byte_rep.end());
		byte_rep[0]-=128;
		for(cbyte& v: byte_rep){
			for(int i = 0;i<7;++i){
				unsigned char check = (1<<i);
				rep.push_back(((v&check)!=0));
			}
		}
		int res = 0;
		int mul = 1;
		for(int& v:rep){
			res += mul*v;
			mul*=2;
		}
		return res;
	}
};