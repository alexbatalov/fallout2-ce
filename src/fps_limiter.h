#ifndef FPS_LIMITER_H
#define FPS_LIMITER_H

class FpsLimiter {
public:
    FpsLimiter(size_t fps = 60);
    void mark();
    void throttle() const;

private:
    const size_t _fps;
    size_t _ticks;
};

#endif /* FPS_LIMITER_H */
