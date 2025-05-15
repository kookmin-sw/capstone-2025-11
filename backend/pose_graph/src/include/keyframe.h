#pragma once

#include <vector>
#include <android/log.h>
#include <android/asset_manager.h>
#include <eigen3/Eigen/Dense>
#include <opencv2/opencv.hpp>
#include <opencv2/core/eigen.hpp>
#include "../utility/tic_toc.h"
#include "../utility/utility.h"
#include "parameters.h"
#include "../ThirdParty/DBoW/DBoW2.h"
#include "../ThirdParty/DVision/DVision.h"

#define MIN_LOOP_NUM 25

#ifndef LOGI
#define LOGI(...) \
  __android_log_print(ANDROID_LOG_INFO, "hello_ar_example_c", __VA_ARGS__)
#endif  // LOGI

using namespace Eigen;
using namespace std;
using namespace DVision;


class BriefExtractor
{
public:
  virtual void operator()(const cv::Mat &im, vector<cv::KeyPoint> &keys, vector<BRIEF::bitset> &descriptors) const;
  BriefExtractor(AAssetManager* asset_manager);

  DVision::BRIEF m_brief;
};

struct IntrinsicParameter {
public:
	double fx = 0., fy = 0., cx = 0., cy = 0.;
	double m_inv_K11, m_inv_K13, m_inv_K22, m_inv_K23;

public:
	void update_parameter(double fx, double fy, double cx, double cy){
		this->fx = fx;
		this->fy = fy;
		this->cx = cx;
		this->cy = cy;
	
		this->m_inv_K11 = 1.0 / this->fx;
		this->m_inv_K13 = -this->cx / this->fx;
		this->m_inv_K22 = 1.0 / this->fy;
		this->m_inv_K23 = -this->cy / this->fy;
	}
};

class KeyFrame
{
public:
	KeyFrame(double _time_stamp, Vector3d &_vio_T_w_i, Matrix3d &_vio_R_w_i, cv::Mat &_image, int _sequence);
	// KeyFrame(double _time_stamp, int _index, Vector3d &_vio_T_w_i, Matrix3d &_vio_R_w_i, cv::Mat &_image,
	// vector<cv::Point3f> &_point_3d, vector<double> &_point_id, int _sequence);
	KeyFrame(double _time_stamp, int _index, Vector3d &_vio_T_w_i, Matrix3d &_vio_R_w_i, Vector3d &_T_w_i, Matrix3d &_R_w_i,
			 cv::Mat &_image, int _loop_index, Eigen::Matrix<double, 8, 1 > &_loop_info,
			 vector<cv::KeyPoint> &_keypoints, vector<cv::KeyPoint> &_keypoints_norm, vector<BRIEF::bitset> &_brief_descriptors);
	bool findConnection(std::shared_ptr<KeyFrame> old_kf);
	void computeWindowBRIEFPoint();
	void computeBRIEFPoint(AAssetManager* asset_manager, const IntrinsicParameter& param, const cv::Mat& depth_mat);
	//void extractBrief();
	int HammingDis(const BRIEF::bitset &a, const BRIEF::bitset &b);
	bool searchInAera(const BRIEF::bitset window_descriptor,
	                  const std::vector<BRIEF::bitset> &descriptors_old,
	                  const std::vector<cv::KeyPoint> &keypoints_old,
	                  const std::vector<cv::KeyPoint> &keypoints_old_norm,
	                  cv::Point2f &best_match,
	                  cv::Point2f &best_match_norm);
	void searchByBRIEFDes(std::vector<cv::Point2f> &matched_2d_old,
						  std::vector<cv::Point2f> &matched_2d_old_norm,
                          std::vector<uchar> &status,
                          const std::vector<BRIEF::bitset> &descriptors_old,
                          const std::vector<cv::KeyPoint> &keypoints_old,
                          const std::vector<cv::KeyPoint> &keypoints_old_norm);
	void FundmantalMatrixRANSAC(const std::vector<cv::Point2f> &matched_2d_cur_norm,
                                const std::vector<cv::Point2f> &matched_2d_old_norm,
                                vector<uchar> &status);
	void PnPRANSAC(const vector<cv::Point2f> &matched_2d_old_norm,
	               const std::vector<cv::Point3f> &matched_3d,
	               std::vector<uchar> &status,
	               Eigen::Vector3d &PnP_T_old, Eigen::Matrix3d &PnP_R_old);

	bool PnPRANSAC_Relative(const vector<cv::Point2f> &matched_2d_old_norm,
							const std::vector<cv::Point3f> &matched_3d,
							std::vector<uchar> &status,
							Eigen::Vector3d &PnP_T_old, Eigen::Matrix3d &PnP_R_old);

	void getVioPose(Eigen::Vector3d &_T_w_i, Eigen::Matrix3d &_R_w_i);
	void getPose(Eigen::Vector3d &_T_w_i, Eigen::Matrix3d &_R_w_i);
	void updatePose(const Eigen::Vector3d &_T_w_i, const Eigen::Matrix3d &_R_w_i);
	void updateVioPose(const Eigen::Vector3d &_T_w_i, const Eigen::Matrix3d &_R_w_i);
	void updateLoop(Eigen::Matrix<double, 8, 1 > &_loop_info);
	//추가
	bool findRelativePose(std::shared_ptr<KeyFrame> old_kf, Eigen::Vector3d &relative_t, Eigen::Quaterniond &relative_q);

	cv::Point2f undistorted(const IntrinsicParameter& param, const Eigen::Vector2d& p);

	Eigen::Vector3d getLoopRelativeT();
	double getLoopRelativeYaw();
	Eigen::Quaterniond getLoopRelativeQ();

	double time_stamp; 
	int index;
	int local_index;
	Eigen::Vector3d vio_T_w_i; 
	Eigen::Matrix3d vio_R_w_i; 
	Eigen::Vector3d T_w_i;
	Eigen::Matrix3d R_w_i;
	Eigen::Vector3d origin_vio_T;		
	Eigen::Matrix3d origin_vio_R;

	cv::Mat image;
	
	cv::Mat thumbnail;
	vector<cv::Point3f> point_3d; 
	vector<cv::Point2f> point_2d_uv;
	vector<cv::Point2f> point_2d_norm;
	
	vector<double> point_id;
	vector<cv::KeyPoint> keypoints;
	vector<cv::KeyPoint> keypoints_norm;
	vector<cv::KeyPoint> window_keypoints;
	vector<BRIEF::bitset> brief_descriptors;
	// vector<BRIEF::bitset> window_brief_descriptors;
	
	bool has_fast_point;
	int sequence;

	bool has_loop;
	int loop_index;
	Eigen::Matrix<double, 8, 1 > loop_info;
};

typedef std::shared_ptr<KeyFrame> KeyFramePtr;
