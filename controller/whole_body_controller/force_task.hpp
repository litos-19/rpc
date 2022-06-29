#pragma once
#include "util/util.hpp"

// for wrench task -> in order of torque, force
class ForceTask {
public:
  ForceTask(const int dim)
      : dim_(dim), rf_des_(Eigen::VectorXd::Zero(dim)),
        weight_(Eigen::VectorXd::Zero(dim)) {
    util::PrettyConstructor(3, "ForceTask");
  };
  virtual ~ForceTask() = default;

  // setter
  void UpdateDesired(const Eigen::VectorXd &rf_des) { rf_des_ = rf_des; };

  // getter
  Eigen::VectorXd DesiredRf() const { return rf_des_; }
  Eigen::VectorXd Weight() const { return weight_; }
  int Dim() const { return dim_; }

  void SetParameters(const YAML::Node &node, const bool b_sim) {
    try {
      weight_ =
          b_sim ? util::ReadParameter<Eigen::VectorXd>(node, "weight_at_swing")
                : util::ReadParameter<Eigen::VectorXd>(node,
                                                       "exp_weight_at_swing");
    } catch (std::runtime_error &e) {
      std::cerr << "Error reading parameter [" << e.what() << "] at file: ["
                << __FILE__ << "]" << std::endl
                << std::endl;
      std::exit(EXIT_FAILURE);
    }
  }

protected:
  int dim_;
  Eigen::VectorXd rf_des_;
  Eigen::VectorXd weight_;
};
