#ifndef SELECT_FILE_LIST_H
#define SELECT_FILE_LIST_H

namespace fallout {

int _compare(const void* a, const void* b);
char** _getFileList(const char* pattern, int* fileNameListLengthPtr);
void _freeFileList(char** fileList);

} // namespace fallout

#endif /* SELECT_FILE_LIST_H */
