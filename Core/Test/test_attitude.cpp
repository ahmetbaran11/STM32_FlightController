#include "AttitudeEstimator.h"
#include <cstdio>
#include <cmath>

int failures = 0;

void check(const char* name, float actual, float expected, float tolerance) {
    bool ok = fabsf(actual - expected) <= tolerance;
    printf("[%s] %-45s beklenen %8.2f, gelen %8.2f\n",
           ok ? "PASS" : "FAIL", name, expected, actual);
    if (!ok) failures++;
}

int main() {
    // --- Senaryo 1: kart duz duruyor ---
    AttitudeEstimator att(0.004f, 1.0f, 0.1f);
	ImuSample s{0.0f, 0.0f, 0.0f, 0.0f, 200.0f, 0.0f, 0, false, false};

    att.init(s);

    Attitude a;
    int sayac = 0;
    for (int i = 0; i < 10000; i++) {
    	a = att.update(s);
    	if (fabsf(a.pitch) > 90.001f) sayac++;
    }

    check("ihlal sayisi", float(sayac), 0.0f, 0.5f);
    check("duz duruyor - roll",  a.roll,  0.0f, 0.5f);
    check("22 tur sonrasi pitch", a.pitch, 80.0f, 0.5f);
	// buraya senin kodun

    printf("\n%d test basarisiz\n", failures);
    return failures;
}
