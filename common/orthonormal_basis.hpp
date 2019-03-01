﻿#pragma once
#include <glm/glm.hpp>
#include <glm/ext.hpp>

namespace rt {
	// Building an Orthonormal Basis, Revisited
	template <typename Real, glm::precision P>
	inline void getOrthonormalBasis(const glm::tvec3<Real, P>& zaxis, glm::tvec3<Real, P> *xaxis, glm::tvec3<Real, P> *yaxis) {
		const Real sign = std::copysign(Real(1.0), zaxis.z);
		const Real a = Real(-1.0) / (sign + zaxis.z);
		const Real b = zaxis.x * zaxis.y * a;
		*xaxis = glm::tvec3<Real, P>(Real(1.0) + sign * zaxis.x * zaxis.x * a, sign * b, -sign * zaxis.x);
		*yaxis = glm::tvec3<Real, P>(b, sign + zaxis.y * zaxis.y * a, -zaxis.y);
	}

	// z が上, 任意の x, y
	// 一般的な極座標系とも捉えられる
	struct OrthonormalBasis {
		OrthonormalBasis(const glm::dvec3 &zAxis) : zaxis(zAxis) {
			getOrthonormalBasis(zAxis, &xaxis, &yaxis);
		}
		glm::dvec3 localToGlobal(const glm::dvec3 v) const {
			/*
			matrix
			xaxis.x, yaxis.x, zaxis.x
			xaxis.y, yaxis.y, zaxis.y
			xaxis.z, yaxis.z, zaxis.z
			*/
			return v.x * xaxis + v.y * yaxis + v.z * zaxis;
		}
		glm::dvec3 globalToLocal(const glm::dvec3 v) const {
			/*
			matrix
			xaxis.x, xaxis.y, xaxis.z
			yaxis.x, yaxis.y, yaxis.z
			zaxis.x, zaxis.y, zaxis.z
			*/
			return
				v.x * glm::dvec3(xaxis.x, yaxis.x, zaxis.x)
				+
				v.y * glm::dvec3(xaxis.y, yaxis.y, zaxis.y)
				+
				v.z * glm::dvec3(xaxis.z, yaxis.z, zaxis.z);
		}

		// axis on global space
		glm::dvec3 xaxis;
		glm::dvec3 yaxis;
		glm::dvec3 zaxis;
	};
}