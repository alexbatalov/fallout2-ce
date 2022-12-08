#ifndef FALLOUT_INPUT_H_
#define FALLOUT_INPUT_H_

namespace fallout {

typedef void(IdleFunc)();
typedef void(FocusFunc)(bool focus);
typedef void(TickerProc)();

typedef int(PauseHandler)();
typedef int(ScreenshotHandler)(int width, int height, unsigned char* buffer, unsigned char* palette);

int inputInit(int a1);
void inputExit();
int inputGetInput();
void _process_bk();
void enqueueInputEvent(int a1);
void inputEventQueueReset();
void tickersExecute();
void tickersAdd(TickerProc* fn);
void tickersRemove(TickerProc* fn);
void tickersEnable();
void tickersDisable();
void pauseHandlerConfigure(int keyCode, PauseHandler* fn);
void takeScreenshot();
int screenshotHandlerDefaultImpl(int width, int height, unsigned char* data, unsigned char* palette);
void screenshotHandlerConfigure(int keyCode, ScreenshotHandler* handler);
unsigned int getTicks();
void inputPauseForTocks(unsigned int ms);
void inputBlockForTocks(unsigned int ms);
unsigned int getTicksSince(unsigned int a1);
unsigned int getTicksBetween(unsigned int a1, unsigned int a2);
unsigned int _get_bk_time();
void inputSetKeyboardKeyRepeatRate(int value);
int inputGetKeyboardKeyRepeatRate();
void inputSetKeyboardKeyRepeatDelay(int value);
int inputGetKeyboardKeyRepeatDelay();
void inputSetFocusFunc(FocusFunc* func);
FocusFunc* inputGetFocusFunc();
void inputSetIdleFunc(IdleFunc* func);
IdleFunc* inputGetIdleFunc();
int _GNW95_input_init();
void _GNW95_process_message();
void _GNW95_clear_time_stamps();
void _GNW95_lost_focus();

void beginTextInput();
void endTextInput();

} // namespace fallout

#endif /* FALLOUT_INPUT_H_ */
