#ifndef STRANGE_WATERMARK_H_
#define STRANGE_WATERMARK_H_

struct ScreenBuffer;

void render_screensaver_watermark(struct ScreenBuffer *buffer, int preview,
                                  int disable_key);

#endif
