#include "Wad.h"
#include <iostream>
#include <stack>
#include <fstream>
#include <regex>

using namespace std;

Node::Node(uint32_t offset, uint32_t length, string name)
{
    this->offset = offset;
    this->length = length;
    this->name = name;
}

Node::~Node()
{
    for (Node *child : children)
    {
        delete child; // Delete all child Nodes recursively
    }
    children.clear(); // Clear the vector after deletion
}

Wad::Wad(const string &path)
{
    // open the file
    fileName = path;
    fstream file(fileName, ios::binary | ios::in | ios::out);
    if (!file)
    {
        throw runtime_error("Failed to open: " + path);
    }

    // Read & update variables
    file.read(magic, 4);
    file.read(reinterpret_cast<char *>(&numDescriptors), 4);
    file.read(reinterpret_cast<char *>(&descriptorOffset), 4);

    // Make the root directory
    Node *root = new Node(0, 0, "/");
    root->children.clear();
    nodesMap["/"] = root;

    // add root to stack
    stack<Node *> stack;
    stack.push(root);

    // Ensures that file pointer begins reading the descriptor at correct place
    file.seekg(descriptorOffset, ios::beg);

    // Make regex patterns
    regex startRegex(".*_START$");
    regex endRegex(".*_END$");
    regex mapMarkerRegex("^E\\dM\\d$");

    for (int i = 0; i < numDescriptors; ++i)
    {
        uint32_t offset;
        uint32_t length;
        char name[9] = {0};

        file.read(reinterpret_cast<char *>(&offset), 4);
        file.read(reinterpret_cast<char *>(&length), 4);
        file.read(name, 8);
        string nameStr(name);

        if (regex_match(nameStr, startRegex)) // namespace directory "_START"
        {
            nameStr = nameStr.substr(0, nameStr.size() - 6);       // Remove "_START"
            Node *currentNode = new Node(offset, length, nameStr); // make new descriptor

            currentNode->children.clear();
            stack.top()->children.push_back(currentNode); // Add to parent directory

            // Update path and add to map
            string dirPath = stack.top()->name + currentNode->name + "/";
            nodesMap[dirPath] = currentNode;
            currentNode->name = dirPath;

            stack.push(currentNode);
        }
        else if (regex_match(nameStr, endRegex)) // namespace directory "_END"
        {
            stack.pop(); // pop because it is the end of the current directory
        }
        else if (regex_match(nameStr, mapMarkerRegex)) // map directory
        {
            Node *currentNode = new Node(offset, length, nameStr); // make new descriptor
            currentNode->children.clear();

            // Update path and add to map
            string dirPath = stack.top()->name + currentNode->name + "/";
            currentNode->name = dirPath;
            stack.top()->children.push_back(currentNode);
            nodesMap[dirPath] = currentNode;

            stack.push(currentNode);

            for (int j = 0; j < 10; j++) // files in map marker directory
            {
                i++;
                file.read(reinterpret_cast<char *>(&offset), 4);
                file.read(reinterpret_cast<char *>(&length), 4);
                file.read(name, 8);

                Node *currentFile = new Node(offset, length, name);

                string filePath = stack.top()->name + name;

                currentFile->name = filePath;
                stack.top()->children.push_back(currentFile);
                nodesMap[filePath] = currentFile;
            }
            stack.pop();
        }
        else
        {                                                          // File
            Node *currentNode = new Node(offset, length, nameStr); // make new descriptor
            string filePath = stack.top()->name + currentNode->name;

            currentNode->name = filePath;
            stack.top()->children.push_back(currentNode); // Add file to parent directory
            nodesMap[filePath] = currentNode;             // Update map with full path
        }
    }
    while (!stack.empty())
    {
        stack.pop();
    }
    file.close();
}

Wad::~Wad()
{
    delete nodesMap["/"];
    nodesMap.clear();
}

Wad *Wad::loadWad(const string &path) // TODO: destructor
{
    Wad *wad = new Wad(path);
    return wad;
}

string Wad::getMagic()
{
    return magic;
}

bool Wad::isContent(const string &path)
{
    // invalid path
    if (path[0] != '/' || path.empty() || path.back() == '/')
    {
        return false;
    }

    if (nodesMap.find(path) == nodesMap.end())
    {
        return false;
    }

    return true;
}

bool Wad::isDirectory(const string &path)
{
    if (path[0] != '/' || path.empty()) // invalid
        return false;

    string p = path;
    if (path.back() != '/') // make sure it ends with / or it wont be found in map
    {
        p += "/";
    }

    if (nodesMap.find(p) == nodesMap.end())
    {
        return false;
    }

    return true;
}

int Wad::getSize(const string &path)
{
    if (!isContent(path)) // invalid
        return -1;

    Node *node = nodesMap[path];
    return node->length;
}

int Wad::getContents(const string &path, char *buffer, int length, int offset)
{
    if (!isContent(path))
        return -1;

    fstream file(fileName, ios::in | ios::out | ios::binary);
    if (!file.is_open())
    {
        throw runtime_error("Failed to open: " + path);
    }

    int fileLength = nodesMap[path]->length;
    int fileOffset = nodesMap[path]->offset;

    if (offset >= fileLength)
    { // offset goes beyond end of file
        file.close();
        return 0;
    }

    int readLength = min(length, fileLength - offset);

    file.seekg(fileOffset + offset, ios::beg);
    file.read(buffer, readLength);
    file.close();

    return readLength;
}

int Wad::getDirectory(const string &path, vector<string> *directory)
{
    if (!isDirectory(path))
    {
        return -1;
    }

    string p = path;
    if (path.back() != '/')
    { // make sure it ends with / or it wont be found in map
        p += "/";
    }

    Node *currentDirectory = nodesMap[p];
    int numChildren = 0;
    for (auto child : currentDirectory->children)
    {
        vector<string> v = tokenizePath(child->name);
        string name = v.back();
        directory->push_back(name);
        ++numChildren;
    }

    return numChildren;
}

void Wad::createDirectory(const string &path)
{
    // no inputted path or no root directory
    if (path.empty() || path[0] != '/')
    {
        return;
    }

    vector<string> pathVec = tokenizePath(path);

    // Check that name of directory is valid length
    if (pathVec.back().length() > 2)
    {
        return;
    }

    // Check if directory already exists
    if (isDirectory(path))
    {
        return;
    }

    // Check if parent directory exists & it is a namespace directory
    regex mapMarkerRegex("^E\\dM\\d$");
    string parentPath = "/";
    string parentEnd;

    for (int i = 0; i < pathVec.size() - 1; i++)
    {
        parentPath += pathVec[i] + "/";
        if (i == pathVec.size() - 2)
            parentEnd = pathVec[i];
    }

    if (!isDirectory(parentPath) || regex_match(parentEnd, mapMarkerRegex))
    {
        return;
    }

    // All necessary checks complete, create the new directory
    string newDirName = pathVec.back();
    string startMarkerName = newDirName + "_START";
    string endMarkerName = newDirName + "_END";

    // Get the names of the end marker of the parent directory
    parentEnd += "_END";

    string p = path;
    if (p.back() != '/')
        p += "/";

    // Open the WAD file
    fstream wadFile;
    wadFile.open(fileName, ios::in | ios::out | ios::binary);
    if (!wadFile.is_open())
    {
        cerr << "Failed to open file: " << fileName << endl;
        return;
    }

    // Update number of descriptors
    numDescriptors += 2;
    wadFile.seekg(4, ios::beg);
    wadFile.write(reinterpret_cast<char *>(&numDescriptors), sizeof(numDescriptors));

    // Handle the root directory case
    if (parentPath == "/")
    {
        // Create a new directory node
        Node *newDir = new Node(0, 0, p);
        Node *parentNode = nodesMap[parentPath];
        parentNode->children.push_back(newDir);
        nodesMap[p] = newDir;

        // Write in the new directory information in the end of the descriptor list
        wadFile.seekp(0, ios::end);
        uint32_t newDescStart = wadFile.tellp();

        wadFile.write(reinterpret_cast<char *>(&newDir->offset), 4);
        wadFile.write(reinterpret_cast<char *>(&newDir->length), 4);
        wadFile.write(startMarkerName.c_str(), 8);
        wadFile.write(reinterpret_cast<char *>(&newDir->offset), 4);
        wadFile.write(reinterpret_cast<char *>(&newDir->length), 4);
        wadFile.write(endMarkerName.c_str(), 8);

        wadFile.close();
        return;
    }

    // Start reading from the file descriptor list
    wadFile.seekg(descriptorOffset, ios::beg);
    streampos parentEndPos = -1;
    char buffer[16];

    vector<string> currDirVec;
    regex startRegex(".*_START$");
    regex endRegex(".*_END$");

    while (wadFile.read(buffer, sizeof(buffer)) || wadFile.gcount() > 0)
    {
        size_t bytesRead = wadFile.gcount();

        // Extract the name and trim it
        string nameInBuffer = string(buffer + 8, 8);

        // Trim spaces and null characters
        nameInBuffer.erase(0, nameInBuffer.find_first_not_of(' '));                                     // Trim leading spaces
        nameInBuffer.erase(nameInBuffer.find_last_not_of(' ') + 1);                                     // Trim trailing spaces
        nameInBuffer.erase(remove(nameInBuffer.begin(), nameInBuffer.end(), '\0'), nameInBuffer.end()); // Remove null characters

        if (regex_match(nameInBuffer, startRegex))
        {
            string name;
            name = nameInBuffer.substr(0, nameInBuffer.size() - 6); // Remove "_START"
            currDirVec.push_back(name);
        }
        else if (regex_match(nameInBuffer, endRegex))
        {
            if (nameInBuffer != parentEnd)
            {
                currDirVec.pop_back();
            }
        }

        // Check if the name matches the parent end marker
        if (bytesRead >= 8 && nameInBuffer == parentEnd) // check for end of parent directory
        {
            string currDir = "/";
            for (auto i : currDirVec)
            {
                currDir += i + "/";
            }

            if (parentPath == currDir) // check ENTIRE path
            {
                parentEndPos = wadFile.tellg();
                parentEndPos -= 16; // Adjust to the start of the current buffer
                break;
            }
            else
            {
                currDirVec.pop_back();
            }
        }
    }

    if (parentEndPos == -1) // Parent end marker not found
    {
        wadFile.close();
        return;
    }

    shiftDataForward(wadFile, parentEndPos, 32);

    // Create a new directory node
    Node *newDir = new Node(0, 0, p);
    Node *parentNode = nodesMap[parentPath];
    parentNode->children.push_back(newDir);
    nodesMap[p] = newDir;

    // Write the new directory information before the end of the parent directory
    wadFile.seekp(parentEndPos, ios::beg); // Ensure we're at the right position before the shift

    // Writing the new directory data
    wadFile.write(reinterpret_cast<char *>(&newDir->offset), 4);
    wadFile.write(reinterpret_cast<char *>(&newDir->length), 4);
    wadFile.write(startMarkerName.c_str(), 8);
    wadFile.write(reinterpret_cast<char *>(&newDir->offset), 4);
    wadFile.write(reinterpret_cast<char *>(&newDir->length), 4);
    wadFile.write(endMarkerName.c_str(), 8);

    // Make sure the file is flushed and closed correctly
    wadFile.flush();
    wadFile.close();
}

void Wad::shiftDataForward(fstream &wadFile, streampos startPos, size_t shiftAmount)
{
    if (startPos < 0)
    {
        cerr << "Invalid startPos!" << endl;
        return;
    }

    // Seek to the end of the file to determine the file size
    wadFile.seekg(0, ios::end);
    streampos fileEnd = wadFile.tellg();

    // Calculate the new file size after shifting
    streampos newFileSize = fileEnd + static_cast<streamoff>(shiftAmount);

    // Extend the file to accommodate the shift if needed
    if (startPos + static_cast<streamoff>(shiftAmount) > fileEnd)
    {
        wadFile.seekp(0, ios::end);                                          // Move to the current end of the file
        vector<char> padding(static_cast<size_t>(newFileSize - fileEnd), 0); // Padding with zeros
        wadFile.write(padding.data(), padding.size());
    }

    // Buffer for shifting
    const size_t bufferSize = 1024; // Choose a reasonable buffer size
    vector<char> tempBuffer(bufferSize);

    // Loop backward through the file, shifting chunks of data
    for (streampos pos = fileEnd; pos > startPos; pos -= static_cast<streamoff>(bufferSize))
    {
        // Calculate how much data to read in this chunk
        streamoff toRead = min(
            static_cast<streamoff>(bufferSize), pos - startPos);

        // Read chunk from current position
        wadFile.seekg(pos - toRead, ios::beg);
        wadFile.read(tempBuffer.data(), static_cast<size_t>(toRead));

        // Write chunk to new position
        wadFile.seekp(pos - toRead + static_cast<streamoff>(shiftAmount), ios::beg);
        wadFile.write(tempBuffer.data(), static_cast<size_t>(toRead));
    }

    // Fill the gap created at the start position with zeros
    wadFile.seekp(startPos, ios::beg);
    vector<char> zeroFill(static_cast<size_t>(shiftAmount), 0);
    wadFile.write(zeroFill.data(), zeroFill.size());

    // Flush changes to ensure they're written to disk
    wadFile.flush();
}

void Wad::createFile(const string &path)
{
    // no inputted path or no root directory
    if (path.empty() || path[0] != '/')
    {
        return;
    }

    vector<string> pathVec = tokenizePath(path);

    // Check that name of file is valid length
    if (pathVec.back().length() > 8)
    {
        return;
    }

    // Check for illegal phrases
    regex mapMarkerRegex("^E\\dM\\d$");
    regex startRegex(".*_START$");
    regex endRegex(".*_END$");
    string name = pathVec.back();
    if (regex_match(name, mapMarkerRegex) || regex_match(name, startRegex) || regex_match(name, endRegex))
    {
        return;
    }

    // Check if file already exists
    if (isContent(path))
    {
        return;
    }

    // Check if parent directory exists & it is a namespace directory
    string parentPath = "/";
    string parentEnd;

    for (int i = 0; i < pathVec.size() - 1; i++)
    {
        parentPath += pathVec[i] + "/";
        if (i == pathVec.size() - 2)
            parentEnd = pathVec[i];
    }

    if (!isDirectory(parentPath) || regex_match(parentEnd, mapMarkerRegex))
    {
        return;
    }

    // All necessary checks complete, create the new directory
    // Get the names of the end marker of the parent directory
    parentEnd += "_END";

    // Open the WAD file
    fstream wadFile;
    wadFile.open(fileName, ios::in | ios::out | ios::binary);
    if (!wadFile.is_open())
    {
        cerr << "Failed to open file: " << fileName << endl;
        return;
    }

    // Update number of descriptors
    numDescriptors++;
    wadFile.seekg(4, ios::beg);
    wadFile.write(reinterpret_cast<char *>(&numDescriptors), sizeof(numDescriptors));

    // Handle the root directory case
    if (parentPath == "/")
    {
        // Create a new file node
        Node *newFile = new Node(0, 0, path);
        Node *parentNode = nodesMap[parentPath];
        parentNode->children.push_back(newFile);
        nodesMap[path] = newFile;

        // Write in the new directory information in the end of the descriptor list
        wadFile.seekg(0, ios::end);
        uint32_t parentEndPos = wadFile.tellg();

        wadFile.write(reinterpret_cast<char *>(&newFile->offset), 4);
        wadFile.write(reinterpret_cast<char *>(&newFile->length), 4);
        wadFile.write(name.c_str(), 8);

        wadFile.close();
        return;
    }

    // Start reading from the file descriptor list
    wadFile.seekg(descriptorOffset, ios::beg);
    streampos parentEndPos = -1;
    char buffer[16];
    vector<string> currDirVec;

    while (wadFile.read(buffer, sizeof(buffer)) || wadFile.gcount() > 0)
    {
        size_t bytesRead = wadFile.gcount();

        // Extract the name and trim it
        string nameInBuffer = string(buffer + 8, 8);

        // Trim spaces and null characters
        nameInBuffer.erase(0, nameInBuffer.find_first_not_of(' '));                                     // Trim leading spaces
        nameInBuffer.erase(nameInBuffer.find_last_not_of(' ') + 1);                                     // Trim trailing spaces
        nameInBuffer.erase(remove(nameInBuffer.begin(), nameInBuffer.end(), '\0'), nameInBuffer.end()); // Remove null characters

        if (regex_match(nameInBuffer, startRegex))
        {
            string name;
            name = nameInBuffer.substr(0, nameInBuffer.size() - 6); // Remove "_START"
            currDirVec.push_back(name);
        }
        else if (regex_match(nameInBuffer, endRegex))
        {
            if (nameInBuffer != parentEnd)
            {
                currDirVec.pop_back();
            }
        }

        // Check if the name matches the parent end marker
        if (bytesRead >= 8 && nameInBuffer == parentEnd) // check for end of parent directory
        {
            string currDir = "/";
            for (auto i : currDirVec)
            {
                currDir += i + "/";
            }

            if (parentPath == currDir) // check ENTIRE path
            {
                parentEndPos = wadFile.tellg();
                parentEndPos -= 16; // Adjust to the start of the current buffer
                break;
            }
            else
            {
                currDirVec.pop_back();
            }
        }
    }

    if (parentEndPos == -1) // Parent end marker not found
    {
        wadFile.close();
        return;
    }

    shiftDataForward(wadFile, parentEndPos, 16);

    // Create a new file node
    Node *newFile = new Node(0, 0, path);
    Node *parentNode = nodesMap[parentPath];
    parentNode->children.push_back(newFile);
    nodesMap[path] = newFile;

    // Write the new descriptor at the shifted position
    wadFile.seekp(parentEndPos, ios::beg);
    wadFile.write(reinterpret_cast<char *>(&newFile->offset), 4);
    wadFile.write(reinterpret_cast<char *>(&newFile->length), 4);
    wadFile.write(name.c_str(), 8);

    wadFile.flush();
    wadFile.close();
}

int Wad::writeToFile(const string &path, const char *buffer, int length, int offset)
{
    // Check if file exists
    if (!isContent(path))
    {
        return -1;
    }

    // Check if file is empty
    Node *node = nodesMap[path];
    if (node->length != 0)
    {
        return 0;
    }

    // Open the WAD file
    fstream wadFile(fileName, ios::in | ios::out | ios::binary);
    if (!wadFile.is_open())
    {
        cerr << "Failed to open WAD file: " << fileName << endl;
        return -1;
    }

    // Shift the file descriptor forward, making space for new lump data
    shiftDataForward(wadFile, descriptorOffset, length);
    uint32_t newLumpStart = descriptorOffset;
    descriptorOffset += length;

    // Update the WAD header to reflect the new descriptor offset
    wadFile.seekp(8, ios::beg);
    wadFile.write(reinterpret_cast<char *>(&descriptorOffset), 4);

    node->length = length;
    node->offset = newLumpStart;

    // Write data from the buffer to the new lump data section
    wadFile.seekp(node->offset, ios::beg); // Use correct offset for lump data
    wadFile.write(buffer, length);

    // Find descriptor index for the given file
    int descriptorIndex = findDescriptorIndex(path);

    wadFile.seekp(descriptorOffset + (descriptorIndex * 16), ios::beg);
    wadFile.write(reinterpret_cast<char *>(&node->offset), 4);
    wadFile.write(reinterpret_cast<char *>(&node->length), 4);

    // Clean up and return
    wadFile.close();
    return length;
}

vector<string> Wad::tokenizePath(const string &path)
{
    vector<string> tokens;
    stringstream ss(path);
    string token;

    // Split by '/' delimiter
    while (getline(ss, token, '/'))
    {
        if (!token.empty())
        {
            tokens.push_back(token);
        }
    }
    return tokens;
}

int Wad::findDescriptorIndex(const string &path)
{
    vector<string> tokens = tokenizePath(path);
    string fName = tokens.back(); // get the file name

    fstream wadFile(fileName, ios::binary | ios::in);
    if (!wadFile)
    {
        throw runtime_error("Failed to open WAD file.");
    }

    // Seek to the start of the descriptor list
    wadFile.seekg(descriptorOffset, ios::beg);

    // Variables for tracking directory hierarchy
    vector<string> currPathVec;
    regex startRegex(".*_START$");
    regex endRegex(".*_END$");

    for (uint32_t i = 0; i < numDescriptors; ++i)
    {
        char name[9] = {0};  // WAD names are 8 characters, null-terminated
        uint32_t offset = 0; // Offset to the data
        uint32_t size = 0;   // Size of the data

        wadFile.read(reinterpret_cast<char *>(&offset), sizeof(offset));
        wadFile.read(reinterpret_cast<char *>(&size), sizeof(size));
        wadFile.read(name, 8); // Read the name (8 bytes)

        string nameStr(name, 8);
        nameStr.erase(0, nameStr.find_first_not_of(' ')); // Trim leading spaces
        nameStr.erase(nameStr.find_last_not_of(' ') + 1); // Trim trailing spaces
        nameStr.erase(remove(nameStr.begin(), nameStr.end(), '\0'), nameStr.end());

        if (regex_match(nameStr, startRegex))
        {
            string currName = nameStr.substr(0, nameStr.size() - 6); // Remove "_START"
            currPathVec.push_back(currName);
        }
        else if (regex_match(nameStr, endRegex))
        {
            currPathVec.pop_back();
        }

        if (nameStr == fName) // compare file name
        {
            // Compare the full path
            string currPath = "/";
            for (const auto &dir : currPathVec)
            {
                currPath += dir + "/";
            }
            currPath += nameStr;

            if (path == currPath) // Match the entire path
            {
                return i;
            }
        }
    }

    return -1; // Descriptor not found
}
