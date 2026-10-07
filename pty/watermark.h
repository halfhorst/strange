#ifndef STRANGE_WATERMARK_H_
#define STRANGE_WATERMARK_H_

struct strange_screen_buffer;

void strange_watermark_render(struct strange_screen_buffer *buffer, int preview,
                              int disable_key);

#endif
