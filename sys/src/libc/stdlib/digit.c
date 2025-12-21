int
char_to_digit(char c, int base)
{
	int digit;
	
	if (c >= '0' && c <= '9') {
		digit = c - '0';
	} else if (c >= 'a' && c <= 'z') {
		digit = c - 'a' + 10;
	} else if (c >= 'A' && c <= 'Z') {
		digit = c - 'A' + 10;
	} else {
		return -1;
	}
	
	return (digit < base) ? digit : -1;
}
