#ifndef TRIE_HPP
#define TRIE_HPP

#include <cedar.h>
#include <string>
#include <vector>
#include <queue>

namespace cos_utils {
using namespace std;
#define MAX_TRIEMAP_KEY_LENGTH 1024

class triemap {
public:
    triemap() {
        ind = 1;
    }

    int insert(const string& key) {
        if (reuseable_ind.empty()) {
            if (trie.exactMatchSearch<int>(key.c_str(), key.length()) < 0) {
                int index = ind++;
                trie.update(key.c_str(), key.length(), index);
                return index;
            } else {
                return -1;
            }
        } else {
            int index = reuseable_ind.front();
            reuseable_ind.pop();
            trie.update(key.c_str(), key.length(), index);
            return index;
        }
    }

    int erase(const string& key) {
        if (trie.exactMatchSearch<int>(key.c_str(), key.length()) < 0) return -1;
        int index = trie.exactMatchSearch<int>(key.c_str(), key.length());
        trie.erase(key.c_str(), key.length());
        reuseable_ind.push(index);
        return index;
    }

    vector<int> prefixEraseAll(const string& prefix, queue<string>& suffixs) {
        const static size_t buffer_size = 16;
        vector<int> erased_ind;
        vector<cedar::da<int>::result_triple_type> buffer(buffer_size);
        const size_t match_num = trie.commonPrefixPredict(prefix.c_str(), buffer.data(), buffer_size);
        if (match_num > buffer_size) {
            buffer.resize(match_num);
            trie.commonPrefixPredict(prefix.c_str(), buffer.data(), match_num);
        }
        char suffix[MAX_TRIEMAP_KEY_LENGTH];
        for (size_t i = 0; i < match_num; i++) {
            trie.suffix(suffix, buffer[i].length, buffer[i].id);
            const string key = prefix + suffix;
            suffixs.push(suffix);
            erased_ind.push_back(erase(key));
        }
        return erased_ind;
    }

    int find(const string& key) {
        return trie.exactMatchSearch<int>(key.c_str(), key.length());
    }

    bool contains(const string& key) {
        return trie.exactMatchSearch<int>(key.c_str(), key.length()) < 0? false: true;
    }

    void listKey(queue<string> &keys, const string& prefix) {
        const static size_t buffer_size = 32;
        vector<cedar::da<int>::result_triple_type> buffer(buffer_size);
        const size_t match_num = trie.commonPrefixPredict(prefix.c_str(), buffer.data(), buffer_size);
        if (match_num > buffer_size) {
            buffer.resize(match_num);
            trie.commonPrefixPredict(prefix.c_str(), buffer.data(), match_num);
        }
        char suffix[MAX_TRIEMAP_KEY_LENGTH];
        for (size_t i = 0; i < match_num; i++) {
            trie.suffix(suffix, buffer[i].length, buffer[i].id);
            const string key = prefix + suffix;
            keys.push(key);
        }
    }
    void listValue(queue<int>& values, const string& prefix) {
        const static size_t buffer_size = 32;
        vector<cedar::da<int>::result_triple_type> buffer(buffer_size);
        const size_t match_num = trie.commonPrefixPredict(prefix.c_str(), buffer.data(), buffer_size);
        if (match_num > buffer_size) {
            buffer.resize(match_num);
            trie.commonPrefixPredict(prefix.c_str(), buffer.data(), match_num);
        }
        for (size_t i = 0; i < match_num; i++) {
            int value = buffer[i].value; 
            values.push(value);
        }
    }

private:
    cedar::da<int>                          trie;
    queue<int>                              reuseable_ind;
    int                                     ind;
};


}

#endif