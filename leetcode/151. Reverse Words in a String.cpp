class Solution {
public:
    void reverseWords(string &s) {
        if(s == "") return;
        string result = "";
        int idx = 0;
        while(idx < s.size() && s[idx] == ' ') idx++;
        if(idx == s.size()){
            s = "";
            return;
        }
        s = idx == 0? s: s.substr(idx, s.size()-idx);
        for(int i = 0; i < s.size(); i++){
            while(i < s.size() && s[i] == ' ') i++;
            if(i == s.size()) break;
            idx = i;
            cout<<idx<<endl;
            while(idx < s.size() && s[idx] != ' ') idx++;
            result = " " + s.substr(i, idx-i) + result;
            i = idx;
        }
        s = result.substr(1, result.size()-1);
    }
};