#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <map>

using namespace std;

struct Node
{
    uint32_t offset;
    uint32_t length;
    string name;
    vector<Node *> children;
    Node(uint32_t offset, uint32_t length, string name);
    ~Node();
};

class Wad
{
    char magic[5];
    uint32_t numDescriptors;
    uint32_t descriptorOffset;
    string fileName;
    map<string, Node *> nodesMap; // to keep track of file paths and their corresponding pointers

    Wad(const string &path);
    vector<string> tokenizePath(const string &path); // helper function
    void shiftDataForward(fstream &wadFile, streampos startPos, size_t shiftAmount);   // helper function
    int findDescriptorIndex(const string &file); //helper function
public:
    ~Wad();
    static Wad *loadWad(const string &path);
    string getMagic();
    bool isContent(const string &path);
    bool isDirectory(const string &path);
    int getSize(const string &path);
    int getContents(const string &path, char *buffer, int length, int offset = 0);
    int getDirectory(const string &path, vector<string> *directory);
    void createDirectory(const string &path);
    void createFile(const string &path);
    int writeToFile(const string &path, const char *buffer, int length, int offset = 0);
    void printWadStructure() const;
};
