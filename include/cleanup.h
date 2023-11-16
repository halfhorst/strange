#ifndef CLEANUP_H_
#define CLEANUP_H_

/**
 * Sets SIGINT and SIGKILL dispositions to the cleanup function 
*/
void register_cleanup(void (*cleanup)(void));

#endif  // CLEANUP_H_
