#ifndef SCREENSAVER_H_
#define SCREENSAVER_H_

#include <stdint.h>
#include <stdbool.h>

struct ScreenBuffer;

struct ScreenSaver {
    void (*init)(struct ScreenBuffer *screenbuffer);
    bool (*update)(struct ScreenBuffer *screenbuffer, uint64_t time, u_int32_t dt);
    void (*cleanup)(void);
    uint16_t character_width;
};  

void init_screensaver(
    void (*ss_init)(struct ScreenBuffer *screenbuffer),
    bool (*ss_update)(struct ScreenBuffer *screenbuffer, uint64_t time, u_int32_t dt),
    void (*ss_cleanup)(void),
    uint16_t character_width,
    struct ScreenSaver *ret
);

#endif  // SCREENSAVER_H_
