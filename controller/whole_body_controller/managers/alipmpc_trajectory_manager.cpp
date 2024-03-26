#include "controller/whole_body_controller/managers/alipmpc_trajectory_manager.hpp"
#include "util/interpolation.hpp"
#include "controller/robot_system/pinocchio_robot_system.hpp"
#include "controller/draco_controller/draco_definition.hpp"
#include "planner/locomotion/dcm_planner/foot_step.hpp"
#include "util/util.hpp"
#include <iostream>
#include <fstream>
#include "configuration.hpp"

void printInData(input_data_t* indata);


AlipMpcTrajectoryManager::AlipMpcTrajectoryManager(NewStep_mpc *alipMpc,
    Task *com_xy_task, Task *com_z_task, Task *torso_ori, Task *lfoot_task, 
    Task *lfoot_ori, Task *rfoot_task, Task *rfoot_ori, 
    ForceTask *lf_force_, ForceTask *rg_force_, PinocchioRobotSystem *robot)
    : alipMpc(alipMpc), com_xy_task(com_xy_task), com_z_task(com_z_task), 
      torso_ori(torso_ori), lfoot_task(lfoot_task), lfoot_ori(lfoot_ori),
      rfoot_task(rfoot_task), rfoot_ori(rfoot_ori), 
      lf_force_task(lf_force_), rg_force_task(rg_force_),robot_(robot) {
  
  //util::PrettyConstructor(2, "AlipMpcTrajectoryManager");
  printCounter = 5;
  first_ever = false;

  indata.xlip_current[2] = 0;
  indata.xlip_current[3] = 0; 
  saveCounter = 0;

  /*
  file1.open(THIS_COM "/test/alip/alip_COM_trajectory.txt", std::fstream::in | std::fstream::out | std::fstream::app);
  file2.open(THIS_COM "/test/alip/Swing1_trajectory.txt", std::fstream::in | std::fstream::out | std::fstream::app);
  file3.open(THIS_COM "/test/alip/Swing2_trajectory.txt", std::fstream::in | std::fstream::out | std::fstream::app);
  file4.open(THIS_COM "/test/alip/BezierSwing_trajectory.txt", std::fstream::in | std::fstream::out | std::fstream::app);
  */

  file1.open(THIS_COM "/test/alip/MpcCOMstate.txt", std::fstream::out);

  file6.open(THIS_COM "/test/alip/Alip2Swing_trajectory.txt", std::fstream::out);
  file7.open(THIS_COM "/test/alip/robotSwingFootTraj.txt", std::fstream::out);
  file8.open(THIS_COM "/test/alip/RobotCOM.txt", std::fstream::out);
  file9.open(THIS_COM "/test/alip/RobotCommand.txt", std::fstream::out);

  Swingfootvel_end(2) = -0.5;


}  //need to add ori 

void AlipMpcTrajectoryManager::initializeOri(){
  des_torso_iso = robot_->GetLinkIsometry(draco_link::torso_link);
  FootStep::MakeHorizontal(des_torso_iso);

  des_torso_quat_ = Eigen::Quaterniond(des_torso_iso.linear());
  des_swfoot_quat_ = des_torso_quat_;

}


void AlipMpcTrajectoryManager::setNewOri(){
    if (com_yaw != 0){
      des_torso_iso = robot_->GetLinkIsometry(draco_link::torso_link);
      FootStep::MakeHorizontal(des_torso_iso);
      Eigen::Matrix3d rotation; 
      rotation << cos(com_yaw), -sin(com_yaw), 0, 
                  sin(com_yaw), cos(com_yaw) , 0,
                  0           , 0            , 1;
      des_torso_iso.linear() = rotation*des_torso_iso.linear();

      des_torso_quat_ = Eigen::Quaterniond(des_torso_iso.linear());
      des_swfoot_quat_ = des_torso_quat_;
    }

}

void AlipMpcTrajectoryManager::MpcSolutions(const double &tr_, const double &st_leg_, 
                                            const double &Lx_offset_, const double &Ly_des_,
                                            const double &com_yaw_, 
                                            const double &kx_, const double &ky_,
                                            const double &mu_) {  //generate footsteps and COM pos
  indata.Tr = tr_;
  indata.Lx_offset = Lx_offset_;
  indata.Ly_des = Ly_des_;
  com_yaw = com_yaw_;
  indata.kx = kx_;
  indata.ky = ky_;
  indata.mu = mu_;
  indata.stance_leg = st_leg_;


  this->InertiaToMpcCoordinates();

   //after we will put Lx and Ly as inputs of the function
  alipMpc->Update_(indata, outdata, fullsol);
  this->OutputMpcToInertiaCoordinates();
}

//use torso horientazion with make horizontal
void AlipMpcTrajectoryManager::InertiaToMpcCoordinates(){
    Eigen::Vector3d pos = robot_->GetRobotComPos();

    //Link coordinates = Rotation*inertial coordinates + position; for now just position implemented
    //we are going to use torso orientation as mpc orientation
    if(indata.stance_leg == 1){  //stance leg is right
      stleg_pos << robot_->GetLinkIsometry(draco_link::r_foot_contact).translation();
    }
    else {   //stance leg is left
      stleg_pos << robot_->GetLinkIsometry(draco_link::l_foot_contact).translation();
    }
    terrain = Eigen::Vector3d(-indata.kx, -indata.ky, 1);
    terrain /= (terrain.norm());

    //Change the actual torso ori with desired torso ori
    //Eigen::Isometry3d torso_iso = robot_->GetLinkIsometry(draco_link::torso_link);
    //FootStep::MakeHorizontal(torso_iso);  
    
    
    Eigen::Vector3d rotpos = des_torso_iso.linear().transpose() * pos;
    Eigen::Vector3d stleg_pos_torso_ori =  des_torso_iso.linear().transpose() * stleg_pos;

    indata.xlip_current[0] = rotpos(0)-stleg_pos_torso_ori(0);
    indata.xlip_current[1] = rotpos(1)-stleg_pos_torso_ori(1);

    /*
    indata.zH = terrain(0)*indata.xlip_current[0];
    indata.zH += terrain(1)*indata.xlip_current[1];
    indata.zH += terrain(2)*(rotpos(2)-stleg_pos_torso_ori(2));
    */
    indata.zH = refzH;

    pos = Eigen::Vector3d(indata.xlip_current[0],indata.xlip_current[1],indata.zH);
    Eigen::Vector3d vel = robot_->GetRobotComLinVel();   //check? the velocity frame needs to be aligned with the foot frame. 
                                                        //now it is aligned with the inertia frame. Maybe a rotation of robot pos and vel is needed.
    vel = des_torso_iso.linear().transpose() * vel;   //we are assuming that the rotation matrix doens't change with time

    Eigen::Vector3d L = pos.cross(mass*vel);
    indata.xlip_current[2]= L[0];             
    indata.xlip_current[3]= L[1];
    indataLz = L[2];
}


void AlipMpcTrajectoryManager::OutputMpcToInertiaCoordinates(){
  //COM
  double sw_end_x = outdata.ufp_wrt_st[0];
  double sw_end_y = outdata.ufp_wrt_st[1];

  double sw_end_z = this->ComputeZpos(sw_end_x, sw_end_y, 0); //assumes flat surface
                                                              //if not flat should be Swingfoot_end instead of 0 
  Swingfoot_end << sw_end_x, sw_end_y, sw_end_z;

  Swingfoot_end = des_torso_iso.linear() * Swingfoot_end;

  Swingfoot_end = Swingfoot_end + stleg_pos;

  Swingfoot_end(2) = this->ComputeZpos(Swingfoot_end(0), Swingfoot_end(1), 0);
}

void AlipMpcTrajectoryManager::add_residual_rl_action(const Eigen::VectorXd &action){
  Eigen::Vector3d res_pos(action(0), action(1), 0);
  Swingfoot_end += res_pos;
  Eigen::Quaterniond res_quat = util::EulerZYXtoQuat(0, 0, action(2));
  des_swfoot_quat_ *= res_quat;

}

void AlipMpcTrajectoryManager::GenerateTrajs(const double &tr_){  //will only use alip2
  indata.Tr = tr_;
  Eigen::Isometry3d curr_swfoot_iso;
  Eigen::Quaterniond start_swfoot_quat;
  Eigen::Quaterniond start_torso_quat(robot_->GetLinkIsometry(draco_link::torso_com_link).linear());


  if (indata.stance_leg == 1){  //LF is swing foot
    curr_swfoot_iso = robot_->GetLinkIsometry(draco_link::l_foot_contact);  //chequear si contact solo funciona cuando es stance foot
    start_swfoot_quat = Eigen::Quaterniond(robot_->GetLinkIsometry(draco_link::l_foot_contact).linear());

  }
  else {
    curr_swfoot_iso = robot_->GetLinkIsometry(draco_link::r_foot_contact);
    start_swfoot_quat = Eigen::Quaterniond(robot_->GetLinkIsometry(draco_link::r_foot_contact).linear());
  }

  Eigen::Vector3d curr_swfoot_pos = curr_swfoot_iso.translation();


  AlipSwingPos2 = new AlipSwing2(Swingfoot_start, Swingfoot_end, swing_height, indata.Ts);
  torso_ori_curve_ = new HermiteQuaternionCurve(start_torso_quat, Eigen::Vector3d::Zero(), 
                                                des_torso_quat_, Eigen::Vector3d::Zero(), indata.Ts);

  swfoot_ori_curve_= new HermiteQuaternionCurve(start_swfoot_quat, Eigen::Vector3d::Zero(),
                                                des_swfoot_quat_, Eigen::Vector3d::Zero(), indata.Ts);

}

void AlipMpcTrajectoryManager::UpdateDesired(const double t){  
  //t is the time since last evaluation of Mpc and trajectory was generated so it's the time since Tr was coomputed
  //not sure works fine for every trajectory right now
  //only works for alip2swing

  Eigen::Isometry3d curr_swfoot_iso;
  if (indata.stance_leg == 1){  //LF is swing foot
    curr_swfoot_iso = robot_->GetLinkIsometry(draco_link::l_foot_contact);  //chequear si contact solo funciona cuando es stance foot
    FootStep::MakeHorizontal(curr_swfoot_iso);          
  }
  else {
    curr_swfoot_iso = robot_->GetLinkIsometry(draco_link::r_foot_contact);
    FootStep::MakeHorizontal(curr_swfoot_iso);
  }
  //file7 << curr_swfoot_iso.translation().transpose() << "  " << std::endl;


  des_swfoot_pos = AlipSwingPos2->Evaluate(t);
  des_swfoot_vel = AlipSwingPos2->EvaluateFirstDerivative(t);
  des_swfoot_acc = AlipSwingPos2->EvaluateSecondDerivative(t);

  swfoot_ori_curve_->Evaluate(t, des_swfoot_quat);
  swfoot_ori_curve_->GetAngularVelocity(t, des_swfoot_ang_vel);
  swfoot_ori_curve_->GetAngularAcceleration(t, des_swfoot_ang_acc);
  
  Eigen::VectorXd des_swfoot_ori(4);
  des_swfoot_ori << des_swfoot_quat.coeffs();
  Eigen::VectorXd des_swfoot_ori_vel(3);
  des_swfoot_ori_vel << des_swfoot_ang_vel;
  Eigen::VectorXd des_swfoot_ori_acc(3);
  des_swfoot_ori_acc << des_swfoot_ang_acc;

  torso_ori_curve_->Evaluate(t, des_torso_quat);
  torso_ori_curve_->GetAngularVelocity(t, des_torso_ang_vel);
  torso_ori_curve_->GetAngularAcceleration(t, des_torso_ang_acc);

  Eigen::VectorXd des_torso_ori(4);
  des_torso_ori << des_torso_quat.coeffs();
  Eigen::VectorXd des_torso_ori_vel(3);
  des_torso_ori_vel << des_torso_ang_vel;
  Eigen::VectorXd des_torso_ori_acc(3);
  des_torso_ori_acc << des_swfoot_ang_acc;







  for (int i = 0; i < 3; i++){
    if (des_swfoot_acc(i) > 10) des_swfoot_acc(i) = 10.;
    else if(des_swfoot_acc(i) < -10) des_swfoot_acc(i) = -10.;
    if (des_swfoot_vel(i) > 15) des_swfoot_vel(i) = 15.;
    else if(des_swfoot_vel(i) < -15) des_swfoot_vel(i) = -15.;
  }



  //in single_support_swing update both the tasks
  //for the stance leg they use UseCurrent. Is it really necessary
  //it would mean that tasks are erased at each controller iteration
  if (indata.stance_leg == 1){ //update left
    lfoot_task->UpdateDesired(des_swfoot_pos, des_swfoot_vel, des_swfoot_acc);
    lfoot_ori->UpdateDesired(des_swfoot_ori, des_swfoot_ori_vel, des_swfoot_ori_acc);
    this->UpdateCurrentPos(rfoot_task);


    //Set hierarchy weights
    com_xy_task->SetWeight(com_xy_task_weight);
    com_z_task->SetWeight(com_z_task_weight);
    rfoot_task->SetWeight(stance_foot_weight);
    lfoot_task->SetWeight(swing_foot_weight);
    lfoot_ori->SetWeight(swing_foot_ori_weight);
    rfoot_ori->SetWeight(stance_foot_ori_weight);
  }
  else { //update right
    rfoot_task->UpdateDesired(des_swfoot_pos, des_swfoot_vel, des_swfoot_acc);
    rfoot_ori->UpdateDesired(des_swfoot_ori, des_swfoot_ori_vel, des_swfoot_ori_acc);
    this->UpdateCurrentPos(lfoot_task);


    //set hierarchy weights
    com_xy_task->SetWeight(com_xy_task_weight);
    com_z_task->SetWeight(com_z_task_weight);
    rfoot_task->SetWeight(swing_foot_weight);
    lfoot_task->SetWeight(stance_foot_weight);
    rfoot_ori->SetWeight(swing_foot_ori_weight);
    lfoot_ori->SetWeight(stance_foot_ori_weight);
    torso_ori->SetWeight(torso_ori_weight);

  }
  
  Eigen::VectorXd Z(1);
  Z << refzH;
  com_z_task->UpdateDesired(Z, Eigen::VectorXd::Zero(1),
                            Eigen::VectorXd::Zero(1));
                            


  Eigen::VectorXd zero3 = Eigen::VectorXd::Zero(3);
  torso_ori->UpdateDesired(des_torso_ori, des_torso_ori_vel, des_torso_ori_acc);
}


void AlipMpcTrajectoryManager::UpdateCurrentOri(Task* task){
  Eigen::Quaterniond des_ori_quat(
    robot_->GetLinkIsometry(task->TargetIdx()).linear());
  Eigen::VectorXd des_ori(4);

  des_ori << des_ori_quat.normalized().coeffs();

  Eigen::VectorXd des_ang_vel = Eigen::VectorXd::Zero(3);
  Eigen::VectorXd des_acc = Eigen::VectorXd::Zero(3);
  
  task->UpdateDesired(des_ori, des_ang_vel, des_acc);
}


void AlipMpcTrajectoryManager::UpdateCurrentPos(Task* task){
  Eigen::VectorXd des_pos(3);
  des_pos << robot_->GetLinkIsometry(task->TargetIdx()).translation();

  Eigen::VectorXd des_vel = Eigen::VectorXd::Zero(3);
  Eigen::VectorXd des_acc = Eigen::VectorXd::Zero(3);

  task->UpdateDesired(des_pos, des_vel, des_acc);
}

//will assume that kx and ky are true in the actual frame of reference 
//need a high level controller or perception to adjust kx and ky when turning on incline plane
//normal vector to ground is (-kx, -ky, 1)
void AlipMpcTrajectoryManager::MakeParallelToGround(Eigen::Isometry3d &pose){
  FootStep::MakeHorizontal(pose);
  const Eigen::Matrix3d R = pose.linear();
  //const Eigen::Vector3d p = pose.translation(); don't change
  double kx = indata.kx;
  double ky = indata.ky;
  double c1 = 1/sqrt(1 + kx*kx);
  double c2 = 1/sqrt(1 + kx*kx + ky*ky);
  Eigen::Matrix3d T;
  T <<  c1  ,   -c1*c2*kx*ky   , -c2*kx,
         0  , c1*c2*(kx*kx + 1), -c2*ky,
       c1*kx,     c1*c2*ky     ,   c2;
  pose.linear() = T*R;
}

double AlipMpcTrajectoryManager::ComputeZpos(const double &x, const double &y, const double &zH_){
  return indata.kx*x + indata.ky*y + zH_;
}



void AlipMpcTrajectoryManager::saveRobotCommand(const double t){
  file9 << des_swfoot_pos.transpose() << " " << des_swfoot_vel.transpose() << " " << des_swfoot_acc.transpose();
  file9 << " " << t << endl;
}



void AlipMpcTrajectoryManager::saveMpcCOMstate(const double t){
  file1 << indata.xlip_current[0] << " " << indata.xlip_current[1] << " " << indata.zH << "  ";
  file1 << indata.xlip_current[2] << " " << indata.xlip_current[3] << " " << indataLz << " ";
  file1 << t << " " << indata.Lx_offset << " " << indata.Ly_des << " ";
  file1 << indata.mu << " "  << indata.leg_width << " " << indata.zH << " ";
  file1 << mass << " " << indata.Ts <<  " ";
  file1 << fullsol.xlip_sol[0] << " " << fullsol.xlip_sol[1] << " " << fullsol.xlip_sol[2] << " ";
  file1 << fullsol.xlip_sol[3] << endl;
}


void AlipMpcTrajectoryManager::saveCurrentCOMstate(const double t){
  Eigen::Vector3d pos = robot_->GetRobotComPos();
  Eigen::Vector3d vel = robot_->GetRobotComLinVel();
  Eigen::Vector3d posWorld = pos;

  Eigen::Isometry3d torso_iso = robot_->GetLinkIsometry(draco_link::torso_link);
  FootStep::MakeHorizontal(torso_iso);  

  pos = torso_iso.linear().transpose() * pos;
  Eigen::Vector3d stleg_pos_torso_ori =  torso_iso.linear().transpose() * stleg_pos;

  pos -= stleg_pos_torso_ori;
  vel = torso_iso.linear().transpose() * vel;
  Eigen::Vector3d L = pos.cross(mass*vel);

  file8 << pos.transpose() << "  ";
  file8 << L.transpose() << " ";
  file8 << t <<  " " << posWorld.transpose() << " ";
  file8 << stleg_pos.transpose() << " ";
  file8 << vel.transpose() << endl;
}

void AlipMpcTrajectoryManager::saveSwingState(const double t){
  Eigen::Isometry3d curr_swfoot_iso;
  Eigen::Matrix<double, 6, 1> curr_swfoot_vel;
  if (indata.stance_leg == 1){  //LF is swing foot
    curr_swfoot_iso = robot_->GetLinkIsometry(draco_link::l_foot_contact);  //chequear si contact solo funciona cuando es stance foot
    FootStep::MakeHorizontal(curr_swfoot_iso);  
    curr_swfoot_vel = robot_->GetLinkBodySpatialVel(draco_link::l_foot_contact);        
  }
  else {
    curr_swfoot_iso = robot_->GetLinkIsometry(draco_link::r_foot_contact);
    FootStep::MakeHorizontal(curr_swfoot_iso);
    curr_swfoot_vel = robot_->GetLinkBodySpatialVel(draco_link::r_foot_contact);

  }
  file7 << curr_swfoot_iso.translation().transpose() << "  " << t << " " << 
           curr_swfoot_vel[3, 0] << " " <<  curr_swfoot_vel[4, 0] << " " << curr_swfoot_vel[5, 0] << std::endl;
}


void AlipMpcTrajectoryManager::saveTrajectories(const double start_time, const double dt,const double end_time){
/*
  std::fstream file1;
  std::fstream file2;
  std::fstream file3;
  std::fstream file4;


  file1.open(THIS_COM "/test/alip/alip_COM_trajectory.txt", ios::out);
  file2.open(THIS_COM "/test/alip/Swing1_trajectory.txt", ios::out);
  file3.open(THIS_COM "/test/alip/Swing2_trajectory.txt", ios::out);
  file4.open(THIS_COM "/test/alip/BezierSwing_trajectory.txt", ios::out);
*/
  saveCounter++;
  if (!file2.is_open() || !file3.is_open() || !file4.is_open() || !file5.is_open() || !file6.is_open()) {
      std::cerr << "Error: Unable to open one or more files for writing." << std::endl;
      return;
  }
  file2 <<"start------"<< saveCounter << "------------------------" << std::endl;
  file3 <<"start------"<< saveCounter << "------------------------" << std::endl;
  file4 <<"start------"<< saveCounter << "------------------------" << std::endl;
  file5 <<"start------"<< saveCounter << "------------------------" << std::endl;
  file6 <<"start------"<< saveCounter << "------------------------" << std::endl;


  for (double t = start_time; t <= end_time; t += dt){
    Eigen::Vector3d SwingAlip2 = AlipSwingPos2->Evaluate(t);

    file6 << SwingAlip2.transpose() << "  " << t <<  std::endl;



  }
  file6 <<"end------"<< saveCounter << "------------------------" << std::endl;

/*
  file1.close();
  file2.close();
  file3.close();
  file4.close();
*/

}



void AlipMpcTrajectoryManager::SetParameters(const YAML::Node &node) {
  try {
    util::ReadParameter(node, "Ts", indata.Ts);
    util::ReadParameter(node, "zH", refzH);
    util::ReadParameter(node, "leg_width", indata.leg_width);
    util::ReadParameter(node, "total_mass", mass);
    util::ReadParameter(node, "Lx_offset", indata.Lx_offset);
    util::ReadParameter(node, "Ly_des", indata.Ly_des);
    util::ReadParameter(node, "variable_height", variable_height);
    util::ReadParameter(node, "reference_swing_height", reference_swing_height);
    util::ReadParameter(node, "swing_height", swing_height);
  } catch (const std::runtime_error &e) {
    std::cerr << "Error reading parameter [" << e.what() << "] at file: ["
              << __FILE__ << "]" << std::endl;
    std::exit(EXIT_FAILURE);
  }
}



void AlipMpcTrajectoryManager::SetTaskWeights(const YAML::Node &node){
  try {
    util::ReadParameter(node, "com_z_pos", com_z_task_weight);
    util::ReadParameter(node, "com_xy_pos", com_xy_task_weight);
    util::ReadParameter(node, "torso_ori", torso_ori_weight);
    util::ReadParameter(node, "swing_foot_weight", swing_foot_weight);
    util::ReadParameter(node, "stance_foot_weight", stance_foot_weight);
    util::ReadParameter(node, "stance_foot_ori_weight", stance_foot_ori_weight);
    util::ReadParameter(node, "swing_foot_ori_weight", swing_foot_ori_weight);


  } catch (const std::runtime_error &e) {
    std::cerr << "Error reading parameter [" << e.what() << "] at file: ["
              << __FILE__ << "]" << std::endl;
    std::exit(EXIT_FAILURE);
  }
}

void printInData(input_data_t* data) {
    //printf("time: %f\n", data->time);
    //printf("s: %f\n", data->s);
    printf("x_c: %f\n", data->xlip_current[0]);
    printf("y_c: %f\n", data->xlip_current[1]);
    printf("L_xc: %f\n", data->xlip_current[2]);
    printf("L_yc: %f\n", data->xlip_current[3]);
    printf("stance_leg: %f\n", data->stance_leg);
    printf("zH: %f\n", data->zH);
    printf("Ts: %f\n", data->Ts);
    printf("Tr: %f\n", data->Tr);
   // printf("leg_width: %f\n", data->leg_width);
    printf("Lx_offset: %f\n", data->Lx_offset);
    printf("Ly_des: %f\n", data->Ly_des);
    //printf("kx: %f\n", data->kx);
    //printf("ky: %f\n", data->ky);
    //printf("mu: %f\n", data->mu);
    printf("---\n");
}