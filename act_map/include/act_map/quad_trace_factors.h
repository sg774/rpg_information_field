#pragma once

#include <rpg_common/eigen_type.h>
#include <vi_utils/vi_jacobians.h>

#include "act_map/info_utils.h"
#include "act_map/sampler.h"

namespace act_map
{
using TraceKValueType = double;
using TraceK1 = Eigen::Matrix<TraceKValueType, 3, 3>;
using TraceK1Vec = rpg::Aligned<std::vector, TraceK1>;

using TraceK2 = Eigen::Matrix<TraceKValueType, 3, 1>;
using TraceK2Vec = rpg::Aligned<std::vector, TraceK2>;

using TraceK3 = Eigen::Matrix<TraceKValueType, 1, 1>;
using TraceK3Vec = rpg::Aligned<std::vector, TraceK3>;
}  // namespace act_map

namespace act_map
{
template <typename F>
void manipulateFactorSingle(const Eigen::Vector3d& pw,
                            const Eigen::Vector3d& twc, TraceK1* K1,
                            TraceK2* K2, TraceK3* K3)
{
  Eigen::Vector3d p0 = pw - twc;
  Eigen::Matrix3d p0p0T = p0 * p0.transpose();
  double d0 = p0.norm();

  rpg::Rotation rot;
  rot.setIdentity();
  rpg::Matrix36 J0 =
      vi_utils::jacobians::dBearing_dIMUPoseGlobal(pw, rpg::Pose(rot, -twc));

  rpg::Vector6 diag_H0;
  for (int i = 0; i < 6; i++)
  {
    diag_H0(i) =
        J0(0, i) * J0(0, i) + J0(1, i) * J0(1, i) + J0(2, i) * J0(2, i);
  }
  rpg::Vector6 diag_H0_over_d = diag_H0 / d0;
  rpg::Vector6 diag_H0_over_d2 = diag_H0_over_d / d0;

  F func;
  Eigen::Matrix3d v1 = Eigen::Matrix3d::Zero();
  Eigen::Vector3d v2(0, 0, 0);
  double v3 = 0;
  for (int i = 0; i < 6; i++)
  {
    v1 += p0p0T * diag_H0_over_d2(i);
    v2 += p0 * diag_H0_over_d(i);
    v3 += diag_H0(i);
  }
  TraceK1 v1k = v1.cast<TraceKValueType>();
  TraceK2 v2k = v2.cast<TraceKValueType>();
  TraceK3 v3k;
  v3k(0, 0) = static_cast<TraceKValueType>(v3);
  func.operator()(*K1, v1k);
  func.operator()(*K2, v2k);
  func.operator()(*K3, v3k);
}

inline void minTraceDirectionUnconstrained(const double k1, const double k2,
                                           const TraceK1& K1, const TraceK2& K2,
                                           Eigen::Vector3d* view)
{
  //  act_map::TraceK1 ApAT = k1 * (K1 + K1.transpose());
  rpg::Matrix33 ApAT = 2 * k1 * K1.cast<double>();  // since K1 is symmetric
  //  Eigen::JacobiSVD<act_map::TraceK1> svd(
  //      ApAT, Eigen::ComputeThinU | Eigen::ComputeThinV);
  //  CHECK_EQ(3, svd.rank());
  rpg::Matrix31 B = k2 * K2.cast<double>();
  (*view) = -ApAT.inverse() * B;
  view->normalize();
}

double getTraceAtRotation(const Eigen::Matrix3d& Rwc, const double k1,
                          const double k2, const double k3, const TraceK1& K1,
                          const TraceK2& K2, const TraceK3& K3);

// overloaded functions
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
inline double getInfoMetricAtRotationFromPositionalFactor(
    const Eigen::Matrix3d& Rwc, const double k1, const double k2,
    const double k3, const TraceK1& K1, const TraceK2& K2, const TraceK3& K3,
    const InfoMetricType& v, Eigen::Vector3d* drot_global = nullptr)
{
  if (v != InfoMetricType::kTrace)
  {
    LOG_FIRST_N(WARNING, 100) << "Query " << kInfoMetricNames.at(v)
                              << " from trace kernel. Falling back to trace. ";
  }
  static double dtheta = (1.0 / 180.0) * M_PI;
  if (drot_global)
  {
    for (int i = 0; i < 3; i++)
    {
      Eigen::Matrix3d Rwcm;
      Eigen::Matrix3d Rwcp;
      utils::getRotationPerturbTwoSides(Rwc, dtheta, i, &Rwcm, &Rwcp);

      double valm = getTraceAtRotation(Rwcm, k1, k2, k3, K1, K2, K3);
      double valp = getTraceAtRotation(Rwcp, k1, k2, k3, K1, K2, K3);
      (*drot_global)(i) = (valp - valm) / (2 * dtheta);
    }
  }
  return getTraceAtRotation(Rwc, k1, k2, k3, K1, K2, K3);
}
#pragma GCC diagnostic pop
}  // namespace act_map
