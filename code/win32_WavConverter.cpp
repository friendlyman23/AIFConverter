#include <windows.h>
#include <stdio.h>
#include <string.h>
/*#include "converter.h"*/
/*#include "converter.cpp"*/

int main(int argc, char *argv[])
{
    if(argc != 2)
    {
	printf("Error: Usage\n");	
	exit(1);
    }

    char *Wav_Filename = argv[1];
    char *Dot = strrchr(Wav_Filename, '.');
    char *Filetype = Dot + 1;
    char *ToMatch = "wav";

    for(int i = 0; i < (strlen(ToMatch)); i++)
    {
	if(tolower(*Filetype) != *ToMatch)
	{
	    printf("Error: File extension\n");
	    exit(1);
	}
	Filetype++;
	ToMatch++;
    }

    return(0);
}
