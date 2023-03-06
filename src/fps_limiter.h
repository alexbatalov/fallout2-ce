#ifndef FPS_LIMITER_H
#define FPS_LIMITER_H

namespace fallout {

class FpsLimiter {
public:
    FpsLimiter(unsigned int fps = 60);
    void mark();
    void throttle() const;

private:
    const unsigned int _fps;
    unsigned int _ticks;
};



class FpsThrottler {
    public:
        FpsThrottler();
        void operator()(int target_fps);
    private:
        unsigned int last_time_tick;
};


} // namespace fallout


#endif /* FPS_LIMITER_H */
