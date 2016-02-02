/********************************************************************
	created:	2006/06/30
	filename: 	Main.cpp
	author:		李创
                http://www.cppblog.com/converse/

	purpose:	测试Factory模式的代码
*********************************************************************/

#include "Factory.h"
#include <stdlib.h>
	
#include <stdio.h>
#include <unistd.h>
#include <termios.h>


int main(int argc,char* argv[])
{
	Creator *p = new ConcreateCreator();
	p->AnOperation();

	delete p;

	//system("pause");
	printf("Press any key to continue") ;
	struct termios te;
	int ch;
	tcgetattr( STDIN_FILENO, &te);
	te.c_lflag &= ~( ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &te);
	tcflush(STDIN_FILENO, TCIFLUSH);
	fgetc(stdin) ; 
	te.c_lflag |= ( ICANON|ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &te);

	return 0;
}


