#pragma once

#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace GooseVF {
    class FileReader {
       public:
        FileReader();
        FileReader(const std::string& path);

        void open(const std::string& path);

        int contentVersion();
        void readFile(const std::string& path, std::vector<char>& output);

        void iterateFiles(const std::function<void(const std::string& path)> callback, const std::string& basePath = "./", int depth = -1);
        void iterateDirectories(const std::function<void(const std::string& path)> callback, const std::string& basePath = "./", int depth = -1);
        void iterateEntries(const std::function<void(const std::string& path, bool is_directory)> callback, const std::string& basePath = "./", int depth = -1);

        bool exists(const std::string& path);
        bool is_file(const std::string& path);
        bool is_dir(const std::string& path);

       private:
        struct FileTreeNode {
            int id;
            std::string name;
            int type;

            unsigned long long offset;
            int size;

            FileTreeNode* parent;
            std::vector<int> children;
            std::vector<FileTreeNode*> children_nodes;
        };

        std::vector<FileTreeNode*> _root;
        std::map<int, std::unique_ptr<FileTreeNode>> _nodes;
        std::ifstream _file;
        int _fileVersion;
        int _contentVersion;
        unsigned long long _fileSectionBegin;

        void readHeader();
        void readEntryTable();
        void readEntry();
        void buildEntryTree();
        void readMetadata();

        std::string buildPath(FileTreeNode* node);
        std::string buildPath(FileTreeNode* node, const std::vector<std::string>& skip);

        FileTreeNode* getNode(const std::string& path);
    };
}  // namespace GooseVF
