#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <windows.h>

#define FLASH_SIZE 512

HANDLE PORT;

void writebyte(unsigned char byte){
	int n;
	WriteFile(PORT,&byte,1,(void*)&n,NULL);
	if (n!=1)printf("write failed\n");
}

unsigned char readbyte(void){
	int n;
	char byte=0;
	ReadFile(PORT,&byte,1,(void*)&n,NULL);
	if (n!=1)printf("read timeout\n");
	return byte;
}

void rspv(void){
//wait for bootloader
	char buf;
	printf("waiting for arduino\n");
	while (1){
		writebyte(0x01);
		buf=readbyte();
		if (buf==0x02)
			break;
	}
	printf("ready\n");
}

unsigned char hex2byte(unsigned char h){
	unsigned char tb1[]="0123456789abcdef";
	unsigned char tb2[]="          ABCDEF";
	for (int i=0;i<16;i++){
		if (tb1[i]==h||tb2[i]==h)
			return i;
	}
	return 0;
}

unsigned short readword(unsigned short a){
	unsigned char hi,lo;
	unsigned short word;
	writebyte(0x06);
	writebyte(a>>8);
	writebyte(a&0xff);
	hi=readbyte();
	lo=readbyte();
	word=(hi<<8)|lo;
	return word;
}

void debug_flash_buffer(void){
	for (unsigned short i=0;i<0x205;i++){
		printf("%03x ",readword(i));
		if ((i+1)%12==0)
			printf("\n");
	}
}

void debug_pic_memory(void){
	printf("------------------\n");
	printf("PROGRAM MEMORY\n\n");	
	printf("CONFIG BITS: %03x\n",readword(0x3ff));
	printf("RESET VECTOR: %03x\n",readword(0x1ff));
	printf("CALIBRATION BITS: %03x\n",readword(0x204));	
	printf("------------------\n\n");
	writebyte(0x02);
	readbyte();
	debug_flash_buffer();
}

FILE *readfile(char *f){
	FILE *fp=fopen(f,"rb+");
	if (fp==NULL){
		int ok=0;
		char dir[MAX_PATH];
		char *ext=malloc(strlen(f)+1);
		if (ext!=NULL){
			ext[0]='\\';
			strcpy(ext+1,f);
			memset(dir,MAX_PATH,0);
			getcwd(dir,MAX_PATH);
			if (strlen(dir)+strlen(ext)<=MAX_PATH){
				strcat(dir,ext);
				fp=fopen(dir,"rb+");
				if (fp!=NULL)
					ok=1;
			}
			free(ext);
		}
		if (!ok)
			printf("invalid file\n");
	}
	return fp;
}



int main(int argc, char **argv){
	if (argc<4){
		printf("missing arguments\n");
		exit(0);
	}
	
	PORT=CreateFileA(argv[1],GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
	
	if (PORT==INVALID_HANDLE_VALUE){
		printf("failure to open port\n");
		exit(1);
	}
	
	DCB mode;
	COMMTIMEOUTS timeouts;
	
	memset(&mode,0,sizeof(DCB));
	mode.DCBlength=sizeof(DCB);
	
	timeouts.ReadIntervalTimeout=1;
	timeouts.ReadTotalTimeoutMultiplier=1000;
	timeouts.ReadTotalTimeoutConstant=1;
	timeouts.WriteTotalTimeoutMultiplier=1000;
	timeouts.WriteTotalTimeoutConstant=1;	
	
	GetCommState(PORT,&mode);
	
	if (!BuildCommDCBA("baud=2400 parity=n data=8 stop=1",&mode)||
			!SetCommState(PORT,&mode)||
			!SetCommTimeouts(PORT,&timeouts)){
		printf("port failure\n");
		CloseHandle(PORT);
		exit(1);
	}
	
	printf("port success\n");
	printf("baud %d parity %d stop %d data %d\n",mode.BaudRate,mode.fParity,mode.StopBits+1,mode.ByteSize);
	
	rspv();
	
	FILE *fp=NULL;
	
	if (!strcmp(argv[2],"w")){
		fp=readfile(argv[3]);
		if (fp!=NULL){
//clear internal buffer		
			writebyte(0x05);
			readbyte();
			int eof=0;
			unsigned char c;
			unsigned char data[32];
			while (!feof(fp) && !eof){
				c=fgetc(fp);
				if (c==':'){
					unsigned char ln,adr1,adr2,rd,cs;
					ln=(hex2byte(fgetc(fp))<<4)|hex2byte(fgetc(fp));
//upper/lower address bytes
					adr1=(hex2byte(fgetc(fp))<<4)|hex2byte(fgetc(fp));
					adr2=(hex2byte(fgetc(fp))<<4)|hex2byte(fgetc(fp));
					rd=(hex2byte(fgetc(fp))<<4)|hex2byte(fgetc(fp));
//todo add support for extended address spaces for other pics
					if (rd==0x01){
						eof=1;
					}else if(rd==0x00){
						unsigned char checksum=ln+adr1+adr2+rd;
						for (int i=0;i<ln;i++){
							c=(hex2byte(fgetc(fp))<<4)|hex2byte(fgetc(fp));
							data[i]=c;
							checksum+=c;
						}
						checksum=~checksum+1;
						cs=(hex2byte(fgetc(fp))<<4)|hex2byte(fgetc(fp));
						if (checksum!=cs){
							printf("checksum failed! terminating\n");
							break;
						}
						unsigned short adr=adr1<<8;
						adr|=adr2;
//divide address by 2 since its a byte address						
						adr=adr>>1;
/*
the arduino expects a 16 bit address plus the record length
and the data
	
0x02 byte tells arduino to copy pic memory into flash buffer
0x03 byte tells the arduino that a data record should be placed
inside the arduino's internal buffer, this buffer is copied into the pic
0x04 byte tells arduino to copy its internal buffer to the pic
0x05 byte clears the internal buffer
*/				
						writebyte(0x03);
						writebyte(adr>>8);
						writebyte(adr&0xff);
						writebyte(ln>>1);
						for (int i=0;i<ln;i++)
							writebyte(data[i]);
						readbyte();
					}
				}
			}
			printf("programming...\n");
			writebyte(0x04);
//the next call to readbyte() doesn't pop the data off the serial buffer
//for some reason, however a while loop works. I'll need to find out why
			while(readbyte());
			printf("done\n");
		}
	}else if(!strcmp(argv[2],"r")){
/*
//this will write the pic contents to a hex file or something later
		fp=readfile(argv[3]);
		if (fp!=NULL){
			//reading from PIC
			//todo
		}
*/
		debug_pic_memory();
	}else{
		printf("invalid command\n");
	}
	fclose(fp);	
	CloseHandle(PORT);
	return 0;
}