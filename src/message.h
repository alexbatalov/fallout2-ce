#ifndef MESSAGE_H
#define MESSAGE_H

#define MESSAGE_LIST_ITEM_TEXT_FILTERED 0x01

#define MESSAGE_LIST_ITEM_FIELD_MAX_SIZE 1024

typedef struct MessageListItem {
    int num;
    int flags;
    char* audio;
    char* text;
} MessageListItem;

typedef struct MessageList {
    int entries_num;
    MessageListItem* entries;
} MessageList;

int badwordsInit();
void badwordsExit();
bool messageListInit(MessageList* msg);
bool messageListFree(MessageList* msg);
bool messageListLoad(MessageList* msg, const char* path);
bool messageListGetItem(MessageList* msg, MessageListItem* entry);
bool _message_make_path(char* dest, const char* path);
char* getmsg(MessageList* msg, MessageListItem* entry, int num);
bool messageListFilterBadwords(MessageList* messageList);

void messageListFilterGenderWords(MessageList* messageList, int gender);

#endif /* MESSAGE_H */
