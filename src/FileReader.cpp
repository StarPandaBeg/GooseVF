#include "FileReader.h"

#include <queue>

#include "Utility.h"
#include "pch.h"

using namespace GooseVF;

FileReader::FileReader(std::string path) {
    file.open(path, std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("File not found");

    readHeader();
    readEntryTable();
    buildEntryTree();
    if (_fileVersion > 0)
        readMetadata();

    _fileSectionBegin = file.tellg();
}

int FileReader::contentVersion() {
    return _contentVersion;
}

void FileReader::readFile(const std::string& path, std::vector<char>& output) {
    auto parts = splitPath(path);
    if (!parts.size())
        throw std::runtime_error("File not found.");

    FileTreeNode* node = nullptr;
    std::vector<FileTreeNode*>* arr = &_root;
    for (size_t i = 0; i < parts.size(); i++) {
        auto& name = parts[i];
        auto expectedType = (i < (parts.size() - 1)) ? ENTRYDATA_TYPE_DIR : ENTRYDATA_TYPE_FILE;

        auto it = std::find_if(arr->begin(), arr->end(),
                               [&name, &expectedType](FileTreeNode* n) {
                                   return (!n->name.compare(name)) && (n->type == expectedType);
                               });
        if (it == arr->end()) {
            throw std::runtime_error("File not found.");
        }
        node = *it;
        arr = &node->children_nodes;
    }

    output.resize(node->size);
    file.seekg(_fileSectionBegin + node->offset);
    file.read(output.data(), node->size);
}

void FileReader::iterateFiles(const std::function<void(const std::string&)> callback) {
    iterateFiles(callback, "\\");
}

void FileReader::iterateFiles(const std::function<void(const std::string&)> callback, const std::string& rootDir) {
    std::vector<int> visited;
    std::queue<FileTreeNode*> q;
    std::vector<std::string> rootPath;

    auto* arr = &_root;
    if (!rootDir.empty() && rootDir.compare("\\") != 0) {
        rootPath = splitPath(rootDir);

        for (auto& part : rootPath) {
            auto it = std::find_if(arr->begin(), arr->end(),
                                   [&part](FileTreeNode* n) {
                                       return (!n->name.compare(part)) && (n->type == ENTRYDATA_TYPE_DIR);
                                   });
            if (it == arr->end()) {
                throw std::runtime_error("Directory not found.");
            }
            arr = &(*it)->children_nodes;
        }
    }

    for (auto& node : *arr) {
        q.emplace(node);
    }

    while (!q.empty()) {
        auto node = q.front();
        q.pop();
        visited.push_back(node->id);

        if (node->type == ENTRYDATA_TYPE_FILE) {
            auto path = buildPath(node, rootPath);
            callback(path);
            continue;
        }

        for (auto& child : node->children_nodes) {
            if (std::find(visited.begin(), visited.end(), child->id) != visited.end())
                continue;
            q.push(child);
        }
    }
}

void FileReader::readHeader() {
    std::vector<char> buffer(4);
    file.read(buffer.data(), 4);  // Read magic header

    std::string magic(buffer.begin(), buffer.end());
    if (magic != "HONK")
        throw new std::runtime_error("Invalid archive format");

    file.read(buffer.data(), 1);  // Read file version
    _fileVersion = buffer[0];

    file.read(buffer.data(), 4);  // Read content version
    _contentVersion = *((int*)buffer.data());
}

void FileReader::readEntryTable() {
    std::vector<char> buffer(4);
    file.read(buffer.data(), 4);  // Read entry amount

    int totalEntries = *((int*)buffer.data());
    for (int i = 0; i < totalEntries; i++) {
        readEntry();
    }
}

void FileReader::readEntry() {
    std::vector<char> buffer(4);
    std::vector<char> buffer2(8);
    auto entry = std::make_unique<FileTreeNode>();

    file.read(buffer.data(), 4);  // Entry id
    entry->id = *((int*)buffer.data());

    std::getline(file, entry->name, '\0');  // Entry name
    file.read(buffer.data(), 1);            // Entry type
    entry->type = buffer[0];

    if (entry->type == ENTRYDATA_TYPE_FILE) {
        file.read(buffer2.data(), 8);  // File offset (from file section beginning)
        entry->offset = *((unsigned long long*)buffer2.data());
        file.read(buffer.data(), 4);  // File size (in bytes)
        entry->size = *((int*)buffer.data());
    }
    if (entry->type == ENTRYDATA_TYPE_DIR) {
        file.read(buffer.data(), 4);  // Amount of children ids
        int totalChildren = *((int*)buffer.data());

        for (int i = 0; i < totalChildren; i++) {
            file.read(buffer.data(), 4);  // Child ID
            int child = *((int*)buffer.data());
            entry->children.push_back(child);
        }
    }

    if (_nodes.count(entry->id))
        throw std::runtime_error("File is corrupted. It contains duplicate entries.");
    _nodes[entry->id] = std::move(entry);
}

void FileReader::buildEntryTree() {
    for (const auto& [id, data] : _nodes) {
        _root.push_back(data.get());
    }

    std::vector<FileTreeNode*> needToRemove;
    for (size_t i = 0; i < _nodes.size(); i++) {
        auto& node = _nodes[i];
        if (node->type != ENTRYDATA_TYPE_DIR)
            continue;

        for (auto& childId : node->children) {
            if (!_nodes.count(childId))
                throw std::runtime_error("File is corrupted. There are missing entries.");

            auto* parent = node.get();
            while (parent != nullptr) {
                if (parent->id == childId)
                    throw std::runtime_error("File is corrupted. It contains recursive entries.");
                parent = parent->parent;
            }

            auto* child = _nodes[childId].get();
            child->parent = node.get();
            node->children_nodes.push_back(child);
            needToRemove.push_back(child);
        }
    }

    _root.erase(std::remove_if(_root.begin(), _root.end(),
                               [needToRemove](FileTreeNode* node) {
                                   return std::find(needToRemove.begin(), needToRemove.end(), node) != needToRemove.end();
                               }),
                _root.end());
}

void FileReader::readMetadata() {
    std::vector<char> buffer(4);
    file.read(buffer.data(), 4);  // Metadata names table size - always 0
    file.read(buffer.data(), 4);  // Metadata values table size - always 0
}

std::string FileReader::buildPath(FileTreeNode* node) {
    std::vector<std::string> skip(0);
    return buildPath(node, skip);
}

std::string FileReader::buildPath(FileTreeNode* node, const std::vector<std::string>& skip) {
    std::vector<std::string> path;

    auto current = node;
    while (current != nullptr) {
        path.push_back(current->name);
        current = current->parent;
    }

    int minSize = std::min(skip.size(), path.size());
    int toRemove = 0;
    for (int i = 0; i < minSize; i++) {
        if (path[path.size() - i - 1] != skip[i])
            break;
        toRemove++;
    }

    path.resize(path.size() - toRemove);
    std::reverse(path.begin(), path.end());

    return GooseVF::buildPath(path);
}
