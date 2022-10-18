#ifndef MAIN_H
#define MAIN_H

namespace fallout {

int falloutMain(int argc, char** argv);

// True if game was started, false when on the main menu
bool mainIsGameLoaded();
void mainSetIsGameLoaded(bool isGameLoaded);

bool mainIsInEndgameSlideshow();
void mainSetIsInEndgameSlideshow(bool isInEndgameSlideshow);

} // namespace fallout

#endif /* MAIN_H */
