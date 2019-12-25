/**** Includes *****/ 

#define _DEFAULT_SOURCE  //feature test macro
#define _BSD_SOURCE    //feature test macro
#define _GNU_SOURCE   //feature test macro

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/types.h>

/****** defines ******/

#define CTRL_KEY(k) ((k) & 0x1f)  //Defining CTRL_KEY(k) as ctrl+ some character. Changing the meaning of the control charcters.

#define ABUF_INIT {NULL,0}  //works as constructor for the structure.

#define EDITOR_VERSION "0.0.1" //Editor version which is displayed in the welcome message.

#define EDITOR_TAB_STOP 8  //the maximum number of spaces a tab keypress can have.

enum editorKey {          //Used to map arrow keys to movement of cursor function and preventing it from clashing with other charch
						//ter inputs.
		Arrow_left=1000,
		Arrow_right,
		Arrow_up ,
		Arrow_down,
		Page_up,
		Page_down,
		Home,
		End,
		Del

	       };

/**** Data *****/

typedef struct erow{                //structure defined to store the characters to be printed on a line in the editor.
			int size;
			char* chars;

			char* render;   //Copies the content of chars and replaces each tab with appropiate number of spaces.

			int rsize;     //Stores the siz eof render string.
			
	       	   } erow;


struct editorConfig{

struct termios orig_termios; //struct termios belong to the <termios.h> header file. contains fields which
			     //store flag bits controlling various attributes of the terminal.

int screenrows;
int screencols;

int cursorX;
int cursorY;

int numrows;   //counts the number of rows of text the file has
erow *row;      //structure which acts as a dynamic array to store a string which is a single line.

int fileopen_flag;    //flag to indicate whether file is opened or not.

int rowoff;     //Stores the row number of the file from which the text should be displayed.

int coloff;    //Stores the column number of the file from which the text should be displayed.

int renderX;   //Stores the horizontal position of the cursor in the render string

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


int editorReadKey()
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
				if(seq[1]>= '0' && seq[1] <= '9')
				{
					if((nread=read(STDIN_FILENO, &seq[2], 1)) != 1)
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

					if(seq[2]=='~')
					{
						switch(seq[1])
						{
							case '5': return Page_up;
								  

							case '6': return Page_down;
								  
							
							case '1': 
							case '7': return Home;

							case '4':
							case '8': return End;
							
							case '3': return Del;	  
						}
					}
					else
					{
						return '\x1b';
					}


				}
				else
				{
					switch(seq[1])
					{
						case 'A': return Arrow_up;
						case 'B': return Arrow_down;
						case 'C': return Arrow_right;
						case 'D': return Arrow_left;
						case 'H': return Home;
						case 'F': return End;
						default: return '\x1b';
					}
				}
			}
			else
			{
				if(seq[0] == 'O')
				{
					switch(seq[1])
					{
						case 'H': return Home;
						case 'F' : return End;
						default: return '\x1b';
					}
				}
				else
				{
					return '\x1b';
				}
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

/******** Row Operations *********/


/*
int editorRowCxtoRx(erow* row, int cx)
{
	int rX =0;
	int j;

	for(j=0;j < cx;j++)
	{
		if(row->chars[j] == '\t')
		{
			rX += (EDITOR_TAB_STOP - 1) - (rX % EDITOR_TAB_STOP);
		}

		rX++;
	}

	return rX;
}

*/


void editorUpdateRow(erow *row)    //function to parse chars and replace tabs with spaces and store them in render string.
{
	int tab =0;
	int j;

	for(j=0;j<(row->size);j++)
	{
		if(row->chars[j] == '\t')
		{
			tab++;
		}
	}


	
	free(row->render);
	row->render = (char*) malloc(row->size+ tab * ((EDITOR_TAB_STOP)-1) + 1);

	int idx=0;

	for(j=0;j<row->size;j++)
	{
		if(row->chars[j] != '\t')
		{
			row->render[idx++] = row->chars[j];
		}
		else
		{
		//	for(int i=0;i<8;i++)
		
			row->render[idx++] = ' ';

		

	         	while(idx % EDITOR_TAB_STOP != 0)
			{
				row->render[idx++] = ' ';
			}
		
		}
	}

	row->render[idx] = '\0';

	row->rsize = idx;
}




void editorAppendRow(char *s, size_t len)
{
		
		E.row = realloc(E.row,sizeof(erow) * (E.numrows+1));
		
		int at=E.numrows;
	        E.row[at].size = len;       
	       	E.row[at].chars = (char*) malloc(len+1);       
	       	memcpy(E.row[at].chars,s,len+1);       
	       	E.row[at].chars[len] = '\0';

	   	E.row[at].render = NULL;
		E.row[at].rsize = 0;	
		
		editorUpdateRow(&E.row[at]);

   		E.numrows++;
}






/******** File I/O *********/

void editorOpen(char* filename)                //function to open the text file which is to be edited in the text editor.
{
	FILE *fp = fopen(filename, "r");
	if(!fp)
	{
		die("fopen");
	}
	else
	{
		E.fileopen_flag = 0;
	}

	
	char* line= NULL;
	size_t linecap=0;
	ssize_t linelen;

	while(1)
	{
		line= NULL;
		linecap=0;

		linelen = getline(&line,&linecap,fp);

		if(linelen != -1)
	 	{
			while(linelen >	0 && (line[linelen-1] == '\n' || line[linelen-1] == '\r'))
			{
				linelen--;
			}
		
		editorAppendRow(line,linelen);
		free(line);

		}
		else
		{

			fclose(fp);
			break;
		}
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

void editorScroll()
{

	//E.renderX = 0;

	/*if(E.cursorY < E.numrows)
	{
		E.renderX = editorRowCxtoRx(&E.row[E.cursorY],E.cursorX);
	}
	*/

	if(E.cursorY < E.rowoff)     //For scrolling during Arrow_up keypress.(E.cursorY represents the position of the cursor in
	{			     // file not in the terminal).
		E.rowoff = E.cursorY;
	}

	if(E.cursorY >= (E.rowoff + E.screenrows))  //For scrolling the file beyond the visible region during Arrow_down keypress.
	{
		E.rowoff = E.cursorY -E.screenrows + 1;
	}

	if(E.renderX >= (E.coloff + E.screencols))
	{
		E.coloff = E.renderX - E.screencols + 1;
	}

	if(E.renderX < E.coloff)
	{
		E.coloff = E.renderX;
	}
}




void editorDrawRows(struct abuf *ab)
{       
       	int y;  
   	int filerow;	
       	for(y=0;y<E.screenrows-1;y++)       
       	{ 
		filerow = y+E.rowoff;
	    if(filerow>=E.numrows)
	    {	    
   		if(E.fileopen_flag && E.numrows==0 && y==E.screenrows/3)
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
	    else
	    {
		 
		int len = E.row[filerow].rsize - E.coloff;

		if(len < 0)
		{
			len = 0;
		}

		if(E.screencols<len)
		{
			len = E.screencols;
		}

		abAppend(ab,&E.row[filerow].render[E.coloff],len);
		abAppend(ab,"\r\n",2);
	    }
    
	 }
	
	filerow=y+E.rowoff;

	if(filerow>=E.numrows)
	{
		abAppend(ab,"~",1);
	}
	else
	{
		int len = E.row[filerow].rsize - E.coloff;
		if(len < 0)
		{
			len = 0;
		}

		if(E.screencols < len)
		{
			len = E.screencols;
		}

		abAppend(ab,&E.row[filerow].render[E.coloff],E.row[filerow].rsize);
	}
}

	

void editorRefreshScreen()   // To render the editor screen in the terminal.
{
	editorScroll();

	struct abuf ab=ABUF_INIT;
	
	abAppend(&ab,"\x1b[?25l",6); // \x1b[?25l escape sequence command is used to hide the cursor. 	
				     // Cursor is hidden before any drawing or clearing of screen is done so as to prevent any
				     // flickering which may happen due to movement of cursor to middle of screen during drawing.	

	abAppend(&ab,"\x1b[2J",4);  // \x1b[2J is a escape sequence command which is ANSI defined and clears the whole screen
	abAppend(&ab,"\x1b[H",3); // \x1b[H is a espace sequence command which is ANSI defined and repositions the cursor to                                          //the defined position mentioned in the command parameter.

	editorDrawRows(&ab);

	char buf[32];
	/*
	if(E.cursorX > (E.row[E.cursorY].size-1))
	{
		E.cursorX = E.row[E.cursorY].size-1;
	}
	*/
       int length=snprintf(buf,sizeof(buf),"\x1b[%d;%dH",(E.cursorY-E.rowoff)+1,(E.renderX-E.coloff)+1); 

	//E.cursorY-E.rowoff+1 actually set the position of the cursor before displaying the cursor in the visible region of the 
	//terminal eventhough E.cursorY actually represents the position of the cursor in the file, not int the terminal.

	abAppend(&ab,buf,length);
	abAppend(&ab, "\x1b[?25h",6); // \x1b[?25h escape sequence command is used to show the cursor which was hidden.

	write(STDOUT_FILENO,ab.b,ab.len);
	abFree(&ab);
}



/***** Input *******/

void editorMoveCursor(int key)
{
	switch(key)
	{
		case Arrow_up:
			{
		      		 if(E.cursorY!=0)
		       		{
						E.cursorY--;
		       		}

				if(E.renderX > (E.row[E.cursorY].rsize-1))
				{
					E.coloff = E.row[E.cursorY].rsize - E.screencols + 1;
					if(E.coloff < 0)
					{
						E.coloff = 0;
					}

					E.renderX = E.row[E.cursorY].rsize;
				}	
			}

			 break;
		
		case Arrow_down:
			 {
				
				if(E.cursorY <E.numrows-1)
				{
					E.cursorY++;
				}

				if(E.cursorX > (E.row[E.cursorY].rsize-1))
				{
					E.coloff = E.row[E.cursorY].rsize - E.screencols + 1;
					if(E.coloff < 0)
					{
						E.coloff = 0;
					}

					E.renderX = E.row[E.cursorY].rsize;
					
				}
			 }
			 break;

		case Arrow_left:
			if(E.renderX!=0)
			{
		 		E.renderX--;
			}
			else
			{
				if(E.cursorY > 0)
				{
					E.cursorY--;
					E.renderX = E.row[E.cursorY].rsize;
				}
			}

			 break;

		case Arrow_right:
			 {
			 	if(E.renderX < E.row[E.cursorY].rsize)
			 	{
			 		E.renderX++;
				}
				else
				{
					if(E.cursorY < E.numrows-1)
					{
						E.cursorY++;

						E.renderX = 0;
					}
				}
			
			 }
			 break;

	}

/*	erow *row = (E.cursorY >= E.numrows) ? NULL : &E.row[E.cursorY];

	int rowlen = row ? row ->size : 0;

	if(E.renderX > rowlen)
	{
		E.renderX = rowlen;
	}
	*/

}



void editorProcessKeypress() {
	int c= editorReadKey();

	switch(c)
	{
		case CTRL_KEY('q'):
		write(STDOUT_FILENO, "\x1b[2J", 4);               
	       	write(STDOUT_FILENO, "\x1b[H", 3);	       

		exit(0);
		break;

		case Arrow_up:
		case Arrow_down:
		case Arrow_left:
		case Arrow_right:
		editorMoveCursor(c);
		break;	

		case Page_up:
		case Page_down:	
		{
				if(c == Page_up)
				{
					E.cursorY = E.rowoff;
					
					if(E.renderX > (E.row[E.cursorY].rsize - 1))
					{
						E.coloff = E.row[E.cursorY].rsize - E.screencols + 1;
						
						if(E.coloff < 0)
						{
							E.coloff = 0;
						
						}

						E.renderX = E.row[E.cursorY].rsize;
					}


					
				}
				else
				{
					if(c == Page_down)
					{
						E.cursorY = E.rowoff + (E.screenrows - 1);

						if(E.cursorY > (E.numrows-1))
						{
							E.cursorY = E.numrows-1;
						}
	
						if(E.renderX > (E.row[E.cursorY].rsize -1))
						{
							E.coloff = E.row[E.cursorY].rsize - E.screencols + 1;
							
							if(E.coloff < 0)
							{
								E.coloff =0 ;
							}

							E.renderX = E.row[E.cursorY].rsize;
						}

					}	

				}
		}		
	
		break;
		
		case End:     //Keypress for moving the cursor to end of line.
		case Home:    //Keypress for moving the cursor to start of line.
		{
			if(c == End)
			{
				E.coloff = E.row[E.cursorY].rsize - E.screencols + 1;

				if(E.coloff < 0)
				{
					E.coloff = 0;
				}

				E.renderX= E.row[E.cursorY].rsize;
			}
			else
			{
				if(c == Home)
				{
					E.coloff = 0;

					E.renderX = 0;
				}
			}
		}

		break;

		case Del:
		
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






int main(int argc, char *argv[])
{
	enableRawMode();
	E.cursorX=0;
	E.cursorY=0;
	E.renderX=0;	
	E.numrows=0;
	E.fileopen_flag= 1;
	E.rowoff=0;
	E.coloff=0;
	E.row = NULL;
	if(argc >=2)
	{
		editorOpen(argv[1]);
	}

	while(1)
	{
		initEditor();	
		editorRefreshScreen();
		editorProcessKeypress();
	}
	
	return 0;
}


