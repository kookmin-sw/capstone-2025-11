#pragma once

#include <mutex>
#include <thread>

#include <queue>
#include <string>

#include <stdio.h>
#include <assert.h>

#include <ceres/ceres.h>
#include <ceres/rotation.h>
#include <opencv2/opencv.hpp>
#include <eigen3/Eigen/Dense>

#include "keyframe.h"
#include "parameters.h"
#include "../utility/tic_toc.h"
#include "../utility/utility.h"
#include "../utility/tic_toc.h"
#include "../ThirdParty/DBoW/DBoW2.h"
#include "../ThirdParty/DVision/DVision.h"
#include "../ThirdParty/DBoW/TemplatedDatabase.h"
#include "../ThirdParty/DBoW/TemplatedVocabulary.h"

using namespace DVision;
using namespace DBoW2;

#define SKIP_FIRST_CNT 10

class PoseGraph
{
public:
	explicit PoseGraph(
		std::string& external_path,
		const std::string& brief_pattern_file,
		const std::string& vocabulary_file, 
		const bool load_previous_pose_graph,
		double skip_dis, int row, int col
	);

	~PoseGraph();
	
	void addKeyFrame(KeyFramePtr cur_kf, bool flag_detect_loop);
	void loadKeyFrame(KeyFramePtr cur_kf, bool flag_detect_loop);
	void loadVocabulary(AAssetManager* asset_manager);
	
	
	KeyFramePtr getKeyFrame(int index);
	
	void savePoseGraph(const std::vector<std::string>& labels);
	void loadPoseGraph();
	//추가
	bool InitialPose(KeyFramePtr);
	//추가
	const std::string VOCABULARY_FILE;
	bool LOAD_PREVIOUS_POSE_GRAPH;
	const double SKIP_DIS;

	int skip_first_cnt = 0;

	Eigen::Vector3d t_drift;
	double yaw_drift;
	Eigen::Matrix3d r_drift;
	
	Eigen::Vector3d last_t = Eigen::Vector3d(-100, -100, -100);

public:
    pair<float, float> getLastElementOfKeyFrameList();
	int detectLoop(KeyFramePtr keyframe, int frame_index);
	void addKeyFrameIntoVoc(KeyFramePtr keyframe);
	void addKeyFrameBuf(KeyFramePtr data);
    int getKeyFrameListSize();
	void optimize4DoF();
	void loopClosure();
	void new_sequence();
	void command();
	void setIntrinsicParam(double fx, double fy, double cx, double cy);

	vector<int> labeled_index;

	list<KeyFramePtr> keyframelist;

	std::mutex m_keyframelist;
	std::mutex m_optimize_buf;
	std::mutex m_path;
	std::mutex m_drift;
	std::mutex m_buf;
	std::mutex m_process;
	
	std::thread t_loopClosure;
	queue<KeyFramePtr> keyframe_buf;
	
	std::thread t_optimization;
	std::queue<int> optimize_buf;

	int global_index;
	int sequence;
	int sequence_cnt;
	vector<bool> sequence_loop;
	map<int, cv::Mat> image_pool;
	int earliest_loop_index;
	int base_sequence;
	int frame_index;

	//추가
	int idx = 0;
	double x = 0.0;
	double z = 0.0;
    int max_len;
	int max_count = 0;
    cv::Mat K;
	//추가
	BriefDatabase db;
	BriefVocabulary* voc;
};

template <typename T>
T NormalizeAngle(const T& angle_degrees) {
  if (angle_degrees > T(180.0))
  	return angle_degrees - T(360.0);
  else if (angle_degrees < T(-180.0))
  	return angle_degrees + T(360.0);
  else
  	return angle_degrees;
};

class AngleLocalParameterization {
 public:

  template <typename T>
  bool operator()(const T* theta_radians, const T* delta_theta_radians,
                  T* theta_radians_plus_delta) const {
    *theta_radians_plus_delta =
        NormalizeAngle(*theta_radians + *delta_theta_radians);

    return true;
  }

  static ceres::LocalParameterization* Create() {
    return (new ceres::AutoDiffLocalParameterization<AngleLocalParameterization,
                                                     1, 1>);
  }
};

template <typename T> 
void YawPitchRollToRotationMatrix(const T yaw, const T pitch, const T roll, T R[9])
{

	T y = yaw / T(180.0) * T(M_PI);
	T p = pitch / T(180.0) * T(M_PI);
	T r = roll / T(180.0) * T(M_PI);


	R[0] = cos(y) * cos(p);
	R[1] = -sin(y) * cos(r) + cos(y) * sin(p) * sin(r);
	R[2] = sin(y) * sin(r) + cos(y) * sin(p) * cos(r);
	R[3] = sin(y) * cos(p);
	R[4] = cos(y) * cos(r) + sin(y) * sin(p) * sin(r);
	R[5] = -cos(y) * sin(r) + sin(y) * sin(p) * cos(r);
	R[6] = -sin(p);
	R[7] = cos(p) * sin(r);
	R[8] = cos(p) * cos(r);
};

template <typename T> 
void RotationMatrixTranspose(const T R[9], T inv_R[9])
{
	inv_R[0] = R[0];
	inv_R[1] = R[3];
	inv_R[2] = R[6];
	inv_R[3] = R[1];
	inv_R[4] = R[4];
	inv_R[5] = R[7];
	inv_R[6] = R[2];
	inv_R[7] = R[5];
	inv_R[8] = R[8];
};

template <typename T> 
void RotationMatrixRotatePoint(const T R[9], const T t[3], T r_t[3])
{
	r_t[0] = R[0] * t[0] + R[1] * t[1] + R[2] * t[2];
	r_t[1] = R[3] * t[0] + R[4] * t[1] + R[5] * t[2];
	r_t[2] = R[6] * t[0] + R[7] * t[1] + R[8] * t[2];
};

struct FourDOFError
{
	FourDOFError(double t_x, double t_y, double t_z, double relative_yaw, double pitch_i, double roll_i)
				  :t_x(t_x), t_y(t_y), t_z(t_z), relative_yaw(relative_yaw), pitch_i(pitch_i), roll_i(roll_i){}

	template <typename T>
	bool operator()(const T* const yaw_i, const T* ti, const T* yaw_j, const T* tj, T* residuals) const
	{
		T t_w_ij[3];
		t_w_ij[0] = tj[0] - ti[0];
		t_w_ij[1] = tj[1] - ti[1];
		t_w_ij[2] = tj[2] - ti[2];

		// euler to rotation
		T w_R_i[9];
		YawPitchRollToRotationMatrix(yaw_i[0], T(pitch_i), T(roll_i), w_R_i);
		// rotation transpose
		T i_R_w[9];
		RotationMatrixTranspose(w_R_i, i_R_w);
		// rotation matrix rotate point
		T t_i_ij[3];
		RotationMatrixRotatePoint(i_R_w, t_w_ij, t_i_ij);

		residuals[0] = (t_i_ij[0] - T(t_x));
		residuals[1] = (t_i_ij[1] - T(t_y));
		residuals[2] = (t_i_ij[2] - T(t_z));
		residuals[3] = NormalizeAngle(yaw_j[0] - yaw_i[0] - T(relative_yaw));

		return true;
	}

	static ceres::CostFunction* Create(const double t_x, const double t_y, const double t_z,
									   const double relative_yaw, const double pitch_i, const double roll_i) 
	{
	  return (new ceres::AutoDiffCostFunction<
	          FourDOFError, 4, 1, 3, 1, 3>(
	          	new FourDOFError(t_x, t_y, t_z, relative_yaw, pitch_i, roll_i)));
	}

	double t_x, t_y, t_z;
	double relative_yaw, pitch_i, roll_i;

};

struct FourDOFWeightError
{
	FourDOFWeightError(double t_x, double t_y, double t_z, double relative_yaw, double pitch_i, double roll_i)
				  :t_x(t_x), t_y(t_y), t_z(t_z), relative_yaw(relative_yaw), pitch_i(pitch_i), roll_i(roll_i){
				  	weight = 1;
				  }

	template <typename T>
	bool operator()(const T* const yaw_i, const T* ti, const T* yaw_j, const T* tj, T* residuals) const
	{
		T t_w_ij[3];
		t_w_ij[0] = tj[0] - ti[0];
		t_w_ij[1] = tj[1] - ti[1];
		t_w_ij[2] = tj[2] - ti[2];

		// euler to rotation
		T w_R_i[9];
		YawPitchRollToRotationMatrix(yaw_i[0], T(pitch_i), T(roll_i), w_R_i);
		// rotation transpose
		T i_R_w[9];
		RotationMatrixTranspose(w_R_i, i_R_w);
		// rotation matrix rotate point
		T t_i_ij[3];
		RotationMatrixRotatePoint(i_R_w, t_w_ij, t_i_ij);

		residuals[0] = (t_i_ij[0] - T(t_x)) * T(weight);
		residuals[1] = (t_i_ij[1] - T(t_y)) * T(weight);
		residuals[2] = (t_i_ij[2] - T(t_z)) * T(weight);
		residuals[3] = NormalizeAngle((yaw_j[0] - yaw_i[0] - T(relative_yaw))) * T(weight) / T(10.0);

		return true;
	}

	static ceres::CostFunction* Create(const double t_x, const double t_y, const double t_z,
									   const double relative_yaw, const double pitch_i, const double roll_i) 
	{
	  return (new ceres::AutoDiffCostFunction<
	          FourDOFWeightError, 4, 1, 3, 1, 3>(
	          	new FourDOFWeightError(t_x, t_y, t_z, relative_yaw, pitch_i, roll_i)));
	}

	double t_x, t_y, t_z;
	double relative_yaw, pitch_i, roll_i;
	double weight;

};

class FourDOFAnalyticError : public ceres::SizedCostFunction<4, 1, 3, 1, 3>
{
public:
  FourDOFAnalyticError(double t_x, double t_y, double t_z,
                       double relative_yaw, double pitch_i, double roll_i)
      : t_x_(t_x), t_y_(t_y), t_z_(t_z),
        relative_yaw_(relative_yaw),
        pitch_i_(pitch_i), roll_i_(roll_i) {}

  virtual ~FourDOFAnalyticError() {}

  virtual bool Evaluate(double const* const* parameters,
                        double* residuals,
                        double** jacobians) const override {

    const double yaw_i = parameters[0][0];
    const double* t_i = parameters[1];
    const double yaw_j = parameters[2][0];
    const double* t_j = parameters[3];

    double t_w_ij[3] = {
		t_j[0] - t_i[0],
		t_j[1] - t_i[1],
		t_j[2] - t_i[2]
    };

    double w_R_i[9];
    YawPitchRollToRotationMatrix(yaw_i, pitch_i_, roll_i_, w_R_i);

    double i_R_w[9];
    RotationMatrixTranspose(w_R_i, i_R_w);

    double t_i_ij[3];
    RotationMatrixRotatePoint(i_R_w, t_w_ij, t_i_ij);

    residuals[0] = t_i_ij[0] - t_x_;
    residuals[1] = t_i_ij[1] - t_y_;
    residuals[2] = t_i_ij[2] - t_z_;
    residuals[3] = NormalizeAngle(yaw_j - yaw_i - relative_yaw_);
    const double deg_to_rad = M_PI / 180.0;

    if (jacobians) {
      // dR_dyaw_i
      double y = yaw_i / 180.0 * M_PI;
      double p = pitch_i_ / 180.0 * M_PI;
      double r = roll_i_ / 180.0 * M_PI;

      double cy = cos(y), sy = sin(y);
      double cp = cos(p), sp = sin(p);
      double cr = cos(r), sr = sin(r);

      double dR_dy[9] = {
        -sy*cp*deg_to_rad,   (-sy*sp*sr - cy*cr)*deg_to_rad,   (-sy*sp*cr + cy*sr)*deg_to_rad,
         cy*cp*deg_to_rad,    (cy*sp*sr - sy*cr)*deg_to_rad,    (cy*sp*cr + sy*sr)*deg_to_rad,
         0,        0,                  0
      };

      // diRw_dyaw = (dR_dyaw)^T
      double diRw_dyaw[9];
      RotationMatrixTranspose(dR_dy, diRw_dyaw);

      if (jacobians[0]) {
        double* J_yaw_i = jacobians[0];
        for (int k = 0; k < 3; ++k) {
          J_yaw_i[k] =
            diRw_dyaw[k * 3 + 0] * t_w_ij[0] +
            diRw_dyaw[k * 3 + 1] * t_w_ij[1] +
            diRw_dyaw[k * 3 + 2] * t_w_ij[2];
        }
        J_yaw_i[3] = -1.0;  // d(yaw_j - yaw_i - rel_yaw) / dyaw_i
      }

      if (jacobians[1]) {
        double* J_ti = jacobians[1];
        for (int r = 0; r < 3; ++r)
          for (int c = 0; c < 3; ++c)
            J_ti[r * 3 + c] = -i_R_w[r * 3 + c];
        std::fill(J_ti + 9, J_ti + 12, 0.0);  // last row = 0
      }

      if (jacobians[2]) {
        double* J_yaw_j = jacobians[2];
        J_yaw_j[0] = 0.0;
        J_yaw_j[1] = 0.0;
        J_yaw_j[2] = 0.0;
        J_yaw_j[3] = 1.0;
      }

      if (jacobians[3]) {
        double* J_tj = jacobians[3];
        for (int r = 0; r < 3; ++r)
          for (int c = 0; c < 3; ++c)
            J_tj[r * 3 + c] = i_R_w[r * 3 + c];
        std::fill(J_tj + 9, J_tj + 12, 0.0);
      }
    }

    return true;
  }

  static ceres::CostFunction* Create(double t_x, double t_y, double t_z,
                                     double relative_yaw,
                                     double pitch_i, double roll_i) {
    return new FourDOFAnalyticError(t_x, t_y, t_z, relative_yaw, pitch_i, roll_i);
  }

private:
  double t_x_, t_y_, t_z_;
  double relative_yaw_, pitch_i_, roll_i_;
};
