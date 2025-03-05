#ifndef FALLOUT_INPUT_H_
#define FALLOUT_INPUT_H_

namespace fallout {

typedef void(TickerProc)();

typedef int(ScreenshotHandler)(int width, int height, unsigned char* buffer, unsigned char* palette);

int inputInit(int a1);
void inputExit();
int inputGetInput();
void get_input_position(int* x, int* y);
void _process_bk();
void enqueueInputEvent(int a1);
void inputEventQueueReset();
void tickersExecute();
void tickersAdd(TickerProc* fn);
void tickersRemove(TickerProc* fn);
void tickersEnable();
void tickersDisable();
void takeScreenshot();
int screenshotHandlerDefaultImpl(int width, int height, unsigned char* data, unsigned char* palette);
void screenshotHandlerConfigure(int keyCode, ScreenshotHandler* handler);
unsigned int getTicks();
void inputPauseForTocks(unsigned int ms);
void inputBlockForTocks(unsigned int ms);
unsigned int getTicksSince(unsigned int a1);
unsigned int getTicksBetween(unsigned int a1, unsigned int a2);
unsigned int _get_bk_time();
int _GNW95_input_init();
void _GNW95_process_message();
void _GNW95_clear_time_stamps();
void _GNW95_lost_focus();

void beginTextInput();
void endTextInput();

} // namespace fallout

#endif /* FALLOUT_INPUT_H_ */
