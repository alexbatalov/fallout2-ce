#ifndef GRAPH_LIB_H
#define GRAPH_LIB_H

extern int* _dad_2;
extern int _match_length;
extern int _textsize;
extern int* _rson;
extern int* _lson;
extern unsigned char* _text_buf;
extern int _codesize;
extern int _match_position;

int graphCompress(unsigned char* a1, unsigned char* a2, int a3);
void _InitTree();
void _InsertNode(int a1);
void _DeleteNode(int a1);
int graphDecompress(unsigned char* a1, unsigned char* a2, int a3);

#endif /* GRAPH_LIB_H */
