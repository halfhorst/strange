
// Basic screensaver functions
void start_screensaver(void);
void stop_screensaver(void);

// Terminal mode functions
void enable_raw_mode(void);
void disable_raw_mode(void);

// PTY screensaver functions
int run_screensaver_loop(void);
void set_screensaver_timeout(int seconds);
int get_screensaver_timeout(void);
void reset_activity_timer(void);
int check_screensaver_timeout(void);
void cleanup_screensaver(int sig);
