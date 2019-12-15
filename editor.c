/**** Includes *****/ 

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>

/****** defines ******/

#define CTRL_KEY(k) ((k) & 0x1f)  //Defining CTRL_KEY(k) as ctrl+ some character. Changing the meaning of the control charcters.

#define ABUF_INIT {NULL,0}  //works as constructor for the structure.

#define EDITOR_VERSION "0.0.1" //Editor version which is displayed in the welcome message.

/**** Data *****/

struct editorConfig{

struct termios orig_termios; //struct termios belong to the <termios.h> header file. contains fields which
			     //store flag bits controlling various attributes of the terminal.

int screenrows;
int screencols;

int cursorX;
int cursorY;


};

struct editorConfig E;


/**** Terminal ******/

void die(const char* s)
{
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);


	perror(s);
	exit(1);
}
                                           
void disableRawMode() {

	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios)==-1) //belong to <termios.h>
		die("tcsetattr");

	}


void enableRawMode() {

	if(tcgetattr(STDIN_FILENO, &E.orig_termios)==-1) //belong to <termios.h>
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


char editorReadKey()
{
		int nread;
		char c;
		while((nread=read(STDIN_FILENO, &c,1) != 1))
	       	{
			if(nread == -1 && errno != EAGAIN)
			{
				die("read");
			}

		}

		if(c=='\x1b')        //Condition for reading arrow keys for moving the cursor; Arrow keys are multiple bytes escape 
		{                    //sequence such as \x1b[? , where ? is replaced by A,B,C,D to associate with the arrow keys.
			char seq[3];

			if((nread=read(STDIN_FILENO, &seq[0], 1))!=1)
			{
				if(nread== -1 && errno != EAGAIN)
				{
					die("read");
				}
				else
				{
					return '\x1b';
				}
			}

			if((nread=read(STDIN_FILENO, &seq[1], 1)) !=1)
			{
				if(nread == -1 && errno != EAGAIN)
				{
					die("read");
				}
				else
				{
					return '\x1b';
				}
			}

			if(seq[0] == '[')
			{
				switch(seq[1])
				{
					case 'A': return 'w';
					case 'B': return 's';
					case 'C': return 'd';
					case 'D': return 'a';
					default: return '\x1b';
				}
			}
			else
			{
				return '\x1b';
			}

		}
		else
		{	
		return c;
		}
}


int getCursorPosition(int* rows,int* cols)       //Isn't working!! The escape sequence command \x1b[6N is not replying with the 
{						// cursor position, checked in bash in windows(WSL).
	char buf[32];
	unsigned int i=0;


	if(write(STDOUT_FILENO, "\x1b[6N",4)!=4)
	{
		return -1;
	}


	while(i < sizeof(buf) - 1)
	{
		if(read(STDIN_FILENO, &buf[i], 1) != 1)
		{
			break;
		}

		if(buf[i]== 'R')
		{
			break;
		}

		i++;
	}

	buf[i] = '\0';

	if(buf[0] != '\x1b' || buf[1] != '[')
	{
		return -1;
	}

	if(sscanf(&buf[2], "%d;%d", rows,cols) != 2)
	{
		return -1;
	}

	return 0;

}	


int getWindowSize(int* row, int* col)  //getting the size of the terminal in which current editor is being displayed.
{
	struct winsize ws;   //defined by <sys/ioctl.h>; structure having two fields, one row and other one is column

	if( ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)==-1 || ws.ws_col==0)  //ioctl() with TIOCGWINSZ returns the terminal size
	{
		if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) !=12)  //Moving the cursor to right bottom position, used in fall
		{							 //back method for finding the terminal size in which the 
			return -1;					 //editor is represented.
		}

		return getCursorPosition(row,col); //Not working
	}
	else
	{
		*row = ws.ws_row;
		*col= ws.ws_col;

		return 0;
	}
}

/****** Append buffer ******/

//Append buffer is created so as to perform write() only one time everytime it refreshScreen is done as multiple write() create a
//flickering effect due to delay between multiple write().

struct abuf     //created a structure for building a dynamic string which is not available by default in C.
{
	char* b;
	int len;
};



void abAppend(struct abuf *ab, const char* s, int len)  //function to append to the dynamic allocated string and form a new string
{
	char* new = realloc(ab->b,ab->len + len);
	
	if(new == NULL)
	{
		return ;
	}

	memcpy(&new[ab->len],s,len);
	ab->b=new;
	ab->len +=len;
}

void abFree(struct abuf *ab)  //Function to deallocate memory to the dynamically allocated string at the end of refresh.
{
	free(ab->b);	
}



/******* Output ******/

void editorDrawRows(struct abuf *ab)
{       
       	int y;       
       	for(y=0;y<E.screenrows-1;y++)       
       	{  
   		if(y==E.screenrows/3)
		{
			abAppend(ab,"~",1);

			char welcome[80];
			int welcomelen = snprintf(welcome, sizeof(welcome),"Editor --version %s\r\n", EDITOR_VERSION); 
			//snprintf() used to interpolate EDITOR_VERSION string to form the welcome string and hence form the welcome			   // message. 

			if(welcomelen > E.screencols)
			{
				welcomelen = E.screencols;
			}
			
			int padding = (E.screencols - welcomelen)/2;
			
			while(padding > 0) 
			{
				abAppend(ab," ",1);
				padding--;
			}

			abAppend(ab,welcome,welcomelen);

		}  else {

		abAppend(ab,"~\r\n",3);
		 
		}
  	}

	abAppend(ab,"~",1);
}

	

void editorRefreshScreen()   // To render the editor screen in the terminal.
{
	struct abuf ab=ABUF_INIT;
	
	abAppend(&ab,"\x1b[?25l",6); // \x1b[?25l escape sequence command is used to hide the cursor. 	
				     // Cursor is hidden before any drawing or clearing of screen is done so as to prevent any
				     // flickering which may happen due to movement of cursor to middle of screen during drawing.	

	abAppend(&ab,"\x1b[2J",4);  // \x1b[2J is a escape sequence command which is ANSI defined and clears the whole screen
	abAppend(&ab,"\x1b[H",3); // \x1b[H is a espace sequence command which is ANSI defined and repositions the cursor to                                          //the defined position mentioned in the command parameter.

	editorDrawRows(&ab);

	char buf[32];

       int length=snprintf(buf,sizeof(buf),"\x1b[%d;%dH",E.cursorY+1,E.cursorX+1);
	
	abAppend(&ab,buf,length);
	abAppend(&ab, "\x1b[?25h",6); // \x1b[?25h escape sequence command is used to show the cursor which was hidden.

	write(STDOUT_FILENO,ab.b,ab.len);
	abFree(&ab);
}



/***** Input *******/

void editorMoveCursor(char key)
{
	switch(key)
	{
		case 'w': E.cursorY--;
			  break;
		
		case 's':E.cursorY++;
			 break;

		case 'a':E.cursorX--;
			 break;

		case 'd':E.cursorX++;
			 break;

	}
}



void editorProcessKeypress() {
	char c= editorReadKey();

	switch(c)
	{
		case CTRL_KEY('q'):
		write(STDOUT_FILENO, "\x1b[2J", 4);               
	       	write(STDOUT_FILENO, "\x1b[H", 3);	       

		exit(0);
		break;

		case 'w':
		case 's':
		case 'a':
		case 'd':
		editorMoveCursor(c);
		break;		
	}

	}

		


/******* Init *******/

void initEditor()
{
	
	if( getWindowSize(&E.screenrows, &E.screencols)==-1)
	{
		die("getWindowSize");
	}
}






int main()
{
	enableRawMode();
	E.cursorX=0;
	E.cursorY=0;	

	while(1)
	{
		initEditor();	
		editorRefreshScreen();
		editorProcessKeypress();
	}
	
	return 0;
}


