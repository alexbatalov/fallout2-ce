#ifndef FILE_UTILS_H
#define FILE_UTILS_H

namespace fallout {

int fileCopyDecompressed(const char* existingFilePath, const char* newFilePath);
int fileCopyCompressed(const char* existingFilePath, const char* newFilePath);
int _gzdecompress_file(const char* existingFilePath, const char* newFilePath);

} // namespace fallout

#endif /* FILE_UTILS_H */
