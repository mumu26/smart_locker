#include <stdio.h>

int main()
{
	unsigned char cmd[6]={0xAA, 4, 2, 4, 0, 0};
	unsigned int checksum = 0;
	unsigned char i;
	for (i = 0; i < 6; i++)
		checksum += (unsigned int)cmd[i];
	checksum = checksum & 0xFF;
	printf("checksum is 0x%x\n", checksum);
	printf("cmd in HEX is -->\n");
	for (i = 0; i < 6; i++)
		printf("%02x", cmd[i]);
	printf("%02x\n", checksum);
	return 0;
}
