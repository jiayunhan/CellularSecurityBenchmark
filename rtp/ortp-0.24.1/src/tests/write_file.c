#include <stdio.h>
#include <stdlib.h>
#include "ortp/ortp.h"
#include "ortp/rtpsession.h"
#include "ortp/rtcp.h"
int main(){
	
	FILE* fp = fopen("audio.wav","w+");
	int i=0,j=1,k=0;
	char data[1000000];
	while(i<1000000){
		while(j<100){
		     while(k<80){
		     	 int first = j/10;
		     	 int last = j%10; 
				data[i]=first+'0';
								if(i>=1000000)
					break;
				i++;
				if(i>=1000000)
					break;
				data[i]=last+'0';
				i++;

				k++;
			}
			k=0;
			j++;
			}
		j=0;
	}
	fwrite(data,1,1000000,fp);
	fclose(fp);

}

		
