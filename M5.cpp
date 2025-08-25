#include <iostream>
#include <fstream>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>
#include <iomanip>
#include <sstream>

#define PACKED_STRUCT __attribute__((packed))

// 文件系统引导扇区数据结构
struct PACKED_STRUCT FSBootSection {
    uint8_t bootstrapCode[3];
    uint8_t manufacturerID[8];
    uint16_t sectorByteCount;
    uint8_t clusterSectorCount;
    uint16_t reservedAreaSize;
    uint8_t fatTableCount;
    uint16_t rootDirEntries;
    uint16_t totalSectorsSmall;
    uint8_t mediaDescriptor;
    uint16_t fatSizeLegacy;
    uint16_t sectorsPerCylinder;
    uint16_t headCount;
    uint32_t hiddenSectorCount;
    uint32_t totalSectorsLarge;
    uint32_t fatSizeExtended;
    uint16_t extensionFlags;
    uint16_t filesystemVersion;
    uint32_t rootDirCluster;
    uint16_t fsInfoSector;
    uint16_t backupBootSector;
    uint8_t reservedArea[12];
    uint8_t physicalDriveNum;
    uint8_t currentHead;
    uint8_t extendedBootSig;
    uint32_t volumeSerialNum;
    char volumeName[11];
    char fsTypeLabel[8];
    uint8_t executableCode[420];
    uint16_t endSignature;
};

// 目录条目结构
struct PACKED_STRUCT DirectoryEntry {
    uint8_t fileName[11];
    uint8_t attributes;
    uint8_t caseInfo;
    uint8_t creationTimeFine;
    uint16_t creationTime;
    uint16_t creationDate;
    uint16_t lastAccessDate;
    uint16_t highClusterBits;
    uint16_t modificationTime;
    uint16_t modificationDate;
    uint16_t lowClusterBits;
    uint32_t sizeInBytes;
};

// 位图文件头结构
struct PACKED_STRUCT BitmapFileHeader {
    uint16_t magicNumber;
    uint32_t totalFileSize;
    uint16_t reservedArea1;
    uint16_t reservedArea2;
    uint32_t pixelDataOffset;
    uint32_t infoHeaderSize;
    int32_t imageWidth;
    int32_t imageHeight;
    uint16_t colorPlanes;
    uint16_t bitsPerPixel;
    uint32_t compressionMethod;
    uint32_t imageDataSize;
    int32_t horizontalResolution;
    int32_t verticalResolution;
    uint32_t paletteColorCount;
    uint32_t importantColors;
};

// 恢复的文件信息
struct RestoredFileInfo {
    std::string name;
    uint32_t startingCluster;
    uint32_t byteCount;
    std::vector<uint8_t> contentData;
};

// 转换FAT文件名格式
std::string ConvertFatFilename(const uint8_t fatName[11]) {
    char baseName[9] = { 0 };
    char extension[4] = { 0 };

    std::memcpy(baseName, fatName, 8);
    std::memcpy(extension, fatName + 8, 3);

    for (int i = 7; i >= 0 && baseName[i] == ' '; --i) {
        baseName[i] = '\0';
    }
    for (int i = 2; i >= 0 && extension[i] == ' '; --i) {
        extension[i] = '\0';
    }

    std::string resultName = baseName;
    if (extension[0] != '\0') {
        resultName += ".";
        resultName += extension;
    }

    for (char& ch : resultName) {
        if (static_cast<uint8_t>(ch) < 32 ||
            ch == '\\' || ch == '/' || ch == ':' ||
            ch == '*' || ch == '?' || ch == '"' ||
            ch == '<' || ch == '>' || ch == '|') {
            ch = '_';
        }
    }

    return resultName;
}

// 计算文件哈希值
std::string ComputeFileHash(const std::vector<uint8_t>& fileData) {
    char tempFilename[] = "/tmp/file_recovery_XXXXXX";
    int fileDesc = mkstemp(tempFilename);
    if (fileDesc == -1) {
        return "";
    }

    write(fileDesc, fileData.data(), fileData.size());
    close(fileDesc);

    std::string command = "sha1sum " + std::string(tempFilename) + " | awk '{print $1}'";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        remove(tempFilename);
        return "";
    }

    char hashBuffer[41] = { 0 };
    if (fscanf(pipe, "%40s", hashBuffer) != 1) {
        hashBuffer[0] = '\0';
    }

    pclose(pipe);
    remove(tempFilename);

    return std::string(hashBuffer);
}

// 主程序入口
int main(int argCount, char* argValues[]) {
    if (argCount != 2) {
        std::cout << "使用方法: " << argValues[0] << " <磁盘镜像文件>" << std::endl;
        std::cout << "使用默认测试文件: fs.img" << std::endl;
        char* defaultArgs[] = { argValues[0], (char*)"fs.img", nullptr };
        argCount = 2;
        argValues = defaultArgs;
    }

    int diskFile = open(argValues[1], O_RDONLY);
    if (diskFile == -1) {
        perror("打开文件失败");
        return EXIT_FAILURE;
    }

    struct stat fileStats;
    if (fstat(diskFile, &fileStats)) {
        perror("获取文件状态失败");
        close(diskFile);
        return EXIT_FAILURE;
    }

    void* mappedAddress = mmap(nullptr, fileStats.st_size, PROT_READ, MAP_PRIVATE, diskFile, 0);
    if (mappedAddress == MAP_FAILED) {
        perror("内存映射失败");
        close(diskFile);
        return EXIT_FAILURE;
    }

    const uint8_t* diskData = static_cast<const uint8_t*>(mappedAddress);
    const FSBootSection* bootSection = reinterpret_cast<const FSBootSection*>(diskData);

    if (bootSection->endSignature != 0xAA55) {
        std::cout << "无效的引导扇区签名" << std::endl;
        munmap(mappedAddress, fileStats.st_size);
        close(diskFile);
        return EXIT_FAILURE;
    }

    const uint32_t bytesPerSector = bootSection->sectorByteCount;
    const uint32_t sectorsPerCluster = bootSection->clusterSectorCount;
    const uint32_t clusterByteSize = bytesPerSector * sectorsPerCluster;
    const uint32_t reservedSectors = bootSection->reservedAreaSize;
    const uint32_t fatCount = bootSection->fatTableCount;

    uint32_t fatSectorSize = 0;
    if (bootSection->fatSizeLegacy != 0) {
        fatSectorSize = bootSection->fatSizeLegacy;
    }
    else {
        fatSectorSize = bootSection->fatSizeExtended;
    }

    const uint32_t fatAreaStart = reservedSectors * bytesPerSector;
    const uint32_t dataAreaStart = fatAreaStart + (fatCount * fatSectorSize * bytesPerSector);
    const uint32_t totalClusters = (fileStats.st_size - dataAreaStart) / clusterByteSize;

    std::vector<RestoredFileInfo> recoveredFiles;
    std::unordered_map<uint32_t, bool> processedClusters;

    for (uint32_t clusterIndex = 2; clusterIndex < totalClusters; ++clusterIndex) {
        if (processedClusters[clusterIndex]) {
            continue;
        }

        const uint32_t clusterOffset = dataAreaStart + (clusterIndex - 2) * clusterByteSize;
        const uint8_t* clusterContent = diskData + clusterOffset;

        if (clusterByteSize >= sizeof(BitmapFileHeader)) {
            const BitmapFileHeader* bmpHeader = reinterpret_cast<const BitmapFileHeader*>(clusterContent);

            if (bmpHeader->magicNumber == 0x4D42 &&
                bmpHeader->bitsPerPixel == 24 &&
                bmpHeader->compressionMethod == 0 &&
                bmpHeader->imageWidth > 0 &&
                bmpHeader->imageHeight > 0) {

                std::string filename = "restored_" + std::to_string(clusterIndex) + ".bmp";
                uint32_t fileSize = bmpHeader->totalFileSize;
                uint32_t requiredClusters = (fileSize + clusterByteSize - 1) / clusterByteSize;

                if (clusterIndex + requiredClusters <= totalClusters) {
                    RestoredFileInfo fileInfo;
                    fileInfo.name = filename;
                    fileInfo.startingCluster = clusterIndex;
                    fileInfo.byteCount = fileSize;
                    fileInfo.contentData.reserve(fileSize);

                    for (uint32_t i = 0; i < requiredClusters; ++i) {
                        uint32_t currentCluster = clusterIndex + i;
                        uint32_t currentOffset = dataAreaStart + (currentCluster - 2) * clusterByteSize;
                        uint32_t copySize = std::min(clusterByteSize, fileSize - static_cast<uint32_t>(fileInfo.contentData.size()));

                        fileInfo.contentData.insert(fileInfo.contentData.end(),
                            diskData + currentOffset,
                            diskData + currentOffset + copySize);
                        processedClusters[currentCluster] = true;
                    }

                    recoveredFiles.push_back(fileInfo);
                }
            }
        }

        for (uint32_t offset = 0; offset < clusterByteSize; offset += 32) {
            const DirectoryEntry* dirEntry = reinterpret_cast<const DirectoryEntry*>(clusterContent + offset);

            if (dirEntry->fileName[0] == 0x00 ||
                dirEntry->fileName[0] == 0xE5 ||
                (dirEntry->attributes & 0x0F) == 0x0F) {
                continue;
            }

            if (!(dirEntry->attributes & 0x20) || (dirEntry->attributes & 0x16)) {
                continue;
            }

            std::string filename = ConvertFatFilename(dirEntry->fileName);
            if (filename.size() < 4 ||
                filename.substr(filename.size() - 4) != ".bmp") {
                continue;
            }

            uint32_t startCluster = (dirEntry->highClusterBits << 16) | dirEntry->lowClusterBits;
            uint32_t fileSize = dirEntry->sizeInBytes;

            if (startCluster < 2 || startCluster >= totalClusters || fileSize == 0) {
                continue;
            }

            uint32_t clustersNeeded = (fileSize + clusterByteSize - 1) / clusterByteSize;

            if (startCluster + clustersNeeded <= totalClusters) {
                RestoredFileInfo fileInfo;
                fileInfo.name = filename;
                fileInfo.startingCluster = startCluster;
                fileInfo.byteCount = fileSize;
                fileInfo.contentData.reserve(fileSize);

                for (uint32_t i = 0; i < clustersNeeded; ++i) {
                    uint32_t currentCluster = startCluster + i;
                    uint32_t currentOffset = dataAreaStart + (currentCluster - 2) * clusterByteSize;
                    uint32_t copySize = std::min(clusterByteSize, fileSize - static_cast<uint32_t>(fileInfo.contentData.size()));

                    fileInfo.contentData.insert(fileInfo.contentData.end(),
                        diskData + currentOffset,
                        diskData + currentOffset + copySize);
                    processedClusters[currentCluster] = true;
                }

                recoveredFiles.push_back(fileInfo);
            }
        }
    }

    for (const auto& file : recoveredFiles) {
        if (file.contentData.size() == file.byteCount) {
            std::string hashValue = ComputeFileHash(file.contentData);
            std::cout << hashValue << "  " << file.name << std::endl;
        }
    }

    munmap(mappedAddress, fileStats.st_size);
    close(diskFile);

    return EXIT_SUCCESS;
}