#include <stdio.h>

#include "adele/axpike_iface.h"

int main(void)
{
    float a = 1.0f;
    float b = 2.0f;

    axpike_activate(AXPIKE_LOWPRECISIONE5M2);
    float c = a + b;
    axpike_deactivate(AXPIKE_LOWPRECISIONE5M2);

    printf("%f\n", c);
    return 0;
}
