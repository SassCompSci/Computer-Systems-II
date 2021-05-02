// mypager.c

/* mypager utility
 * Prints a file to standard output, one page worth of lines
 * at a time. It is designed for text files because it prints
 * each byte to the screen as an ASCII character.
 * The user controls the output by pressing keys, as follows:
 * 'f': forwards to the next page
 * 'q': quits
 * NOTE: Each keypress is read immediately; the user does not
 * press the Enter key. To learn how immediate input mode is
 * effectuated, see the eliminate_stdio_buffering() function,
 * below, or see 'man termios'.
 */
 
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>

// preprocessor definitions
#define PAGE_SIZE 20
#define LINE_WIDTH 80
#define BUFFER_SIZE ((LINE_WIDTH + 1) * PAGE_SIZE)

// forward declarations
int display_page();
int fetch_next_line( char line[] );
int fetch_next_word( char word[], int max_size );
int refill_buffer( int start );

void eliminate_stdio_buffering();
void restore_stdio_buffering();

// global variables
int fd;
char buffer[BUFFER_SIZE];
int buffer_start, buffer_end;
int buffer_has_data;
int current_page_size;
struct termios old, new;

int usage( char * argv[] )
{
	fprintf( stderr, "Usage:\n" );
	fprintf( stderr, "%s <filename>\n", argv[0] );
	return 1;
}

int display_page()
{
	// read characters from the starting byte of the buffer
	// until the page size is reached; page size is denoted
	// in number of lines
	int number_of_chars;
	int number_of_lines;
	char line[LINE_WIDTH];
	int i;
	for( number_of_lines= 0, current_page_size= 0;
	 number_of_lines < PAGE_SIZE; number_of_lines++ )
	{
		number_of_chars= fetch_next_line( line );
		if ( number_of_chars > 0 )
		{
			// print chars
			fwrite( line, sizeof(char), number_of_chars, stdout );
			current_page_size+= number_of_chars;
			// print '\n' if not included in line
			if ( line[number_of_chars-1] != '\n' )
				printf( "\n" );
		}
		else // EOF or error
		{
			if ( number_of_chars == 0 )
				printf( "=== EOF ===\n" );
			else
				printf( "(error reading file)\n" );
			return 0;
		}
	}
	return 1;
}

int fetch_next_line( char line[] )
{
	// break each line at either:
	//  a) LF char
	//  b) last space that is <= LINE_WIDTH + 1
	int count= 0;
	int number_of_chars;
	char word[LINE_WIDTH];
	int more= 1;
	
	while( more )
	{
		number_of_chars= fetch_next_word( word, LINE_WIDTH - count );
		if ( number_of_chars > 0 ) // word found
		{
			memcpy( line + count, word, number_of_chars );
			count+= number_of_chars;
			if ( word[number_of_chars-1] == '\n' || count >= LINE_WIDTH )
				more= 0;
		}
		else if ( number_of_chars == 0 /* EOF */ || 
		 number_of_chars == -2 /* End of Line */ ) 
			more= 0;
		else // -1, indicating file error
			return -1;
	}
	return count;
}

int fetch_next_word( char word[], int max_size )
{
	int start= buffer_start;
	int count= 0;
	int more= 1;
	int refill_return;
		
	// read bytes from buffer until next word is found
	// if the size of the word, plus trailing whitespace char,
	// is <= (max_size + 1)
	//    copy the word to the buffer and return the number of chars
	// else
	//    return an error code
	// refill buffer if end of buffer is encountered while determining
	// the next word
	
	while( more && count < (max_size + 1) )
	{
		// if the end of the buffer is reached,
		// move the contents to the beginning of the buffer
		// and refill the remainder of the buffer
		if ( start + count > buffer_end )
		{			
			if ( count ) // move contents
				memcpy( buffer, buffer + buffer_start, count );			
			refill_return= refill_buffer( count );
			start= buffer_start;
			if ( refill_return == 0 ) // EOF
			{
				if ( count ) // final word!
				{
					more= 0;
					break;
				}
				else // buffer is empty and EOF
					return 0;
			}
			else if ( refill_return == -1 ) // error
				return -1;
			
		}
		
		switch( buffer[start+count] )
		{
			// return when whitespace encountered
			case ' ':
			case '\n':
			case '\t':
				more= 0; // fall through
			default: // do not return just for '\r'
				count++;
		}		
	}
	if ( more ) // uh-oh, count exceeds max_size
		return -2;
	
	// copy the bytes into the word buffer
	memcpy( word, buffer + start, count );
	buffer_start+= count;
	
	return count;
}

int refill_buffer( int start )
{
	// reset data
	buffer_start= 0;
	buffer_end= start - 1;
	// read one buffer worth of data
	int number_of_bytes= read( fd, buffer + start, BUFFER_SIZE - start );
	if ( number_of_bytes > 0 )
	{
		buffer_has_data= 1;
		buffer_end+= number_of_bytes;
	}
	else
		buffer_has_data= 0;
	return number_of_bytes;
}

int main( int argc, char * argv[] )
{	
	// get the first command line argument
	// open the file
	// wait for commands--
	//  f   forward (next page)
	//  q   quit
	
	if ( argc != 2 )
		return usage( argv );
		
	printf( "Opening file %s...\n", argv[1] );
	fd= open( argv[1], O_RDONLY );
	if ( fd == -1 )
	{
		perror( "open() failed" );
		return 1;
	}
	
	refill_buffer( 0 );
	
	// set up the terminal to eliminate buffering for stdio
	eliminate_stdio_buffering();

	// read the default number of lines and output them to standard output
	char command= 'f'; // triggers display of first page
	do
	{
		switch( command )
		{
		case 'f':
			display_page();
			break;
		case 'q':
			// clean up the screen
			break;
		default:
			break;
		}
		command= (char) getchar();
	} while ( command != 'q' );
	
	close( fd );
	
	restore_stdio_buffering();
}

void eliminate_stdio_buffering()
{
	tcgetattr( 0, &old );
	new= old;
	new.c_lflag&= ~ICANON; // disable canonical mode
	new.c_lflag&= ~ECHO; // disable input echo
	tcsetattr( 0, TCSANOW, &new );
}

void restore_stdio_buffering()
{
	tcsetattr( 0, TCSANOW, &old );
}
