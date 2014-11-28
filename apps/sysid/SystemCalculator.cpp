#include "SystemCalculator.h"
#include "dart/dynamics/Skeleton.h"
#include "dart/dynamics/BodyNode.h"

#include <iomanip>

#include <Eigen/SVD>

SystemCalculator::SystemCalculator(const dart::dynamics::Skeleton* robot) :
  mRobot(robot)
{

}

static Eigen::Matrix3d skew(const Eigen::Vector3d& v)
{
  Eigen::Matrix3d S;
  S <<    0,  -v[2],  v[1],
       v[2],      0, -v[0],
      -v[1],   v[0],     0;

  return S;
}

namespace Eigen {
typedef Eigen::Matrix<double,3,6> Matrix36d;
} // namespace Eigen

static Eigen::Matrix36d L(const Eigen::Vector3d& w)
{
  Eigen::Matrix36d result;
  result <<  w[0], w[1], w[2],    0,    0,    0,
                0, w[0],    0, w[1], w[2],    0,
                0,    0, w[0],    0, w[1], w[2];

  return result;
}

Eigen::Matrix0610d SystemCalculator::computeA(size_t index) const
{
  using namespace dart::dynamics;
  const BodyNode* bn = mRobot->getBodyNode(index);
  if(!bn)
  {
    std::cerr << "out-of-bounds index: " << index << " (max " << mRobot->getNumBodyNodes() << ")"
              << std::endl;
    return Eigen::Matrix0610d::Zero();
  }

  Eigen::Matrix0610d A;

  const Eigen::Vector6d& V = bn->getBodyVelocity();
  const Eigen::Vector3d& v = V.block<3,1>(3,0);
  const Eigen::Vector3d& w = V.block<3,1>(0,0);

  const Eigen::Vector6d& a = bn->getBodyAcceleration();
  const Eigen::Vector3d& d = a.block<3,1>(3,0) + w.cross(v);
  const Eigen::Vector3d& w_dot = a.block<3,1>(0,0);

  const Eigen::Matrix3d& S_w = skew(w);
  A.block<3,1>(0,0).setZero();
  A.block<3,1>(3,0) = d;
  A.block<3,3>(0,1) = -skew(d);
  A.block<3,3>(3,1) = skew(w_dot) + S_w*S_w;
  A.block<3,6>(0,4) = L(w_dot) + S_w*L(w);
  A.block<3,6>(3,4).setZero();

  return A;
}


Eigen::Matrix6d SystemCalculator::computeSpatialTransform(size_t from, size_t to) const
{
  using namespace dart::dynamics;
  const BodyNode* bnFrom = mRobot->getBodyNode(from);
  const BodyNode* bnTo = mRobot->getBodyNode(to);
  if(!bnFrom || !bnTo)
  {
    std::cerr << "out-of-bounds index: " << from << ", " << to << " (max "
              << mRobot->getNumBodyNodes() << ")" << std::endl;
    return Eigen::Matrix6d::Identity();
  }

  const Eigen::Isometry3d& tfFrom = bnFrom->getTransform();
  const Eigen::Isometry3d& tfTo = bnTo->getTransform();
  const Eigen::Isometry3d& tf = tfTo.inverse()*tfFrom;

  Eigen::Matrix6d X;
  X.block<3,3>(0,0) = tf.linear();
  X.block<3,3>(3,0) = tf.linear()*skew(tf.translation()).transpose();
  X.block<3,3>(0,3).setZero();
  X.block<3,3>(3,3) = tf.linear();

  return X;
}


Eigen::VectorXd SystemCalculator::postProcessing(dart::dynamics::Skeleton* robot,
                                                 const std::vector<Eigen::VectorXd>& forces,
                                                 const std::vector<Eigen::MatrixXd>& dynamics)
{
  std::cout << "Creating data structures: " << forces.size() << ", " << dynamics.size() << std::endl;
  size_t n = dynamics[0].rows();
  std::cout << "n: " << n << std::endl;
  size_t P = dynamics.size();
  size_t C = dynamics[0].cols();
  Eigen::MatrixXd A(n*P,C);
  Eigen::VectorXd f(n*P);

  std::cout << "Filling data structures" << std::endl;
  for(size_t i=0; i<P; ++i)
  {
    A.block(i*n,0,n,C) = dynamics[i];
    f.block(i*n,0,n,1) = forces[i];
  }
  std::cout << "Data filled" << std::endl;

  Eigen::JacobiSVD<Eigen::MatrixXd> svd(A, Eigen::ComputeFullU | Eigen::ComputeFullV);
//  Eigen::JacobiSVD<Eigen::MatrixXd> svd(A);
//  if(!svd.computeU())
//    std::cout << "Call to computeU failed" << std::endl;
//  if(!svd.computeV())
//    std::cout << "Call to computeV failed" << std::endl;

  std::cout << "SVD constructed" << std::endl;

  size_t count = 0;
  for(int i=0; i<svd.singularValues().size(); ++i)
  {
    if(svd.singularValues()[i] > 1e-8)
      ++count;
  }

  std::cout << "We have " << count << " non-zero parameters out of a possible " << C << std::endl;

  Eigen::VectorXd parameters(C);
  parameters.setZero();

  std::cout << "U: " << svd.matrixU().cols() << "\n"
            << "V: " << svd.matrixV().cols() << std::endl;


  for(size_t j=0; j<C; ++j)
  {
    Eigen::VectorXd uj = svd.matrixU().col(j);
    Eigen::VectorXd vj = svd.matrixV().col(j);
    double mu = svd.singularValues()[j];
    if(mu < 1e-8)
      continue;

    parameters += vj*(uj.transpose()*f)/mu;
  }

//  std::cout << parameters.transpose() << std::endl;

  size_t nw = 4;
  for(size_t i=0; i<robot->getNumBodyNodes(); ++i)
    nw = std::max(nw, robot->getBodyNode(i)->getName().size());
  nw += 3;

  size_t w = 10;
  std::cout.width(nw);
  std::cout << "name" << std::setw(w)
            << "mass" << std::setw(w)
            << "mx" << std::setw(w)
            << "my" << std::setw(w)
            << "mz" << std::setw(w)
            << "Ixx" << std::setw(w)
            << "Ixy" << std::setw(w)
            << "Ixz" << std::setw(w)
            << "Iyy" << std::setw(w)
            << "Iyz" << std::setw(w)
            << "Izz" << std::endl;
  for(size_t i=0; i<(size_t)parameters.size()/10; ++i)
  {
    std::cout << std::setw(nw) << robot->getBodyNode(i)->getName();
    for(size_t p=0; p<10; ++p)
    {
      std::cout << std::setw(w) << std::setprecision(3) << parameters[10*i+p];
    }
    std::cout << "\n";
  }
  std::cout << std::flush;

  return parameters;
}
