#include <stdio.h>
#include "foo.c"

struct s {
    int a;
    int b;
};

static struct s g_sa;
static int x;

int main(int argc, char **argv)
{
    int a;
    int b;
	int *p_a;
	float *p_f_a;
    int c = 2;
    int d = 4, e;
    float f = 1;
    struct s sa = { 1, 2};
    struct s sb;
	struct s *p_s_a;

	if (1) {
		int block_a;
	}
	for (int i; i != i; i++) {
	}
    return 0;
}
