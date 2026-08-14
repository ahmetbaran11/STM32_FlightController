#include "AttitudeEstimator.h"
#include <cmath>


namespace {
    constexpr float ACC_MIN = 0.85f;
    constexpr float ACC_MAX = 1.15f;
}

AttitudeEstimator::AttitudeEstimator(float dt, float kp, float ki) : dt_(dt), kp_(kp), ki_(ki) {}

void AttitudeEstimator::init(const ImuSample& s) {
	// Eski hata birikimi yeni baslangicta gecersiz
	integralFB_[0] = integralFB_[1] = integralFB_[2] = 0.0f;

	float norm = sqrtf(s.ax*s.ax + s.ay*s.ay + s.az*s.az);

	// update() ile ayni makullük kontrolu: titresim, carpma ya da bozuk
	// bir okuma sirasinda acilirsak yanlis bir yonelimden baslamayalim.
	// Boyle bir durumda birim quaternion'da (duz) kaliyoruz; ivmeolcer
	// duzelince update() zaten dogru acya cekecek.
	if (norm < ACC_MIN || norm > ACC_MAX || s.accelSaturated) {
		q0_ = 1.0f; q1_ = 0.0f; q2_ = 0.0f; q3_ = 0.0f;
		return;
	}

	float roll  = atan2f(s.ay, s.az);
	float pitch = atan2f(-s.ax, sqrtf(s.ay*s.ay + s.az*s.az));

	float cr = cosf(roll  * 0.5f), sr = sinf(roll  * 0.5f);
	float cp = cosf(pitch * 0.5f), sp = sinf(pitch * 0.5f);

	q0_ =  cr * cp;
	q1_ =  sr * cp;
	q2_ =  cr * sp;
	q3_ = -sr * sp;
}

Attitude AttitudeEstimator::update(const ImuSample& s) {
	float ax = s.ax, ay = s.ay, az = s.az;
	float norm = sqrtf(ax*ax + ay*ay + az*az);


	// Yercekimi disinda bir ivme varsa (hizlanma, carpma, titresim) vektorun
	// buyuklugu 1g'den sapar ve artik "asagi" yonunu gostermez. Boyle
	// orneklerde ivmeolcere guvenmeyip sadece gyro ile ilerliyoruz.
	bool useAccel = (norm > ACC_MIN && norm < ACC_MAX) && !s.accelSaturated;
	float ex = 0.0f, ey = 0.0f, ez = 0.0f;
	if (useAccel) {
	    ax /= norm;  ay /= norm;  az /= norm;
	    // Quaternion'un soyledigi yonelimde yercekimi hangi eksende hissedilmeli?
	    // Dunya koordinatlarindaki asagi vektorunu govde koordinatlarina cevirir.
	    float vx = 2.0f * (q1_*q3_ - q0_*q2_);
	    float vy = 2.0f * (q0_*q1_ + q2_*q3_);
	    float vz = q0_*q0_ - q1_*q1_ - q2_*q2_ + q3_*q3_;

	    // Olculen yercekimi yonu ile tahmin edilen yon arasindaki fark.
	    // Capraz carpim: sonucun yonu donme eksenini, buyuklugu ise
	    // sin(hata acisi) degerini verir - kucuk acilarda dogrudan hata.
	    ex = (ay * vz - az * vy);
	    ey = (az * vx - ax * vz);
	    ez = (ax * vy - ay * vx);

	}

	constexpr float DEG2RAD = 0.01745329252f;
	float gx = s.gx * DEG2RAD;
	float gy = s.gy * DEG2RAD;
	float gz = s.gz * DEG2RAD;

	if (useAccel) {
	// Integral terimi: hatayi zamanla biriktirerek kalan gyro bias'ini bulur.
	// Sadece ivmeolcere guvendigimizde birikir - aksi halde cop veriyle sisirdik.
		integralFB_[0] += ki_ * ex * dt_;
		integralFB_[1] += ki_ * ey * dt_;
		integralFB_[2] += ki_ * ez * dt_;

		// Integral terimi sinirsiz birikirse, bozukluk gectikten sonra bile
		// filtre uzun sure toparlanamaz (integral windup). Makul bir bant
		// disina cikmasina izin vermiyoruz.
		constexpr float MAX_INTEGRAL = 0.1f;   // rad/s mertebesinde
		for (int i = 0; i < 3; i++) {
			if (integralFB_[i] >  MAX_INTEGRAL) integralFB_[i] =  MAX_INTEGRAL;
			if (integralFB_[i] < -MAX_INTEGRAL) integralFB_[i] = -MAX_INTEGRAL;
		}

		gx += kp_ * ex + integralFB_[0];
		gy += kp_ * ey + integralFB_[1];
		gz += kp_ * ez + integralFB_[2];
	}

	// Quaternion'un turevi: q_nokta = 0.5 * q (x) omega
	float qDot0 = 0.5f * (-q1_*gx - q2_*gy - q3_*gz);
	float qDot1 = 0.5f * ( q0_*gx + q2_*gz - q3_*gy);
	float qDot2 = 0.5f * ( q0_*gy - q1_*gz + q3_*gx);
	float qDot3 = 0.5f * ( q0_*gz + q1_*gy - q2_*gx);

	// Euler ileri entegrasyonu: bir adim ilerlet
	q0_ += qDot0 * dt_;
	q1_ += qDot1 * dt_;
	q2_ += qDot2 * dt_;
	q3_ += qDot3 * dt_;

	// Her adimdan sonra birim uzunluga geri cek.
	// Kayan nokta yuvarlama hatalari ve Euler entegrasyonunun yaklasikligi
	// yuzunden quaternion zamanla birim uzunluktan sapar; duzeltilmezse
	// yonelim matematigi bozulur.
	float qNorm = sqrtf(q0_*q0_ + q1_*q1_ + q2_*q2_ + q3_*q3_);
	if (qNorm > 0.0f) {
		q0_ /= qNorm;  q1_ /= qNorm;  q2_ /= qNorm;  q3_ /= qNorm;
	}

	// --- Cikti icin quaternion -> Euler ---
	constexpr float RAD2DEG = 57.29577951f;

	// asin argumani kayan nokta hatasiyla +/-1 disina tasabilir;
	// tasarsa asinf NaN dondurur ve cikti kalici olarak bozulur.
	float sinPitch = 2.0f * (q0_*q2_ - q1_*q3_);
	if (sinPitch >  1.0f) sinPitch =  1.0f;
	if (sinPitch < -1.0f) sinPitch = -1.0f;

	Attitude result;
	result.roll  = atan2f(2.0f * (q0_*q1_ + q2_*q3_),
		                      1.0f - 2.0f * (q1_*q1_ + q2_*q2_)) * RAD2DEG;
	result.pitch = asinf(sinPitch) * RAD2DEG;
	return result;
}
