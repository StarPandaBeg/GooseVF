#include "GooseVF/FileWriter.h"

#include <filesystem>
#include <queue>

#include "GooseVF/Utility.h"
#include "GooseVF/pch.h"

using namespace GooseVF;

void FileWriter::setFileVersion(int version) {
    _fileVersion = version;
}

void FileWriter::addFile(const std::string& path, const std::string& targetPath) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("File not found");
    }
    if (std::filesystem::file_size(path) > ULONG_MAX) {
        throw std::runtime_error("File is too big.");
    }

    auto parts = splitPath(targetPath);
    auto fileName = parts[parts.size() - 1];

    std::transform(fileName.begin(), fileName.end(), fileName.begin(),
                   [](unsigned char c) { return std::tolower(c); });  // Convert file name to lower case

    EntryData* parent = createDirectories(parts);
    auto& arr = (parent == nullptr) ? _data : parent->children;
    createFile(fileName, path, arr);
}

void FileWriter::addFile(const std::string& path) {
    addFile(path, path);
}

void FileWriter::save(const std::string& path) {
    std::ofstream of(path, std::ios::out | std::ios::binary);

    of << "HONK";                                         // Magic header
    of << (BYTE)0;                                        // Format version
    of.write(reinterpret_cast<char*>(&_fileVersion), 4);  // File content version

    writeEntryTable(of);
    // writeMetadata(of); // Will be added in version 2
    writeFilesData(of);
}

FileWriter::EntryData* FileWriter::createDirectories(const std::vector<std::string>& path) {
    EntryData* parent = nullptr;

    for (size_t i = 0; i < path.size() - 1; i++) {
        auto& arr = (parent == nullptr) ? _data : parent->children;
        auto name = path[i];
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c) { return std::tolower(c); });  // Convert directory name to lower case

        bool found = false;
        for (auto& entry : arr) {
            if (entry.name == name) {
                found = true;
                parent = &entry;
                break;
            }
        }
        if (found)
            continue;

        arr.emplace_back();

        auto& dir = *(arr.end() - 1);
        dir.name = name;
        dir.type = ENTRYDATA_TYPE_DIR;
        dir.id = _idCounter++;

        parent = &(*(arr.end() - 1));
    }

    return parent;
}

void FileWriter::createFile(const std::string& filename, const std::string& realPath, std::vector<EntryData>& parentArray) {
    parentArray.emplace_back();

    auto& file = *(parentArray.end() - 1);
    file.name = filename;

    file.id = _idCounter++;
    file.type = ENTRYDATA_TYPE_FILE;
    file.originalPath = realPath;

    uintmax_t fileSize = std::filesystem::file_size(realPath);
    file.offset = _fileOffsetCounter;
    file.size = fileSize;

    _fileOffsetCounter += fileSize;
    _files.push_back(realPath);
}

void FileWriter::writeEntryTable(std::ofstream& of) {
    int totalEntries = _idCounter;
    of.write(reinterpret_cast<char*>(&totalEntries), 4);

    std::queue<EntryData*> entries;
    for (auto& entry : _data) {
        entries.push(&entry);
    }

    while (!entries.empty()) {
        auto entry = entries.front();
        entries.pop();
        writeEntry(of, entry);

        if (entry->type == ENTRYDATA_TYPE_DIR) {
            for (auto& child : entry->children) {
                entries.push(&child);
            }
        }
    }
}

void FileWriter::writeEntry(std::ofstream& of, EntryData* data) {
    auto childAmount = data->children.size();

    of.write(reinterpret_cast<char*>(&data->id), 4);
    of << data->name << '\0';
    of.write(reinterpret_cast<char*>(&data->type), 1);

    if (data->type == ENTRYDATA_TYPE_DIR) {
        of.write(reinterpret_cast<char*>(&childAmount), 4);
        for (auto& child : data->children) {
            of.write(reinterpret_cast<char*>(&child.id), 4);
        }
    }
    if (data->type == ENTRYDATA_TYPE_FILE) {
        of.write(reinterpret_cast<char*>(&data->offset), 8);
        of.write(reinterpret_cast<char*>(&data->size), 4);
    }
}

void FileWriter::writeMetadata(std::ofstream& of) {
    auto zero = 0;
    of.write(reinterpret_cast<char*>(&zero), 4);
    of.write(reinterpret_cast<char*>(&zero), 4);
}

void FileWriter::writeFilesData(std::ofstream& of) {
    for (auto& filename : _files) {
        writeFileData(of, filename);
    }
}

void FileWriter::writeFileData(std::ofstream& of, const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    std::vector<char> buffer(1024);

    while (!file.eof()) {
        file.read(buffer.data(), buffer.size());
        auto actualRead = file.gcount();
        of.write(buffer.data(), actualRead);
    }
}
