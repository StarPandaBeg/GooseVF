#pragma once

#include <fstream>
#include <string>
#include <vector>

namespace GooseVF {
    class FileWriter {
       public:
        void setFileVersion(int version);
        void addFile(const std::string& path, const std::string& targetPath);
        void addFile(const std::string& path);
        void save(const std::string& path);

       private:
        struct EntryData {
            int id = -1;
            std::string name;
            int type = -1;

            unsigned long long offset = -1;
            int size = -1;

            std::string originalPath;
            std::vector<EntryData> children;
        };

        std::vector<EntryData> _data;
        std::vector<std::string> _files;

        int _fileVersion = 0;
        int _idCounter = 0;
        unsigned long long _fileOffsetCounter = 0;

        EntryData* createDirectories(const std::vector<std::string>& path);
        void createFile(const std::string& filename, const std::string& realPath, std::vector<EntryData>& parentArray);

        void writeEntryTable(std::ofstream& of);
        void writeEntry(std::ofstream& of, EntryData* data);

        void writeMetadata(std::ofstream& of);

        void writeFilesData(std::ofstream& of);
        void writeFileData(std::ofstream& of, const std::string& filename);
    };
}  // namespace GooseVF
