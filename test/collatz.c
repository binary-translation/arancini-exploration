int collatz(int n)
{
	int c = 0;
	do {
		n = ((n % 2) == 0) ? (n / 2) : ((n * 3) + 1);
		c++;
	} while (n > 1);

	return c;
}
