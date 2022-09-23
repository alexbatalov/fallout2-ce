#ifndef FPS_LIMITER_H
#define FPS_LIMITER_H

#include <cstddef>

namespace fallout {

class FpsLimiter {
public:
    FpsLimiter(std::size_t fps = 60);
    void mark();
    void throttle() const;

private:
    const std::size_t _fps;
    std::size_t _ticks;
};

} // namespace fallout

#endif /* FPS_LIMITER_H */
