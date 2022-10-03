#ifndef GAME_DIALOG_H
#define GAME_DIALOG_H

#include "interpreter.h"
#include "obj_types.h"

namespace fallout {

extern Object* gGameDialogSpeaker;
extern bool gGameDialogSpeakerIsPartyMember;
extern int gGameDialogHeadFid;
extern int gGameDialogSid;

int gameDialogInit();
int gameDialogReset();
int gameDialogExit();
bool _gdialogActive();
void gameDialogEnter(Object* speaker, int a2);
void _gdialogSystemEnter();
void gameDialogStartLips(const char* a1);
int gameDialogEnable();
int gameDialogDisable();
int _gdialogInitFromScript(int headFid, int reaction);
int _gdialogExitFromScript();
void gameDialogSetBackground(int a1);
void gameDialogRenderSupplementaryMessage(char* msg);
int _gdialogStart();
int _gdialogSayMessage();
int gameDialogAddMessageOptionWithProcIdentifier(int messageListId, int messageId, const char* a3, int reaction);
int gameDialogAddTextOptionWithProcIdentifier(int messageListId, const char* text, const char* a3, int reaction);
int gameDialogAddMessageOptionWithProc(int messageListId, int messageId, int proc, int reaction);
int gameDialogAddTextOptionWithProc(int messageListId, const char* text, int proc, int reaction);
int gameDialogSetMessageReply(Program* a1, int a2, int a3);
int gameDialogSetTextReply(Program* a1, int a2, const char* a3);
int _gdialogGo();
void _gdialogUpdatePartyStatus();
void _talk_to_critter_reacts(int a1);
void gameDialogSetBarterModifier(int modifier);
int gameDialogBarter(int modifier);
void _barter_end_to_talk_to();

} // namespace fallout

#endif /* GAME_DIALOG_H */
