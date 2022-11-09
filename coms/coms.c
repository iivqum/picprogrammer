#include <stdio.h>
#include <windows.h>

HANDLE PORT;

void writebyte(unsigned char byte){
	int n;
	WriteFile(PORT,&byte,1,(void*)&n,NULL);
	if (n!=1)printf("write failed\n");
}

void readbyte(unsigned char *byte){
	int n;
	ReadFile(PORT,byte,1,(void*)&n,NULL);
	//if (n!=1)printf("read failure or timeout\n");
}

void querydevice(){
//wait for bootloader
	char buf;
	printf("waiting for arduino\n");
	while (1){
		writebyte(0x01);
		readbyte(&buf);
		if (buf==0x02)
			break;
	}
	printf("ready\n");
}

int main(int argc, char **argv){
	PORT=CreateFileA(argv[1],GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
	
	if (PORT==INVALID_HANDLE_VALUE){
		printf("failure to open port\n");
		exit(1);
	}
	
	DCB mode;
	
	memset(&mode,0,sizeof(DCB));
	mode.DCBlength=sizeof(DCB);
	
	COMMTIMEOUTS timeouts;
	
    timeouts.ReadIntervalTimeout=1;
    timeouts.ReadTotalTimeoutMultiplier=1000;
    timeouts.ReadTotalTimeoutConstant=1;
    timeouts.WriteTotalTimeoutMultiplier=1000;
    timeouts.WriteTotalTimeoutConstant=1;	
	
	GetCommState(PORT,&mode);
	
//	mode.fDsrSensitivity=FALSE;
//	mode.fRtsControl=RTS_CONTROL_TOGGLE;
//	mode.fDtrControl=DTR_CONTROL_HANDSHAKE;
//	mode.fOutX=TRUE;
	
	if (!BuildCommDCBA("baud=2400 parity=n data=8 stop=1",&mode)||
			!SetCommState(PORT,&mode)||
			!SetCommTimeouts(PORT,&timeouts)){
		printf("port failure\n");
		CloseHandle(PORT);
		exit(1);
	}
	
	printf("port success\n");
	printf("baud %d parity %d stop %d data %d\n",mode.BaudRate,mode.fParity,mode.StopBits+1,mode.ByteSize);
	
	querydevice();
	
	unsigned char buf=0x00;
	
	while (1){
		readbyte(&buf);
		switch (buf){
			case 0x01: printf("entered programming mode\n"); break;
			case 0x02:
				char buf2[2];
				readbyte(buf2);
				readbyte(buf2+1);
				printf("%d\n",*(unsigned short *)buf2);
				break;
			case 0x03: printf("programming...\n"); break;
			case 0x04: printf("done\n"); break;
		}
		buf=0x00;
	}
		
	CloseHandle(PORT);
	return 0;
}