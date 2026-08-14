#pragma once
#include "ImuTypes.h"

class AttitudeEstimator {
public:
	AttitudeEstimator(float dt, float kp, float ki);
	void init(const ImuSample& s);
	Attitude update(const ImuSample& s);
private:
	float dt_;
	float kp_;              // orantili kazanc - ivmeolcer duzeltmesinin gucu
	float ki_;              // integral kazanc - kalan gyro bias'ini surekli duzeltir

	// Yonelim quaternion'u. Baslangic: birim quaternion = "hic donmemis"
	float q0_ = 1.0f, q1_ = 0.0f, q2_ = 0.0f, q3_ = 0.0f;

	// Integral geri besleme terimi - gyro bias tahminini biriktirir
	float integralFB_[3] = {0.0f, 0.0f, 0.0f};
};
