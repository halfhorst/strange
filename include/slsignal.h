#ifndef SL_SIGNAL_H_
#define SL_SIGNAL_H_


/**
 * Sets SIGINT and SIGKILL dispositions to the cleanup function 
*/
void register_cleanup(void (*cleanup)(void));


#endif  // SL_SIGNAL_H_
