/**** Includes *****/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

/****** defines ******/

#define CTRL_KEY(k) ((k) & 0x1f)

/**** Data *****/

struct termios orig_termios; //struct termios belong to the <termios.h> header file. contains fields which
			     //store flag bits controlling various attributes of the terminal.

/**** Terminal ******/

void die(const char* s)
{
	perror(s);
	exit(1);
}

void disableRawMode() {

	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios)==-1) //belong to <termios.h>
		die("tcsetattr");

	}


void enableRawMode() {

	if(tcgetattr(STDIN_FILENO, &orig_termios)==-1) //belong to <termios.h>
		die("tcgetattr");

	atexit(disableRawMode); //belong to <stdio.h> : tells the program to call disableRawMode() whenever				   // the program exits.

	struct termios raw;
 
 	if(tcgetattr(STDIN_FILENO, &raw)==-1)
	{
		die("tcgetattr");
	}

	raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);//To change the flag in terminal controlling the ctrl-s and ctr-q  //so as to print only the ascii values when such control character are typed. 
			
	raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN); //To change the flags controlling various attrinbutes of						   //terminal, thus changing the terminal to raw mode,turning off echo, and disabling signal  controls				       // such as ctrl-c a ctrl-z.

	raw.c_oflag &= ~(OPOST); //To turn off any output processing in terminal

	raw.c_cflag |= (CS8);
             
	raw.c_cc[VMIN]=	0;  //VMIN is a index of c_cc byte array,(cc- control character), c_cc[VMIN] decides how many minimum charc-
			    //ters must read() must take before returning. 
				
	raw.c_cc[VTIME]= 1; //VTIME is a index of c_cc byte array, c_cc[VTIME] decides time read() has to wait before returning

	//the above VMIN and VTIME doesn't work for WSL (bash in windows).

	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)==-1)
	{
		die("tcsetattr");
	}
	
	}	


/******* Init *******/
	       

int main()
{
	enableRawMode();
		
	while(1)
	{	
		
		char c='\0';
		if(read(STDIN_FILENO, &c, 1)==-1 && errno!= EAGAIN)
		{
			die("read");
		}

		if(iscntrl(c)) {
			printf("%d\r\n",c);
		}
		else {
			printf("%d ('%c')\r\n",c,c);
		}

		if(c== CTRL_KEY('q'))
			break;
	}
	
	return 0;
}

