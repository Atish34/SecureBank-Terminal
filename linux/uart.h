#ifndef _UART_H_
#define _UART_H_

int   serialOpen      (const char *device, const int baud) ;
void  serialClose     (const int fd) ;
void  serialFlush     (const int fd) ;
void  serialPutchar   (const int fd, const unsigned char c) ;
int   serialGetchar   (const int fd) ;

/* Added for SecureBank Terminal protocol (whole-message send/receive) */
void  serialPutString (const int fd, const char *str) ;
int   serialGetString (const int fd, char *buf, int buf_size) ;

#endif
