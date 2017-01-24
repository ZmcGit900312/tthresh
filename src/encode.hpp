#ifndef __ENCODE_HPP__
#define __ENCODE_HPP__

// Parts of this code were taken from https://rosettacode.org/wiki/Huffman_coding
// (released under the GNU Free Documentation License 1.2, http://www.gnu.org/licenses/fdl-1.2.html)
// which is a C++ Huffman encoding implementation.

#include <iostream>
#include <fstream>
#include <queue>
#include <map>
#include <iterator>
#include <algorithm>
#include <cstring>
#include "zlib_io.hpp"

using namespace std;

typedef
std::vector < bool > HuffCode;
typedef
std::map < unsigned long int, HuffCode > HuffCodeMap;

class INode {
public:
    const unsigned long int
    f;

    virtual ~ INode() {
    } protected:
    INode(unsigned long int f):f(f) {
    }
};

class InternalNode:public INode {
public:
    INode * const
    left;
    INode *const
    right;

    InternalNode(INode * c0, INode * c1):INode(c0->f + c1->f), left(c0), right(c1) {
    } ~InternalNode() {
        delete left;
        delete right;
    }
};

class LeafNode:public INode {
public:
    const unsigned long int
    c;

    LeafNode(unsigned long int f, unsigned long int c):INode(f), c(c) {
    }};

struct NodeCmp {
    bool operator  () (const INode * lhs, const INode * rhs) const {
        return lhs->f > rhs->f;
    }};

INode *BuildTree(std::map < unsigned long int, int >&frequencies)
{
    std::priority_queue < INode *, std::vector < INode * >, NodeCmp > trees;

    for (std::map < unsigned long int, int >::iterator it = frequencies.begin(); it != frequencies.end(); ++it) {
        trees.push(new LeafNode(it->second, (int) it->first));
    }
    while (trees.size() > 1) {
        INode *childR = trees.top();
        trees.pop();

        INode *childL = trees.top();
        trees.pop();

        INode *parent = new InternalNode(childR, childL);
        trees.push(parent);
    }
    return trees.top();
}

void GenerateCodes(const INode * node, const HuffCode & prefix, HuffCodeMap & outCodes)
{
    if (const LeafNode * lf = dynamic_cast < const LeafNode * >(node)) {
        outCodes[lf->c] = prefix;
    } else if (const InternalNode * in = dynamic_cast < const InternalNode * >(node)) {
        HuffCode leftPrefix = prefix;
        leftPrefix.push_back(false);
        GenerateCodes(in->left, leftPrefix, outCodes);

        HuffCode rightPrefix = prefix;
        rightPrefix.push_back(true);
        GenerateCodes(in->right, rightPrefix, outCodes);
    }
}

unsigned char wbyte;
char wbit;

void inline write_bit(char bit) {
    wbyte |= bit << wbit;
    wbit--;
    if (wbit < 0) {
        write_zlib_stream(reinterpret_cast < unsigned char *> (&wbyte), sizeof(wbyte));
        wbyte = 0;
        wbit = 7;
    }
}

void encode(vector<unsigned long int>& counters) {

    std::map < unsigned long int, int >frequencies;
    for (unsigned long int i = 0; i < counters.size(); ++i)
        ++frequencies[counters[i]];

    /*******************************/
    // Create the Huffman dictionary
    /*******************************/

    INode *root = BuildTree(frequencies);

    HuffCodeMap codes;
    GenerateCodes(root, HuffCode(), codes);
    delete root;

    if (frequencies.size() == 1) // If there's only one symbol, we still need one bit for it
        codes[frequencies.begin()->first].push_back(false);

    /**************************************************************************/
    // Save the dictionary + encoding information:
    // (1) Number of key/code pairs (int)
    // (2) For each key/code pair:
    //         length(key) (char)
    //         key (sequence of bits)
    //         length(code) (char)
    //         code (sequence of bits)
    // (3) Number N of keys to code (ind_t)
    // (4) N codes (sequence of bits)
    /**************************************************************************/

    wbyte = 0;
    wbit = 7;

    // Number of key/code pairs
    unsigned int dict_size = codes.size();
    for (int rbit = 31; rbit >= 0; --rbit)
        write_bit((dict_size >> rbit)&1);

    // Key/code pairs
    for (HuffCodeMap::const_iterator it = codes.begin(); it != codes.end(); ++it) {

        unsigned long int key = it->first; // TODO change key to ind_t

        // First, the key's length
        unsigned int key_len = floor(log2(key)) + 1; // TODO with bit arithmetic
        for (int rbit = 7; rbit >= 0; --rbit)
            write_bit((key_len >> rbit)&1);

        // Next, the key itself
        for (int rbit = key_len-1; rbit >= 0; --rbit)
            write_bit((key >> rbit)&1);

        // Now, the code's length
        unsigned int code_len = it->second.size();
        for (int rbit = 7; rbit >= 0; --rbit)
            write_bit((code_len >> rbit)&1);

        // Finally, the code itself
        for (int rbit = code_len-1; rbit >= 0; --rbit)
            write_bit(it->second[code_len-1-rbit]);
    }

    // Number N of symbols to code
    unsigned long int n_symbols = counters.size();
    for (int rbit = 63; rbit >= 0; --rbit)
        write_bit((n_symbols >> rbit)&1);

    // Now the N codes
    for (unsigned long int i = 0; i < counters.size(); ++i)
        for (unsigned long int j = 0; j < codes[counters[i]].size(); ++j)
            write_bit(codes[counters[i]][j]);

    // Write any reamining bits
    if (wbit < 7)
        write_zlib_stream(reinterpret_cast < unsigned char *> (&wbyte), sizeof(wbyte));
}

#endif // ENCODE_HPP
