#include <stdio.h>

int main() {

	char oldname[10] = "a.txt";
	char newname[10] = "b.txt";
	int ret;

	fork();
	ret = rename(oldname, newname);

	printf("return value : ");
	return 0;
}

