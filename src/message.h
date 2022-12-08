#ifndef MESSAGE_H
#define MESSAGE_H

#include <stddef.h>

namespace fallout {

#define MESSAGE_LIST_ITEM_TEXT_FILTERED 0x01

#define MESSAGE_LIST_ITEM_FIELD_MAX_SIZE 1024

// CE: Working with standard message lists is tricky in Sfall. Many message
// lists are initialized only for the duration of appropriate modal window. This
// is not documented in Sfall and shifts too much responsibility to scripters
// (who should check game mode before accessing volatile message lists). For now
// CE only exposes persistent standard message lists:
// - combat.msg
// - combatai.msg
// - scrname.msg
// - misc.msg
// - item.msg
// - map.msg
// - proto.msg
// - script.msg
// - skill.msg
// - stat.msg
// - trait.msg
// - worldmap.msg
enum StandardMessageList {
    STANDARD_MESSAGE_LIST_COMBAT,
    STANDARD_MESSAGE_LIST_COMBAT_AI,
    STANDARD_MESSAGE_LIST_SCRNAME,
    STANDARD_MESSAGE_LIST_MISC,
    STANDARD_MESSAGE_LIST_CUSTOM,
    STANDARD_MESSAGE_LIST_INVENTORY,
    STANDARD_MESSAGE_LIST_ITEM,
    STANDARD_MESSAGE_LIST_LSGAME,
    STANDARD_MESSAGE_LIST_MAP,
    STANDARD_MESSAGE_LIST_OPTIONS,
    STANDARD_MESSAGE_LIST_PERK,
    STANDARD_MESSAGE_LIST_PIPBOY,
    STANDARD_MESSAGE_LIST_QUESTS,
    STANDARD_MESSAGE_LIST_PROTO,
    STANDARD_MESSAGE_LIST_SCRIPT,
    STANDARD_MESSAGE_LIST_SKILL,
    STANDARD_MESSAGE_LIST_SKILLDEX,
    STANDARD_MESSAGE_LIST_STAT,
    STANDARD_MESSAGE_LIST_TRAIT,
    STANDARD_MESSAGE_LIST_WORLDMAP,
    STANDARD_MESSAGE_LIST_COUNT,
};

enum {
    PROTO_MESSAGE_LIST_ITEMS,
    PROTO_MESSAGE_LIST_CRITTERS,
    PROTO_MESSAGE_LIST_SCENERY,
    PROTO_MESSAGE_LIST_TILES,
    PROTO_MESSAGE_LIST_WALLS,
    PROTO_MESSAGE_LIST_MISC,
    PROTO_MESSAGE_LIST_COUNT,
};

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
bool _message_make_path(char* dest, size_t size, const char* path);
char* getmsg(MessageList* msg, MessageListItem* entry, int num);
bool messageListFilterBadwords(MessageList* messageList);

void messageListFilterGenderWords(MessageList* messageList, int gender);

bool messageListRepositoryInit();
void messageListRepositoryReset();
void messageListRepositoryExit();
void messageListRepositorySetStandardMessageList(int messageListId, MessageList* messageList);
void messageListRepositorySetProtoMessageList(int messageListId, MessageList* messageList);
int messageListRepositoryAddExtra(int messageListId, const char* path);
char* messageListRepositoryGetMsg(int messageListId, int messageId);

} // namespace fallout

#endif /* MESSAGE_H */
