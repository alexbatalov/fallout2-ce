#ifndef MOUSE_MANAGER_H
#define MOUSE_MANAGER_H

extern char* (*_mouseNameMangler)(char*);
extern int (*_rateCallback)();
extern int (*_currentTimeCallback)();

char* _defaultNameMangler(char* a1);
int _defaultRateCallback();
int _defaultTimeCallback();
void _mousemgrSetNameMangler(char* (*func)(char*));
void _initMousemgr();
void _mouseHide();
void _mouseShow();

#endif /* MOUSE_MANAGER_H */
