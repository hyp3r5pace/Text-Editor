#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

struct termios orig_termios; //struct termios belong to the <termios.h> header file. contains fields which
			     //store flag bits controlling various attributes of the terminal.

void disableRawMode() {

	tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); //belong to <termios.h>

	}


void enableRawMode() {

	tcgetattr(STDIN_FILENO, &orig_termios); //belong to <termios.h>
	atexit(disableRawMode); //belong to <stdio.h> : tells the program to call disableRawMode() whenever				   // the program exits.

	struct termios raw;
 
 	tcgetattr(STDIN_FILENO, &raw);

	raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);   //To change the flag in terminal controlling the ctrl-s and ctrl-q so
       				  //as to print only the ascii values when such control characters are
				  //typed. 
			
	raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN); //To change the flags controlling various attrinbutes of						   //terminal, thus changing the terminal to raw mode,turning off echo, and disabling signal  controls				       // such as ctrl-c a ctrl-z.

	raw.c_oflag &= ~(OPOST); //To turn off any output processing in terminal

	raw.c_cflag |= (CS8);


	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
	
	}	
	       

int main()
{
	enableRawMode();
	
	char c;
	
	while(read(STDIN_FILENO, &c, 1) == 1 && c!='q')
	{
		if(iscntrl(c)) {
			printf("%d\r\n",c);
		}
		else {
			printf("%d ('%c')\r\n",c,c);
		}
	}
	
	return 0;
}

