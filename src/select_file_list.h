#ifndef SELECT_FILE_LIST_H
#define SELECT_FILE_LIST_H

int _compare(const void* a1, const void* a2);
char** _getFileList(const char* pattern, int* fileNameListLengthPtr);
void _freeFileList(char** fileList);

#endif /* SELECT_FILE_LIST_H */
