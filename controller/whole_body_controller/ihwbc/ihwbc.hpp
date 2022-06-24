#pragma once
#include <Eigen/Dense>
#include <vector>

#include "third_party/optimizer/goldfarb/QuadProg++.hh"
#include "util/util.hpp"

class Task;
class Contact;
class InternalConstraint;
class ForceTask;
class IHWBC {
public:
  IHWBC(const Eigen::MatrixXd &Sa, const Eigen::MatrixXd *Sf = NULL,
        const Eigen::MatrixXd *Sv = NULL);
  virtual ~IHWBC();

  void UpdateSetting(const Eigen::MatrixXd &A, const Eigen::MatrixXd &Ainv,
                     const Eigen::VectorXd &cori, const Eigen::VectorXd &grav);
  void
  Solve(const std::vector<Task *> &task_container,
        const std::vector<Contact *> &contact_container,
        const std::vector<InternalConstraint *> &internal_constraint_container,
        const std::vector<ForceTask *> &force_task_container,
        Eigen::VectorXd &qddot_cmd, Eigen::VectorXd &rf_cmd,
        Eigen::VectorXd &trq_cmd);

  void SetParameters(const YAML::Node &node);

  // getter
  bool IsTrqLimit() const { return b_trq_limit_; }

  Eigen::VectorXd TrqCommand() const { return trq_cmd_; }
  Eigen::VectorXd QddotCommand() const { return qddot_sol_; }
  Eigen::VectorXd RfCommand() const { return rf_sol_; }

  // setter
  void SetTrqLimit(const Eigen::Matrix<double, Eigen::Dynamic, 2> &trq_limit) {
    joint_trq_limits_ = trq_limit;
  }

protected:
  void _SetQPCost(const Eigen::MatrixXd &cost_mat,
                  const Eigen::VectorXd &cost_vec);
  void _SetQPEqualityConstraint(const Eigen::MatrixXd &eq_mat,
                                const Eigen::VectorXd &eq_vec);
  void _SetQPInEqualityConstraint(const Eigen::MatrixXd &ineq_mat,
                                  const Eigen::VectorXd &ineq_vec);
  void _SolveQP();

  Eigen::MatrixXd A_;
  Eigen::MatrixXd Ainv_;
  Eigen::VectorXd cori_;
  Eigen::VectorXd grav_;

  int num_qdot_;
  int num_active_;
  int num_passive_;
  int num_float_;

  Eigen::MatrixXd sa_;
  Eigen::MatrixXd sv_;
  Eigen::MatrixXd sf_;
  Eigen::MatrixXd snf_;

  bool b_passive_;
  bool b_floating_;
  bool b_contact_;

  int dim_contact_;

  // optimization regularization parameters
  double lambda_qddot_;
  double lambda_rf_;
  bool b_trq_limit_;

  // joint pos, vel, trq limits (cols(1) = lower limit / cols(2) = upper limit)
  Eigen::Matrix<double, Eigen::Dynamic, 2> joint_trq_limits_;

  /// QP solver interface
  int num_qp_vars_ = 1;
  int num_eq_const_ = 0;
  int num_ineq_const_ = 0;

  Eigen::VectorXd qddot_sol_;
  Eigen::VectorXd rf_sol_;
  Eigen::VectorXd trq_cmd_;

  /// QP decision variables.
  GolDIdnani::GVect<double> x_;
  // Eigen::VectorXd x_;

  /// QP cost.
  GolDIdnani::GMatr<double> G_;
  GolDIdnani::GVect<double> g0_;

  /// QP equality cosntraint.
  GolDIdnani::GMatr<double> CE_;
  GolDIdnani::GVect<double> ce0_;

  /// QP inequality constraint.
  GolDIdnani::GMatr<double> CI_;
  GolDIdnani::GVect<double> ci0_;
};
