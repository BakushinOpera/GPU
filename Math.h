#pragma once
#define _USE_MATH_DEFINES
#include <cmath>
#define SQ(x) ((x) * (x))
#include <iostream>

template<typename T>
static T lerp(const T a, const T b, float t) {
	return a * (1.0f - t) + (b * t);
}

template<typename t>
t simpleInterpolateAttribute(t a0, t a1, t a2, float alpha, float beta, float gamma) {
	return (a0 * alpha) + (a1 * beta) + (a2 * gamma);
}
template<typename t>
t perspectiveCorrectInterpolateAttribute(t a0, t a1, t a2, float v0_w, float v1_w, float v2_w,
	float alpha, float beta, float gamma, float frag_w) {
	t attrib[3];
	attrib[0] = a0 * alpha * v0_w;
	attrib[1] = a1 * beta * v1_w;
	attrib[2] = a2 * gamma * v2_w;
	return ((attrib[0] + attrib[1] + attrib[2]) / frag_w);
}

class Vec3 {
public:
	union {
		float v[3];
		struct { float x, y, z; };
	};

	Vec3() : v{} {};
	Vec3(float x, float y, float z) : v{ x,y,z } {};

	Vec3 operator+(const Vec3& a) const {
		return Vec3(v[0] + a.v[0], v[1] + a.v[1], v[2] + a.v[2]);
	}

	Vec3 operator+=(const Vec3& a) {
		v[0] += a.v[0];
		v[1] += a.v[1];
		v[2] += a.v[2];
		return *this;
	}
	Vec3 operator*(const Vec3& a) const {
		return Vec3(v[0] * a.v[0], v[1] * a.v[1], v[2] * a.v[2]);
	}
	Vec3 operator*(const float val) const {
		return Vec3(v[0] * val, v[1] * val, v[2] * val);
	}
	Vec3 operator-() const {
		return Vec3(-v[0], -v[1], -v[2]);
	}
	Vec3 operator-(const Vec3& b) const {
		return Vec3(v[0] - b.v[0], v[1] - b.v[1], v[2] - b.v[2]);
	}
	Vec3 operator/(float f) const {
		return Vec3(v[0] / f, v[1] / f, v[2] / f);
	}

	float GetLength() const {
		return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	}
	Vec3 normalize() const {
		float len = 1.0f / GetLength();
		return Vec3(x * len, y * len, z * len);
	}

	float Dot(const Vec3& a) const {
		return v[0] * a.v[0] + v[1] * a.v[1] + v[2] * a.v[2];
	}
	static float Dot(const Vec3& v1, const Vec3& v2) {
		return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
	}
	const Vec3 Cross(const Vec3& v1) const {
		return Vec3(v[1] * v1.v[2] - v[2] * v1.v[1],
					v[2] * v1.v[0] - v[0] * v1.v[2],
					v[0] * v1.v[1] - v[1] * v1.v[0]);
	}
	float Max() const {
		return (std::max)(x, (std::max)(y, z));
	}
	static Vec3 Max(const Vec3& v1, const Vec3& v2) {
		return Vec3((std::max)(v1.v[0], v2.v[0]),
					(std::max)(v1.v[1], v2.v[1]),
					(std::max)(v1.v[2], v2.v[2]));
	}

};

class Vec4 {
public:
	union {
		float v[4];
		struct { float x, y, z, w; };
	};

	Vec4() : v{} {};
	Vec4(float x, float y, float z, float w) : v{ x,y,z,w} {};
	
	Vec4 DividedByW() const {
		return Vec4(x / w, y / w, z / w, 1 / w);
	}
	Vec4 operator-(const Vec4& b) const {
		return Vec4(x - b.x, y - b.y, z - b.z, 1);
	}
	Vec4 operator*(const Vec4& a) const {
		return Vec4(x * a.x, y * a.y, z * a.z, 1);
	}
};

class Matrix {
public:
	union {
		float a[4][4];
		float m[16];
	};
	static Matrix identity() {
		Matrix identity;
		identity.a[0][0] = identity.a[1][1] = identity.a[2][2] = identity.a[3][3] = 1.0f;
		return identity;
	}

	Matrix() :m{} {};

	float& operator[](int index) {
		return m[index];
	}

	Vec4 mul(const Vec4& v) {
		return Vec4(
			(v.x * m[0] + v.y * m[1] + v.z * m[2] + v.w * m[3]),
			(v.x * m[4] + v.y * m[5] + v.z * m[6] + v.w * m[7]),
			(v.x * m[8] + v.y * m[9] + v.z * m[10] + v.w * m[11]),
			(v.x * m[12] + v.y * m[13] + v.z * m[14] + v.w * m[15])
		);
	}
	Matrix mul(const Matrix& b) const{
		Matrix r;
		for (int i = 0;i < 4;i++) {
			for (int j = 0;j < 4;j++) {
				for (int k = 0;k < 4;k++) {
					r.a[i][j] += a[i][k] * b.a[k][j];
				}
			}
		}
		return r;
	}
	Matrix operator*(const Matrix& matrix) const {
		return mul(matrix);
	}

	Vec3 mulPoint(const Vec3& v) {
		return Vec3(
			(v.x * m[0] + v.y * m[1] + v.z * m[2]) + m[3],
			(v.x * m[4] + v.y * m[5] + v.z * m[6]) + m[7],
			(v.x * m[8] + v.y * m[9] + v.z * m[10]) + m[11]
		);
	}
	Vec3 mulVec(const Vec3& v) {
		return Vec3(
			(v.x * m[0] + v.y * m[1] + v.z * m[2]),
			(v.x * m[4] + v.y * m[5] + v.z * m[6]),
			(v.x * m[8] + v.y * m[9] + v.z * m[10])
		);
	}
	
	static Matrix Rotation_X(float th) {
		float c = cos(th), s = std::sin(th);
		Matrix r = identity();
		r.a[1][1] = c; r.a[1][2] = -s;
		r.a[2][1] = s; r.a[2][2] = c;
		return r;
	}
	static Matrix Rotation_Y( float th) {
		float c = cos(th), s = std::sin(th);
		Matrix r = identity();
		r.a[0][0] = c ; r.a[0][2] = s;
		r.a[2][0] = -s; r.a[2][2] = c;
		return r;
	}
	static Matrix Rotation_Z(float th) {
		float c = cos(th), s = std::sin(th);
		Matrix r = identity();
		r.a[0][0] = c; r.a[0][1] = -s;
		r.a[1][0] = s; r.a[1][1] = c;
		return r;
	}
	static Matrix Translation(float tx, float ty, float tz) {
		Matrix r = identity();
		r.a[0][3] = tx;
		r.a[1][3] = ty;
		r.a[2][3] = tz;
		return r;
	}
	static Matrix Scaling(float sx, float sy, float sz) {
		Matrix r;
		r.a[0][0] = sx;
		r.a[1][1] = sy;
		r.a[2][2] = sz;
		r.a[3][3] = 1.0f;
		return r;
	}

	Matrix Transpose() const {
		Matrix c;
		for (int i = 0;i < 4;i++) {
			for (int j = 0;j < 4;j++) {
				c.a[i][j] = a[j][i];
			}
		}
		return c;
	}

	Matrix invert() const
	{
		Matrix inv;
		inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
		inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
		inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
		inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
		inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
		inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
		inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
		inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
		inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
		inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
		inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
		inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
		inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
		inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
		inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
		inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
		float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
		if (det == 0) {
			// Handle this case
		}
		det = 1.0 / det;
		for (int i = 0; i < 16; i++) {
			inv[i] = inv[i] * det;
		}
		return inv;
	}

	static Matrix Perspective_Proj(float zfar, float znear, float fov, float aspect) {
		Matrix r;
		float scale = zfar / (zfar - znear), offset = -(zfar * znear) / (zfar - znear);
		float thFov = 1 / (std::tan(fov / 2));

		r.a[0][0] = thFov / aspect;
		r.a[1][1] = thFov;
		r.a[2][2] = scale;
		r.a[2][3] = offset;
		r.a[3][2] = 1;
		return r;
	}

	static Matrix LookAt(const Vec3& from, const Vec3& to, const Vec3& up) {
		Vec3 dir = to - from;
		dir = dir.normalize();
		Vec3 right = up.Cross(dir);
		Vec3 _up = dir.Cross(right);
		right = right.normalize();_up = _up.normalize();
		Matrix r;
		r[0] = right.x;r[1] = right.y;r[2] = right.z;
		r[4] = _up.x;r[5] = _up.y;r[6] = _up.z;
		r[8] = dir.x;r[9] = dir.y;r[10] = dir.z;
		r[15] = 1;
		Matrix t = Translation(-from.x, -from.y, -from.z);
		return r * t;
	}


};

class Shading {
public:
	Vec3 n;
	Matrix ret;
	// calculate the basis (F)
	Matrix Gram_Schmidt(Vec3& n) {
		Matrix r;
		Vec3 u,v;
		if (n.x == 1 && n.y == 0 && n.z == 0) {
			u.x = 0;u.y = 1;u.z = 0;
		}
		else {
			u.x = 1;u.y = 0;u.z = 0;
		}
		u = u.Cross(n).normalize();
		v = n.Cross(u);
		r.a[0][0] = u.x;r.a[0][1] = v.x;r.a[0][2] = n.x;
		r.a[1][0] = u.y;r.a[1][1] = v.y;r.a[1][2] = n.y;
		r.a[2][0] = u.z;r.a[2][1] = v.z;r.a[2][2] = n.z;
		return r;
	}

	Shading() :n{ 1,1,1 }, ret{ Gram_Schmidt(n)} {}

	// from world to local, use transposed F
	Vec3 Forward(Vec3& w) const {
		Matrix _F = ret.Transpose();
		return Vec3(
			(_F.a[0][0] * w.x + _F.a[0][1] * w.y + _F.a[0][2] * w.z),
			(_F.a[1][0] * w.x + _F.a[1][1] * w.y + _F.a[1][2] * w.z),
			(_F.a[2][0] * w.x + _F.a[2][1] * w.y + _F.a[2][2] * w.z)
		);
	}
	// from local to world, use F
	Vec3 Backward(Vec3& w) const {
		Matrix F = ret;
		return Vec3(
			(F.a[0][0] * w.x + F.a[0][1] * w.y + F.a[0][2] * w.z),
			(F.a[1][0] * w.x + F.a[1][1] * w.y + F.a[1][2] * w.z),
			(F.a[2][0] * w.x + F.a[2][1] * w.y + F.a[2][2] * w.z)
		);
	}
};

class Spherical_cor {
public:
	float th, phi;
	static constexpr int r = 1;

	Spherical_cor(float _th, float _phi, const int _r) : th{ _th }, phi{ _phi } {};

	static Spherical_cor ToSphr(Vec3& v) {
		return Spherical_cor(std::acosf(v.z), std::atan2f(v.y, v.x), 1);
	}
	static float getTheta(const Vec3& v) {
		return std::acosf(v.z);
	}
	static float getPhi(const Vec3& v) {
		return std::atan2f(v.y, v.x);
	}
	Vec3 SphrToVec() {
		return Vec3(
			r * std::sin(th) * std::cos(phi),
			r * std::sin(th) * std::sin(phi),
			r * std::cos(th)
		);
	}
};

class Quaternion {
public:
	union {
		struct {
			float d, a, b, c;
		};
		float q[4];
	};
	//q = d + ai + bj + ck
	//  = c + v_x * s * i + v_y * s * j + v_z * s * k
	//c(d here) = cos(theta/2); s = sin(theta/2)
	Quaternion() :q{ 1,0,0,0 } {};
	Quaternion(float _d, float _a, float _b, float _c) : d{_d}, a{ _a }, b{ _b }, c{ _c } {};
	//operations
	Quaternion operator*(const Quaternion& q2) {
		float dd = d * q2.d - a * q2.a - b * q2.b - c * q2.c;
		float ai = d * q2.a + a * q2.d + b * q2.c - c * q2.b;
		float bj = d * q2.b - a * q2.c + b * q2.d + c * q2.a;
		float ck = d * q2.c + a * q2.b - b * q2.a + c * q2.d;
		return Quaternion(dd, ai, bj, ck);
	}
	// get |q|
	float Magnitude() const {
		return std::sqrt(a * a + b * b + c * c + d * d);
	}
	static Quaternion Normalize(const Quaternion& q) {
		float norm = q.Magnitude();
		return Quaternion(q.d / norm, q.a / norm, q.b / norm, q.c / norm);
	}
	// get q*
	Quaternion Conjugate() {
		return Quaternion(d, -a, -b, -c);
	}
	// get q^(-1)
	// if its normalized, |q| is always 1
	Quaternion Inverse() {
		//return Conjugate();
		float qq = Magnitude() * Magnitude();
		return Quaternion(
			Conjugate().d / qq, Conjugate().a / qq, Conjugate().b / qq, Conjugate().c / qq
		);
	}
	Vec3 Rotate_Vec(Vec3& v) const {
		Quaternion vp(0, v.x, v.y, v.z);
		Quaternion q(d, a, b, c);
		Quaternion vp_ = q * vp * q.Conjugate();
		return Vec3(vp_.a, vp_.b, vp_.c);
	}
	//Slerp
	float Dot(Quaternion& q2) const {
		return d * q2.d + a * q2.a + b * q2.b + c * q2.c;
	}
	float Find_angle(Quaternion& q2) const {
		float product = Dot(q2);
		if (product < 0) {
			product = -product;
			q2 = Quaternion(-q2.d, -q2.a,-q2.b,-q2.c);
		}
		return std::acosf(product);
	}
	// get q(interpolated)
	Quaternion Slerp(Quaternion q2, float t) {
		float th = Find_angle(q2);
		float s1 = std::sin(th * (1 - t));
		float s2 = std::sin(th);
		float s3 = std::sin(th*t);
		return Quaternion(
			d * (s1 / s2) + q2.d * (s3 / s2),
			a * (s1 / s2) + q2.a * (s3 / s2),
			b * (s1 / s2) + q2.b * (s3 / s2),
			c * (s1 / s2) + q2.c * (s3 / s2)
		);
	}

	Matrix toMatrix() const {
		float aa = a * a, ab = a * b, ac = a * c;
		float bb = b * b, bc = b * c, cc = c * c;
		float da = d * a, db = d * b, dc = d * c;
		Matrix m;
		m[0] = 1 - 2 * (bb + cc); m[1] = 2 * (ab - dc); m[2] = 2 * (ac + db); m[3] = 0;
		m[4] = 2 * (ab + dc); m[5] = 1 - 2 * (aa + cc); m[6] = 2 * (bc - da); m[7] = 0;
		m[8] = 2 * (ac - db); m[9] = 2 * (bc + da); m[10] = 1 - 2 * (aa + bb); m[11] = 0;
		m[12] = m[13] = m[14] = 0; m[15] = 1;
		return m;
	}
};

class Colour {
public:
	float r, g, b, a;

	Colour() :r{ 0 }, g{ 0 }, b{ 0 }, a{ 0 } {};
	Colour(float _r, float _g, float _b, float _a) : r(_r), g(_g), b(_b), a(_a) {};
	Colour(unsigned char _r, unsigned char _g, unsigned char _b, unsigned char _a) 
		:r{ _r / 255.0f }, g{ _g / 255.0f }, b{ _b / 255.0f }, a{ _a / 255.0f } {
	};
	Colour operator+(const Colour& c2) const {
		return Colour(r + c2.r, g + c2.g, b + c2.b, a);
	}
	Colour operator*(const Colour& c2) const {
		return Colour(r * c2.r, g * c2.g, b * c2.b, a);
	}
	Colour operator*(const float f) const {
		return Colour(r * f, g * f, b * f, a);
	}
	Colour operator/(const float f) const {
		return Colour(r / f, g / f, b / f, a);
	}
};
