#include "include/keyframe.h"
#include <sys/stat.h>
#include <sys/types.h>

template <typename Derived>
static void reduceVector(vector<Derived> &v, vector<uchar> status)
{
    int j = 0;
    for (int i = 0; i < int(v.size()); i++)
        if (status[i])
            v[j++] = v[i];
    v.resize(j);
}

// create keyframe online
KeyFrame::KeyFrame(double _time_stamp, Vector3d &_vio_T_w_i, Matrix3d &_vio_R_w_i, cv::Mat &_image, int _sequence)
{
	time_stamp = _time_stamp;
	vio_T_w_i = _vio_T_w_i;
	vio_R_w_i = _vio_R_w_i;
	T_w_i = vio_T_w_i;
	R_w_i = vio_R_w_i;
	origin_vio_T = vio_T_w_i;
	origin_vio_R = vio_R_w_i;

	image = _image.clone();

	cv::resize(image, thumbnail, cv::Size(80, 60));
	has_loop = false;
	loop_index = -1;
	has_fast_point = false;
	loop_info << 0, 0, 0, 0, 0, 0, 0, 0;
	sequence = _sequence;
}

// load previous keyframe
KeyFrame::KeyFrame(double _time_stamp, int _index, Vector3d &_vio_T_w_i, Matrix3d &_vio_R_w_i, Vector3d &_T_w_i, Matrix3d &_R_w_i,
					cv::Mat &_image, int _loop_index, Eigen::Matrix<double, 8, 1 > &_loop_info,
					vector<cv::KeyPoint> &_keypoints, vector<cv::KeyPoint> &_keypoints_norm, vector<BRIEF::bitset> &_brief_descriptors)
{
	time_stamp = _time_stamp;
	index = _index;
	//vio_T_w_i = _vio_T_w_i;
	//vio_R_w_i = _vio_R_w_i;
	vio_T_w_i = _T_w_i;
	vio_R_w_i = _R_w_i;
	T_w_i = _T_w_i;
	R_w_i = _R_w_i;
	if (_loop_index != -1)
		has_loop = true;
	else
		has_loop = false;
	loop_index = _loop_index;
	loop_info = _loop_info;
	has_fast_point = false;
	sequence = 0;
	keypoints = _keypoints;
	keypoints_norm = _keypoints_norm;
	brief_descriptors = _brief_descriptors;
}

void KeyFrame::computeBRIEFPoint(AAssetManager* asset_manager, const IntrinsicParameter& param, const cv::Mat& depth_mat)
{
	BriefExtractor extractor(asset_manager);
	
	vector<cv::Point2f> tmp_pts;
	cv::goodFeaturesToTrack(image, tmp_pts, 500, 0.01, 10);
	for(int i = 0; i < (int)tmp_pts.size(); i++)
	{
		cv::KeyPoint key;
		key.pt = tmp_pts[i];
        point_2d_uv.push_back(tmp_pts[i]);
		keypoints.push_back(key);
	}

	extractor(image, keypoints, brief_descriptors);

    double mx_u, my_u, depth;
	for (int i = 0; i < (int)keypoints.size(); i++) {
        uint16_t pixel_value = depth_mat.at<uint16_t>((int) keypoints[i].pt.y, (int) keypoints[i].pt.x);

        mx_u = param.m_inv_K11 * keypoints[i].pt.x + param.m_inv_K13;
        my_u = param.m_inv_K22 * keypoints[i].pt.y + param.m_inv_K23;
        
        keypoints_norm.emplace_back(cv::KeyPoint());
        keypoints_norm.back().pt = cv::Point2f(mx_u, my_u);
        point_2d_norm.push_back(keypoints_norm.back().pt);

		depth = 1.0 * pixel_value / 1000.0;
		point_3d.emplace_back(cv::Point3f(mx_u * depth, my_u * depth, depth));
	}
}

void BriefExtractor::operator() (const cv::Mat &im, vector<cv::KeyPoint> &keys, vector<BRIEF::bitset> &descriptors) const
{
  m_brief.compute(im, keys, descriptors);
}

void KeyFrame::getVioPose(Eigen::Vector3d &_T_w_i, Eigen::Matrix3d &_R_w_i)
{
    _T_w_i = vio_T_w_i;
    _R_w_i = vio_R_w_i;
}

void KeyFrame::getPose(Eigen::Vector3d &_T_w_i, Eigen::Matrix3d &_R_w_i)
{
    _T_w_i = T_w_i;
    _R_w_i = R_w_i;
}

void KeyFrame::updatePose(const Eigen::Vector3d &_T_w_i, const Eigen::Matrix3d &_R_w_i)
{
    T_w_i = _T_w_i;
    R_w_i = _R_w_i;
}

void KeyFrame::updateVioPose(const Eigen::Vector3d &_T_w_i, const Eigen::Matrix3d &_R_w_i)
{
	vio_T_w_i = _T_w_i;
	vio_R_w_i = _R_w_i;
	T_w_i = vio_T_w_i;
	R_w_i = vio_R_w_i;
}

Eigen::Vector3d KeyFrame::getLoopRelativeT()
{
    return Eigen::Vector3d(loop_info(0), loop_info(1), loop_info(2));
}

Eigen::Quaterniond KeyFrame::getLoopRelativeQ()
{
    return Eigen::Quaterniond(loop_info(3), loop_info(4), loop_info(5), loop_info(6));
}

double KeyFrame::getLoopRelativeYaw()
{
    return loop_info(7);
}

void KeyFrame::updateLoop(Eigen::Matrix<double, 8, 1 > &_loop_info)
{
	if (abs(_loop_info(7)) < 30.0 && Vector3d(_loop_info(0), _loop_info(1), _loop_info(2)).norm() < 20.0)
	{
		//printf("update loop info\n");
		loop_info = _loop_info;
	}
}

BriefExtractor::BriefExtractor(AAssetManager* asset_manager){
  std::string internal_path = EXTERNAL_PATH + "/" + BRIEF_PATTERN_FILE;

  std::ifstream infile(internal_path);
  if (!infile.good()) {
    // Only copy if file doesn't exist
    AAsset* asset = AAssetManager_open(asset_manager, ("brief/" + BRIEF_PATTERN_FILE).c_str(), AASSET_MODE_STREAMING);
    if (!asset) throw std::runtime_error("Could not open asset: " + BRIEF_PATTERN_FILE);

    FILE* out = fopen(internal_path.c_str(), "wb");
    if (!out) {
        AAsset_close(asset);
        throw std::runtime_error("Could not create output file: " + internal_path);
    }

    char buffer[1024];
    int bytes_read;
    while ((bytes_read = AAsset_read(asset, buffer, sizeof(buffer))) > 0) {
        fwrite(buffer, 1, bytes_read, out);
    }
    fclose(out);
    AAsset_close(asset);
  }

  cv::FileStorage fs(internal_path.c_str(), cv::FileStorage::READ);
  if(!fs.isOpened()) throw std::runtime_error("Could not open FileStorage file: " + internal_path);

  std::vector<int> x1, y1, x2, y2;
  fs["x1"] >> x1;
  fs["x2"] >> x2;
  fs["y1"] >> y1;
  fs["y2"] >> y2;
  
  m_brief.importPairs(x1, y1, x2, y2);

  fs.release();
}

bool KeyFrame::searchInAera(const BRIEF::bitset _brief_descriptors,
                            const std::vector<BRIEF::bitset> &descriptors_old,
                            const std::vector<cv::KeyPoint> &keypoints_old,
                            const std::vector<cv::KeyPoint> &keypoints_old_norm,
                            cv::Point2f &best_match,
                            cv::Point2f &best_match_norm)
{
    cv::Point2f best_pt;
    int bestDist = 128;
    int bestIndex = -1;
    for(int i = 0; i < (int)descriptors_old.size(); i++)
    {
        int dis = HammingDis(_brief_descriptors, descriptors_old[i]);
        if(dis < bestDist)
        {
            bestDist = dis;
            bestIndex = i;
        }
    }
    //printf("best dist %d", bestDist);
    if (bestIndex != -1 && bestDist < 80)
    {
      best_match = keypoints_old[bestIndex].pt;
      best_match_norm = keypoints_old_norm[bestIndex].pt;
      return true;
    }
    else
      return false;
}

void KeyFrame::searchByBRIEFDes(std::vector<cv::Point2f> &matched_2d_old,
								std::vector<cv::Point2f> &matched_2d_old_norm,
                                std::vector<uchar> &status,
                                const std::vector<BRIEF::bitset> &descriptors_old,
                                const std::vector<cv::KeyPoint> &keypoints_old,
                                const std::vector<cv::KeyPoint> &keypoints_old_norm)
{
    for(int i = 0; i < (int)brief_descriptors.size(); i++)
    {
        cv::Point2f pt(0.f, 0.f);
        cv::Point2f pt_norm(0.f, 0.f);
        if (searchInAera(brief_descriptors[i], descriptors_old, keypoints_old, keypoints_old_norm, pt, pt_norm))
          status.push_back(1);
        else
          status.push_back(0);
        matched_2d_old.push_back(pt);
        matched_2d_old_norm.push_back(pt_norm);
    }

}

void KeyFrame::PnPRANSAC(const vector<cv::Point2f> &matched_2d_old_norm,
                         const std::vector<cv::Point3f> &matched_3d,
                         std::vector<uchar> &status,
                         Eigen::Vector3d &PnP_T_old, Eigen::Matrix3d &PnP_R_old)
{
    cv::Mat r, rvec, t, D, tmp_r;
    cv::Mat K = (cv::Mat_<double>(3, 3) << 1.0, 0, 0, 0, 1.0, 0, 0, 0, 1.0);
    
	Matrix3d R_w_c = origin_vio_R;
    Vector3d T_w_c = origin_vio_T;

    Matrix3d R_inital = R_w_c.inverse();
    Vector3d P_inital = -(R_inital * T_w_c);

    cv::eigen2cv(R_inital, tmp_r);
    cv::Rodrigues(tmp_r, rvec);
    cv::eigen2cv(P_inital, t);

    cv::Mat inliers;
    TicToc t_pnp_ransac;

    if (CV_MAJOR_VERSION < 3)
        solvePnPRansac(matched_3d, matched_2d_old_norm, K, D, rvec, t, true, 100, 10.0 / 460.0, 100, inliers);
    else
    {
        if (CV_MINOR_VERSION < 2)
            solvePnPRansac(matched_3d, matched_2d_old_norm, K, D, rvec, t, true, 100, sqrt(10.0 / 460.0), 0.99, inliers);
        else
            solvePnPRansac(matched_3d, matched_2d_old_norm, K, D, rvec, t, true, 100, 10.0 / 460.0, 0.99, inliers);

    }

    for (int i = 0; i < (int)matched_2d_old_norm.size(); i++)
        status.push_back(0);

    for( int i = 0; i < inliers.rows; i++){
        int n = inliers.at<int>(i);
        status[n] = 1;
    }

    cv::Rodrigues(rvec, r);

    Matrix3d R_pnp;
    cv::cv2eigen(r, R_pnp);
    PnP_R_old = R_pnp.transpose();

    Vector3d T_pnp;
    cv::cv2eigen(t, T_pnp);
    PnP_T_old = PnP_R_old * (-T_pnp);
}

bool KeyFrame::PnPRANSAC_Relative(const vector<cv::Point2f> &matched_2d_old_norm,
                         const std::vector<cv::Point3f> &matched_3d,
                         std::vector<uchar> &status,
                         Eigen::Vector3d &PnP_T_old, Eigen::Matrix3d &PnP_R_old)
{
    cv::Mat r, rvec, t, D, tmp_r;
    cv::Mat K = (cv::Mat_<double>(3, 3) << 1.0, 0, 0, 0, 1.0, 0, 0, 0, 1.0);

    Matrix3d R_w_c = origin_vio_R;
    Vector3d T_w_c = origin_vio_T;

    Matrix3d R_inital = R_w_c.inverse();
    Vector3d P_inital = -(R_inital * T_w_c);

    cv::eigen2cv(R_inital, tmp_r);
    cv::Rodrigues(tmp_r, rvec);
    cv::eigen2cv(P_inital, t);

    cv::Mat inliers;
    TicToc t_pnp_ransac;

    bool status1 = true;
    LOGI("matched_2d_old_norm, %d", matched_2d_old_norm.size());
    LOGI("matched_3d %d", matched_3d.size());

    status1 = solvePnPRansac(matched_3d, matched_2d_old_norm, K, D, rvec, t, true, 100, 0.1, 0.99, inliers);
    
    if(!status1)
        return false;

    LOGI("inliers size: %d x %d", inliers.rows, inliers.cols);
    for (int i = 0; i < (int)matched_2d_old_norm.size(); i++)
        status.push_back(0);

    for( int i = 0; i < inliers.rows; i++){
        int n = inliers.at<int>(i);
        status[n] = 1;
    }

    cv::Rodrigues(rvec, r);

    Matrix3d R_pnp;
    cv::cv2eigen(r, R_pnp);
    PnP_R_old = R_pnp.transpose();

    Vector3d T_pnp;
    cv::cv2eigen(t, T_pnp);
    PnP_T_old = PnP_R_old * (-T_pnp);

    return true;
}

bool KeyFrame::findConnection(KeyFramePtr old_kf){
	TicToc tmp_t;
	
	vector<cv::Point2f> matched_2d_cur, matched_2d_old;
	vector<cv::Point2f> matched_2d_cur_norm, matched_2d_old_norm;
	vector<cv::Point3f> matched_3d;
	vector<double> matched_id;
	vector<uchar> status;

	matched_3d = point_3d;
	matched_2d_cur = point_2d_uv;
	matched_2d_cur_norm = point_2d_norm;
	matched_id = point_id;

	TicToc t_match;

	searchByBRIEFDes(matched_2d_old, matched_2d_old_norm, status, old_kf->brief_descriptors, old_kf->keypoints, old_kf->keypoints_norm);
	reduceVector(matched_2d_cur, status);
	reduceVector(matched_2d_old, status);
	reduceVector(matched_2d_cur_norm, status);
	reduceVector(matched_2d_old_norm, status);
	reduceVector(matched_3d, status);
	reduceVector(matched_id, status);
	vector<uchar>().swap(status);
	
	Eigen::Vector3d PnP_T_old;
	Eigen::Matrix3d PnP_R_old;
	Eigen::Vector3d relative_t;
	Quaterniond relative_q;
	double relative_yaw;

	if ((int) matched_2d_cur.size() > MIN_LOOP_NUM){
	    PnPRANSAC(matched_2d_old_norm, matched_3d, status, PnP_T_old, PnP_R_old);
	    reduceVector(matched_2d_cur, status);
	    reduceVector(matched_2d_old, status);
	    reduceVector(matched_2d_cur_norm, status);
	    reduceVector(matched_2d_old_norm, status);
	    reduceVector(matched_3d, status);
	    reduceVector(matched_id, status);
	}

	if ((int) matched_2d_cur.size() > MIN_LOOP_NUM){
	    relative_t = PnP_R_old.transpose() * (origin_vio_T - PnP_T_old);
	    relative_q = PnP_R_old.transpose() * origin_vio_R;
	    relative_yaw = Utility::normalizeAngle(Utility::R2ypr(origin_vio_R).x() - Utility::R2ypr(PnP_R_old).x());

	    if (abs(relative_yaw) < 30.0 && relative_t.norm() < 20.0){
	    	has_loop = true;
	    	loop_index = old_kf->index;
	    	loop_info << relative_t.x(), relative_t.y(), relative_t.z(),
	    	             relative_q.w(), relative_q.x(), relative_q.y(), relative_q.z(),
	    	             relative_yaw;

	        return true;
	    }
	}
	
	return false;
}

bool KeyFrame::findRelativePose(KeyFramePtr old_kf, Eigen::Vector3d &relative_t, Eigen::Quaterniond &relative_q)
{
    vector<cv::Point2f> matched_2d_cur, matched_2d_old;
    vector<cv::Point2f> matched_2d_cur_norm, matched_2d_old_norm;
    vector<cv::Point3f> matched_3d;
    vector<double> matched_id;
    vector<uchar> status;

    matched_3d = point_3d;
    matched_2d_cur = point_2d_uv;
    matched_2d_cur_norm = point_2d_norm;
    matched_id = point_id;
    LOGI("matched_3d_before : %d", matched_3d.size());
    searchByBRIEFDes(matched_2d_old, matched_2d_old_norm, status, old_kf->brief_descriptors, old_kf->keypoints, old_kf->keypoints_norm);
    
    reduceVector(matched_2d_cur, status);
    reduceVector(matched_2d_old, status);
    reduceVector(matched_2d_cur_norm, status);
    reduceVector(matched_2d_old_norm, status);
    reduceVector(matched_3d, status);
    reduceVector(matched_id, status);
    status.clear();

    if ((int)matched_2d_cur.size() < MIN_LOOP_NUM)
        return false;

    Eigen::Vector3d PnP_T_old;
    Eigen::Matrix3d PnP_R_old;

    bool PnP_flag = PnPRANSAC_Relative(matched_2d_old_norm, matched_3d, status, PnP_T_old, PnP_R_old);
    if(!PnP_flag) return false;

    reduceVector(matched_2d_cur, status);
    reduceVector(matched_2d_old, status);
    reduceVector(matched_2d_cur_norm, status);
    reduceVector(matched_2d_old_norm, status);
    reduceVector(matched_3d, status);
    reduceVector(matched_id, status);

    relative_t = PnP_T_old;
    relative_q = Eigen::Quaterniond(PnP_R_old);

    return true;
}


int KeyFrame::HammingDis(const BRIEF::bitset &a, const BRIEF::bitset &b){
    BRIEF::bitset xor_of_bitset = a ^ b;
    int dis = xor_of_bitset.count();
    return dis;
}
