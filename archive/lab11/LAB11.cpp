#include <iostream>
#include <vector>
#include <list>
#include <algorithm>
#include <fstream>
#include "stopWatch.hpp"

using namespace std;

void display(vector<string> out) {
    for (auto it : out) {
        cout << it << endl;
    }
}

bool checkprime(unsigned int p) {
    if (p == 1)  
        return false;
    if (p == 2) 
        return true;
    if (p % 2 == 0) 
        return false;
    for (int i = 3; i * i <= p; i += 2)  
        if (p % i == 0)
            return false;
    return true;
}

int getNextPrime(unsigned int n) {
    while (!checkprime(n)) {
        n++;
    }
    return n;  
}

class hashtable {
    int size;
    int myprime;
    vector<list<string>> myhashtable;

public:
    hashtable(int n) : size(n / 4) { 
        myprime = getNextPrime(n / 4);
        myhashtable.resize(size);
    }
   
    int hash(string val) {
        unsigned long long hash = 0;  
        for (auto it : val) {
            hash = it + hash * myprime;
        }
        return hash % size;
    }
    
    void insert(string val) {
    int index = hash(val);
    //cout << "Inserting: " << val << " at index " << index << endl;
    myhashtable[index].push_front(val);
    }

    bool lookup(string val) {
        int index = hash(val);  
        auto& list_at_index = myhashtable[index];
        
        for (const auto& item : list_at_index) {
            if (item == val) {
                return true; 
            }
        }
        return false; 
    }
};

int main(int argc, char* argv[]) {
    cout << "Program started" << endl;
    if (argc != 3) {
        cout << "Usage: lab11.exe <dictionary_file> <grid_file>" << endl;
        return 1; 
    }
    
    string dictionaryFile = argv[1];
    string gridFile = argv[2];
    
    ifstream dictFile(dictionaryFile);
    cout << "counting words" << endl;
    string word;
    int count = 0;
    while (getline(dictFile, word)) {
        count++;
    }
    
    dictFile.close();
    
    hashtable h1(count);
    hashtable h2(count);
    
    ifstream dictFile2(dictionaryFile);
  
    cout<<"loading words"<<endl;

    while (getline(dictFile2, word)) {
    //cout << "Word: " << word << endl;
    if (word.length() < 3) continue;
    h1.insert(word);
    //cout << "Inserted: " << word << endl;
    for (int i = 0; i < word.length() - 2; i++) {
        string substring = word.substr(i, 3);
        //cout << "Substring: " << substring << endl;
        if (h2.lookup(substring)) continue;
        h2.insert(substring);
        //cout << "Inserted substring: " << substring << endl;
    }
}
    cout<<"words loaded"<<endl;
    
    dictFile2.close();
    
    ifstream gridFileStream(gridFile);
   

    int numRows, numCols;
    gridFileStream >> numRows >> numCols;
    cout<<"rows , columns read"<<endl;
    vector<vector<string>> grid(numRows, vector<string>(numCols));
    cout<<"grid created"<<endl;
    string line;
    gridFileStream.ignore();  


    int k=0;
    for (int i = 0; i < numRows; ++i) {
        getline(gridFileStream, line);
        for (int j = 0; j < numCols; ++j) {
            grid[i][j] = line[k]; 
            k++;
        }
    }
    cout<<"grid populated"<<endl;

    gridFileStream.close();
    
    int directions[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}};
    
    hashtable h3(count);
    vector<string> found;
    bool flag=true;
    Stopwatch clock;
    
    for (int i = 0; i < numRows; i++) {
        for (int j = 0; j < numCols; j++) {
            for (auto& dir : directions) {
                string temp = "";
                int c = 0;
                flag=true;
                while (i + c * dir[0] >= 0 && i + c * dir[0] < numRows && 
                       j + c * dir[1] >= 0 && j + c * dir[1] < numCols) {
                    temp += grid[i + c * dir[0]][j + c * dir[1]];
                    c++;

                    if (temp.length() >= 3  ) {

                        for(int k =0;k<=temp.length()-3;k++)
                        {
                            if(!h2.lookup(temp.substr(k, 3))){
                                flag = false;
                                break;
                            }
                           
                        }
                        if(!flag){
                            break;
                        }

                        if (h1.lookup(temp)) {
                            found.push_back("Word found at direction (" + to_string(dir[0]) + ", " + to_string(dir[1]) + 
                                            ") at (" + to_string(i) + ", " + to_string(j) + "): " + temp);
                        }
                    }
                }
            }
        }
    }
    cout<<"words found"<<endl;
    display(found);
    cout<<found.size()<<" words found"<<endl;
    cout<<"time taken "<<clock.elapsedTime();
    return 0;
}
