#include <stdint.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>   /* added: declares read(), write(), close(), usleep() */
#include <string.h>   /* added: for strlen() in serialPutString */


/* serialOpen: Open and initialise the serial port, 
   setting all the right port parameters - or 
   as many as are required - hopefully! */

int serialOpen (const char *device, const int baud)
{
  struct termios options ;
  speed_t myBaud ;
  int     status, fd ;

  switch (baud)
  {
    case     50:	myBaud =     B50 ; break ;
    case     75:	myBaud =     B75 ; break ;
    case    110:	myBaud =    B110 ; break ;
    case    134:	myBaud =    B134 ; break ;
    case    150:	myBaud =    B150 ; break ;
    case    200:	myBaud =    B200 ; break ;
    case    300:	myBaud =    B300 ; break ;
    case    600:	myBaud =    B600 ; break ;
    case   1200:	myBaud =   B1200 ; break ;
    case   1800:	myBaud =   B1800 ; break ;
    case   2400:	myBaud =   B2400 ; break ;
    case   4800:	myBaud =   B4800 ; break ;
    case   9600:	myBaud =   B9600 ; break ;
    case  19200:	myBaud =  B19200 ; break ;
    case  38400:	myBaud =  B38400 ; break ;
    case  57600:	myBaud =  B57600 ; break ;
    case 115200:	myBaud = B115200 ; break ;
    case 230400:	myBaud = B230400 ; break ;

    default:
      return -2 ;
  }

  if ((fd = open (device, O_RDWR | O_NOCTTY )) == -1)
    return -1 ;

  // Get and modify current options:

  tcgetattr (fd, &options) ;

    cfmakeraw   (&options) ;
    cfsetispeed (&options, myBaud) ;
    cfsetospeed (&options, myBaud) ;

    options.c_cflag |= (CLOCAL | CREAD) ;
    options.c_cflag &= ~PARENB ;
    options.c_cflag &= ~CSTOPB ;
    options.c_cflag &= ~CSIZE ;
    options.c_cflag |= CS8 ;
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG) ;
    options.c_oflag &= ~OPOST ;

  tcsetattr (fd, TCSANOW | TCSAFLUSH, &options) ;

  usleep (10000) ;	// 10mS

  return fd ;
}


/* serialFlush: Flush the serial buffers (both tx & rx) */

void serialFlush (const int fd)
{
  tcflush (fd, TCIOFLUSH) ;
}


/* serialClose: Release the serial port */

void serialClose (const int fd)
{
  close (fd) ;
}


/* serialPutchar: Send a single character to the serial port */

void serialPutchar (const int fd, const unsigned char c)
{
  write (fd, &c, 1) ;
}



/** serialGetchar: Get a single character from the serial device.
 * Note: Zero is a valid character and this function will time-out after 10 seconds.
 **********************************************************************************/

int serialGetchar (const int fd)
{
  uint8_t x ;

  if (read (fd, &x, 1) != 1)
    return -1 ;

  return ((int)x) & 0xFF ;
}


/* serialPutString: Send a whole message, char by char, using the
   existing serialPutchar(). Use this for protocol messages like
   "#CARD:12345678$" — pass the full string including the trailing $. */

void serialPutString (const int fd, const char *str)
{
  size_t i ;
  size_t len = strlen (str) ;

  for (i = 0 ; i < len ; i++)
    serialPutchar (fd, (unsigned char) str[i]) ;
}


/* serialGetString: Read characters one at a time (via serialGetchar())
   until the '$' terminator from the project protocol is seen, or the
   buffer fills, or a read times out.
   Returns the number of characters stored (not including the null
   terminator), or -1 on timeout/error before any '$' was received.
   buf must be at least buf_size bytes; the result is null-terminated. */

int serialGetString (const int fd, char *buf, int buf_size)
{
  int idx = 0 ;
  int c ;

  while (idx < buf_size - 1)
  {
    c = serialGetchar (fd) ;      /* -1 means timeout/no data */

    if (c == -1)
      return -1 ;

    if (c == '$')                 /* end-of-message marker, per protocol */
      break ;

    buf[idx++] = (char) c ;
  }

  buf[idx] = '\0' ;
  return idx ;
}
