/* stdlib_test.c - Simple test for stdlib.h */

#include <stdlib.h>
#include <string.h>
#include <syscall.h>

void print(const char *s) {
	write(1, s, strlen(s));
}

void print_num(int n) {
	char buf[20];
	int i = 0;
	int neg = 0;
	
	if (n < 0) {
		neg = 1;
		n = -n;
	}
	
	if (n == 0) {
		buf[i++] = '0';
	} else {
		while (n > 0) {
			buf[i++] = '0' + (n % 10);
			n /= 10;
		}
	}
	
	if (neg) buf[i++] = '-';
	
	while (i > 0) {
		write(1, &buf[--i], 1);
	}
}

void cleanup_handler(void) {
	print("Cleanup handler called!\n");
}

int int_compare(const void *a, const void *b) {
	return (*(int*)a - *(int*)b);
}

int main(void) {
	print("=== stdlib.h Test Program ===\n\n");
	
	/* Test atoi */
	print("Testing atoi:\n");
	int num = atoi("42");
	print("  atoi(\"42\") = ");
	print_num(num);
	print("\n");
	
	num = atoi("-123");
	print("  atoi(\"-123\") = ");
	print_num(num);
	print("\n\n");
	
	/* Test strtol with hex */
	print("Testing strtol with hex:\n");
	long hex = strtol("0xFF", NULL, 16);
	print("  strtol(\"0xFF\", 16) = ");
	print_num((int)hex);
	print("\n\n");
	
	/* Test abs */
	print("Testing abs:\n");
	print("  abs(-42) = ");
	print_num(abs(-42));
	print("\n\n");
	
	/* Test div */
	print("Testing div:\n");
	div_t d = div(17, 5);
	print("  div(17, 5) -> quot = ");
	print_num(d.quot);
	print(", rem = ");
	print_num(d.rem);
	print("\n\n");
	
	/* Test rand */
	print("Testing rand:\n");
	srand(42);
	print("  rand() = ");
	print_num(rand());
	print("\n  rand() = ");
	print_num(rand());
	print("\n\n");
	
	/* Test qsort */
	print("Testing qsort:\n");
	int arr[] = {5, 2, 8, 1, 9};
	int n = 5;
	
	print("  Before: ");
	for (int i = 0; i < n; i++) {
		print_num(arr[i]);
		print(" ");
	}
	print("\n");
	
	qsort(arr, n, sizeof(int), int_compare);
	
	print("  After:  ");
	for (int i = 0; i < n; i++) {
		print_num(arr[i]);
		print(" ");
	}
	print("\n\n");
	
	/* Test atexit */
	print("Testing atexit:\n");
	atexit(cleanup_handler);
	print("  Registered cleanup handler\n\n");
	
	print("=== All tests complete ===\n");
	print("Now exiting (watch for cleanup handler)...\n");
	
	return 0;
}