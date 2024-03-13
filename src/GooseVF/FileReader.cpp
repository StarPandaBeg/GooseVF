#include "GooseVF/FileReader.h"

#include <filesystem>
#include <queue>

#include "GooseVF/Utility.h"

using namespace GooseVF;

FileReader::FileReader() {
}

FileReader::FileReader(const std::string& path) {
    open(path);
}

void FileReader::open(const std::string& path) {
    _file.open(path, std::ios::binary);
    if (!_file.is_open())
        throw std::runtime_error("File not found");

    readHeader();
    readEntryTable();
    buildEntryTree();
    if (_fileVersion > 0)
        readMetadata();

    _fileSectionBegin = _file.tellg();
}

int FileReader::contentVersion() {
    if (!_file.is_open())
        throw std::runtime_error("File is not opened");
    return _contentVersion;
}

void FileReader::readFile(const std::string& path, std::vector<char>& output) {
    if (!_file.is_open())
        throw std::runtime_error("File is not opened");

    auto parts = splitPath(path);
    if (parts[0] == ".")
        parts.erase(parts.begin());

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
    _file.seekg(_fileSectionBegin + node->offset);
    _file.read(output.data(), node->size);
}

void FileReader::iterateFiles(const std::function<void(const std::string&)> callback, const std::string& basePath, int depth) {
    if (!_file.is_open())
        throw std::runtime_error("File is not opened");
    iterateEntries(
        [&callback](const std::string& path, bool is_directory) {
            if (is_directory)
                return;
            callback(path);
        },
        basePath,
        depth);
}

void FileReader::iterateDirectories(const std::function<void(const std::string& path)> callback, const std::string& basePath, int depth) {
    if (!_file.is_open())
        throw std::runtime_error("File is not opened");
    iterateEntries(
        [&callback](const std::string& path, bool is_directory) {
            if (!is_directory)
                return;
            callback(path);
        },
        basePath,
        depth);
}

void FileReader::iterateEntries(const std::function<void(const std::string& path, bool is_directory)> callback, const std::string& basePath, int depth) {
    if (!_file.is_open())
        throw std::runtime_error("File is not opened");

    auto parts = splitPath(basePath);
    if (parts[0] == ".")
        parts.erase(parts.begin());
    if (depth >= 0)
        depth -= parts.size();
    auto parentNode = getNode(basePath);

    std::vector<int> visited;
    std::queue<std::pair<FileTreeNode*, int>> q;
    auto& arr = (parentNode == nullptr) ? _root : parentNode->children_nodes;

    for (auto& node : arr) {
        q.emplace(std::make_pair(node, 0));
    }

    while (!q.empty()) {
        auto [node, nodeDepth] = q.front();
        q.pop();
        visited.push_back(node->id);
        if (depth >= 0 && nodeDepth > depth)
            continue;

        auto path = buildPath(node);
        auto relativePath = std::filesystem::relative(path, basePath).string();
        callback(relativePath, node->type == ENTRYDATA_TYPE_DIR);

        for (auto& child : node->children_nodes) {
            if (std::find(visited.begin(), visited.end(), child->id) != visited.end())
                continue;
            q.push(std::make_pair(child, nodeDepth + 1));
        }
    }
}

bool FileReader::exists(const std::string& path) {
    if (!_file.is_open())
        throw std::runtime_error("File is not opened");

    auto parts = splitPath(path);
    if (parts[0] == ".")
        parts.erase(parts.begin());
    if (!parts.size())
        return false;

    std::vector<FileTreeNode*>* arr = &_root;
    for (size_t i = 0; i < parts.size(); i++) {
        auto& name = parts[i];
        auto it = std::find_if(arr->begin(), arr->end(),
                               [&name](FileTreeNode* n) {
                                   return (!n->name.compare(name));
                               });
        if (it == arr->end()) {
            return false;
        }
        arr = &(*it)->children_nodes;
    }
    return true;
}

bool FileReader::is_file(const std::string& path) {
    if (!_file.is_open())
        throw std::runtime_error("File is not opened");

    auto parts = splitPath(path);
    if (parts[0] == ".")
        parts.erase(parts.begin());
    if (!parts.size())
        return false;

    std::vector<FileTreeNode*>* arr = &_root;
    for (size_t i = 0; i < parts.size(); i++) {
        auto& name = parts[i];
        auto it = std::find_if(arr->begin(), arr->end(),
                               [&name](FileTreeNode* n) {
                                   return (!n->name.compare(name));
                               });
        if (it == arr->end()) {
            return false;
        }
        arr = &(*it)->children_nodes;

        if (i == parts.size() - 1) {
            return (*it)->type == ENTRYDATA_TYPE_FILE;
        }
    }
    return false;
}

bool FileReader::is_dir(const std::string& path) {
    if (!_file.is_open())
        throw std::runtime_error("File is not opened");
    auto parts = splitPath(path);
    if (parts[0] == ".")
        parts.erase(parts.begin());
    if (!parts.size())
        return false;

    std::vector<FileTreeNode*>* arr = &_root;
    for (size_t i = 0; i < parts.size(); i++) {
        auto& name = parts[i];
        auto it = std::find_if(arr->begin(), arr->end(),
                               [&name](FileTreeNode* n) {
                                   return (!n->name.compare(name));
                               });
        if (it == arr->end()) {
            return false;
        }
        arr = &(*it)->children_nodes;

        if (i == parts.size() - 1) {
            return (*it)->type == ENTRYDATA_TYPE_DIR;
        }
    }
    return false;
}

void FileReader::readHeader() {
    std::vector<char> buffer(4);
    _file.read(buffer.data(), 4);  // Read magic header

    std::string magic(buffer.begin(), buffer.end());
    if (magic != "HONK")
        throw new std::runtime_error("Invalid archive format");

    _file.read(buffer.data(), 1);  // Read file version
    _fileVersion = buffer[0];

    _file.read(buffer.data(), 4);  // Read content version
    _contentVersion = *((int*)buffer.data());
}

void FileReader::readEntryTable() {
    std::vector<char> buffer(4);
    _file.read(buffer.data(), 4);  // Read entry amount

    int totalEntries = *((int*)buffer.data());
    for (int i = 0; i < totalEntries; i++) {
        readEntry();
    }
}

void FileReader::readEntry() {
    std::vector<char> buffer(4);
    std::vector<char> buffer2(8);
    auto entry = std::make_unique<FileTreeNode>();

    _file.read(buffer.data(), 4);  // Entry id
    entry->id = *((int*)buffer.data());

    std::getline(_file, entry->name, '\0');  // Entry name
    _file.read(buffer.data(), 1);            // Entry type
    entry->type = buffer[0];

    if (entry->type == ENTRYDATA_TYPE_FILE) {
        _file.read(buffer2.data(), 8);  // File offset (from file section beginning)
        entry->offset = *((unsigned long long*)buffer2.data());
        _file.read(buffer.data(), 4);  // File size (in bytes)
        entry->size = *((int*)buffer.data());
    }
    if (entry->type == ENTRYDATA_TYPE_DIR) {
        _file.read(buffer.data(), 4);  // Amount of children ids
        int totalChildren = *((int*)buffer.data());

        for (int i = 0; i < totalChildren; i++) {
            _file.read(buffer.data(), 4);  // Child ID
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
    _file.read(buffer.data(), 4);  // Metadata names table size - always 0
    _file.read(buffer.data(), 4);  // Metadata values table size - always 0
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

FileReader::FileTreeNode* FileReader::getNode(const std::string& path) {
    auto parts = splitPath(path);
    if (parts[0] == ".")
        parts.erase(parts.begin());
    if (parts.size() == 0)
        return nullptr;

    auto* arr = &_root;
    FileReader::FileTreeNode* node = nullptr;
    for (auto& part : parts) {
        auto it = std::find_if(arr->begin(), arr->end(),
                               [&part](FileTreeNode* n) {
                                   return (!n->name.compare(part)) && (n->type == ENTRYDATA_TYPE_DIR);
                               });
        if (it == arr->end()) {
            return nullptr;
        }
        node = *it;
        arr = &node->children_nodes;
    }
    return node;
}