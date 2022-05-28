#ifndef FILE_UTILS_H
#define FILE_UTILS_H

int fileCopyDecompressed(const char* existingFilePath, const char* newFilePath);
int fileCopyCompressed(const char* existingFilePath, const char* newFilePath);
int _gzdecompress_file(const char* existingFilePath, const char* newFilePath);
void fileCopy(const char* existingFilePath, const char* newFilePath, bool overwrite);

#endif /* FILE_UTILS_H */
