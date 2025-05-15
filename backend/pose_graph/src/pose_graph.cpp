#include "include/pose_graph.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

int ROW, COL;
std::string EXTERNAL_PATH;
std::string BRIEF_PATTERN_FILE;
Eigen::Vector3d tic;
Eigen::Matrix3d qic;

PoseGraph::PoseGraph(
        std::string& external_path, const std::string& brief_pattern_file,
        const std::string& vocabulary_file, const bool load_previous_pose_graph, double skip_dis, int row, int col):
        VOCABULARY_FILE(vocabulary_file),
        LOAD_PREVIOUS_POSE_GRAPH(load_previous_pose_graph),
        SKIP_DIS(skip_dis), frame_index(0)
{
    earliest_loop_index = -1;

    ROW = row;
    COL = col;
    EXTERNAL_PATH = external_path;
    BRIEF_PATTERN_FILE = brief_pattern_file;

    tic = Eigen::Vector3d(0, 0, 0);
    qic = Eigen::Matrix3d::Identity();

    t_drift = Eigen::Vector3d(0, 0, 0);
    yaw_drift = 0;
    r_drift = Eigen::Matrix3d::Identity();

    global_index = 0;
    sequence_cnt = 0;
    sequence_loop.push_back(0);
    base_sequence = 1;

    t_loopClosure = std::thread(&PoseGraph::loopClosure, this);
    t_optimization = std::thread(&PoseGraph::optimize4DoF, this);
}

PoseGraph::~PoseGraph()
{
    t_optimization.join();
    t_loopClosure.join();
}

void PoseGraph::loadVocabulary(AAssetManager* asset_manager){
    std::string internal_path = EXTERNAL_PATH + "/" + VOCABULARY_FILE;

    AAsset* asset = AAssetManager_open(asset_manager, ("brief/" + VOCABULARY_FILE).c_str(), AASSET_MODE_STREAMING);
    if (!asset) throw std::runtime_error("Could not open asset: " + VOCABULARY_FILE);

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

    voc = new BriefVocabulary(internal_path);
    db.setVocabulary(*voc, false, 0);
}

int PoseGraph::getKeyFrameListSize(){
    int size = 0;

    m_keyframelist.lock();
    size = keyframelist.size();
    m_keyframelist.unlock();

    return size;
}

pair<float, float> PoseGraph::getLastElementOfKeyFrameList() {
    pair<float, float> ret = {0., 0.};

    while(!m_keyframelist.try_lock());

    if (!keyframelist.empty()){
        ret.first = keyframelist.back()->T_w_i.x();
        ret.second = keyframelist.back()->T_w_i.z();

        labeled_index.push_back(keyframelist.back()->index);
    }
    
    m_keyframelist.unlock();

    return ret;
}

void PoseGraph::addKeyFrameBuf(KeyFramePtr keyframe){
    m_buf.lock();
    keyframe_buf.push(keyframe);
    m_buf.unlock();
}

void PoseGraph::addKeyFrame(KeyFramePtr cur_kf, bool flag_detect_loop){
    if (sequence_cnt != cur_kf->sequence) {
        sequence_cnt++;
        sequence_loop.push_back(0);

        m_drift.lock();
        t_drift = Eigen::Vector3d(0, 0, 0);
        r_drift = Eigen::Matrix3d::Identity();
        m_drift.unlock();
    }

    cur_kf->index = global_index;
    global_index++;

    int loop_index = -1;
    if (flag_detect_loop){
        TicToc tmp_t;
        loop_index = detectLoop(cur_kf, cur_kf->index);
    }

    else {
        addKeyFrameIntoVoc(cur_kf);
    }

    if (loop_index != -1){
        KeyFramePtr old_kf = getKeyFrame(loop_index);

        if (cur_kf->findConnection(old_kf)){
            if (earliest_loop_index > loop_index || earliest_loop_index == -1)
                earliest_loop_index = loop_index;

            Vector3d w_P_old, w_P_cur, vio_P_cur;
            Matrix3d w_R_old, w_R_cur, vio_R_cur;
            old_kf->getVioPose(w_P_old, w_R_old);
            cur_kf->getVioPose(vio_P_cur, vio_R_cur);

            Vector3d relative_t = cur_kf->getLoopRelativeT();
            Quaterniond relative_q = cur_kf->getLoopRelativeQ();

            w_P_cur = w_R_old * relative_t + w_P_old;
            w_R_cur = w_R_old * relative_q;

            double shift_yaw = Utility::R2ypr(w_R_cur).x() - Utility::R2ypr(vio_R_cur).x();
            Matrix3d shift_r = Utility::ypr2R(Vector3d(shift_yaw, 0, 0));
            Vector3d shift_t = w_P_cur - w_R_cur * vio_R_cur.transpose() * vio_P_cur;

            if (old_kf->sequence != cur_kf->sequence && sequence_loop[cur_kf->sequence] == 0){
                vio_P_cur = shift_r * vio_P_cur + shift_t;
                vio_R_cur = shift_r *  vio_R_cur;
                cur_kf->updateVioPose(vio_P_cur, vio_R_cur);

                list<KeyFramePtr>::iterator it = keyframelist.begin();
                for (; it != keyframelist.end(); it++){
                    if((*it)->sequence == cur_kf->sequence){
                        Vector3d vio_P_cur;
                        Matrix3d vio_R_cur;
                        (*it)->getVioPose(vio_P_cur, vio_R_cur);
                        vio_P_cur = shift_r * vio_P_cur + shift_t;
                        vio_R_cur = shift_r *  vio_R_cur;
                        (*it)->updateVioPose(vio_P_cur, vio_R_cur);
                    }
                }
                sequence_loop[cur_kf->sequence] = 1;
            }
            
            m_optimize_buf.lock();
            optimize_buf.push(cur_kf->index);
            
            m_optimize_buf.unlock();
        }
    }
    
    m_keyframelist.lock();

    Vector3d P;
    Matrix3d R;
    cur_kf->getVioPose(P, R);
    P = r_drift * P + t_drift;
    R = r_drift * R;
    cur_kf->updatePose(P, R);

    keyframelist.push_back(cur_kf);
    m_keyframelist.unlock();
}

void PoseGraph::loadKeyFrame(KeyFramePtr cur_kf, bool flag_detect_loop)
{
    cur_kf->index = global_index;
    global_index++;

    int loop_index = -1;
    if (flag_detect_loop)
        loop_index = detectLoop(cur_kf, cur_kf->index);

    else
        addKeyFrameIntoVoc(cur_kf);

    if (loop_index != -1)
    {
        printf(" %d detect loop with %d \n", cur_kf->index, loop_index);
        KeyFramePtr old_kf = getKeyFrame(loop_index);
//        if (cur_kf->findConnection(old_kf))
//        {
//            if (earliest_loop_index > loop_index || earliest_loop_index == -1)
//                earliest_loop_index = loop_index;
//
        m_optimize_buf.lock();
//            optimize_buf.push(cur_kf->index);
        m_optimize_buf.unlock();
//        }
    }
    
    m_keyframelist.lock();
    keyframelist.push_back(cur_kf);
    m_keyframelist.unlock();
}

KeyFramePtr PoseGraph::getKeyFrame(int index)
{
    list<KeyFramePtr>::iterator it = keyframelist.begin();
    for (; it != keyframelist.end(); it++)
    {
        if((*it)->index == index)
            break;
    }
    if (it != keyframelist.end())
        return *it;
    else
        return NULL;
}

int PoseGraph::detectLoop(KeyFramePtr keyframe, int frame_index)
{
    TicToc tmp_t;
    //first query; then add this frame into database!
    QueryResults ret;
    TicToc t_query;

//    assert(keyframe->brief_descriptors.size() != 0);

    // @TODO 마지막 인자에 값 빼는 것 잊지 않기(원래 50을 뺐음)
    db.query(keyframe->brief_descriptors, ret, 4, frame_index);
    db.add(keyframe->brief_descriptors);
    bool find_loop = false;
    // // a good match with its nerghbour
    if (ret.size() >= 1 && ret[0].Score > 0.05)
    {
        for (unsigned int i = 1; i < ret.size(); i++)
        {
            //if (ret[i].Score > ret[0].Score * 0.3)
            if (ret[i].Score > 0.015)
            {
                find_loop = true;
            }
        }
    }

    if (find_loop && frame_index > 10)
    {
        int min_index = -1;
        for (unsigned int i = 0; i < ret.size(); i++)
        {
            if (min_index == -1 || (ret[i].Id < min_index && ret[i].Score > 0.015))
                min_index = ret[i].Id;
        }
        return min_index;
    }

    else
        return -1;

}

void PoseGraph::addKeyFrameIntoVoc(KeyFramePtr keyframe)
{
    // 돌려줘
    db.add(keyframe->brief_descriptors);
}

void PoseGraph::optimize4DoF()
{
    while(true)
    {
        int cur_index = -1;
        int first_looped_index = -1;
        m_optimize_buf.lock();
        while(!optimize_buf.empty())
        {
            cur_index = optimize_buf.front();
            first_looped_index = earliest_loop_index;
            optimize_buf.pop();
        }
    
        m_optimize_buf.unlock();

        if (cur_index != -1)
        {
            LOGI("optimize pose graph \n");

            m_keyframelist.lock();
            KeyFramePtr cur_kf = getKeyFrame(cur_index);

            int max_length = cur_index + 1;

            // w^t_i   w^q_i
            double t_array[max_length][3];
            Quaterniond q_array[max_length];
            double euler_array[max_length][3];
            double sequence_array[max_length];

            ceres::Problem problem;
            ceres::Solver::Options options;
            options.linear_solver_type = ceres::SPARSE_NORMAL_CHOLESKY;
            options.max_num_iterations = 5;
            ceres::Solver::Summary summary;
            ceres::LossFunction *loss_function;
            loss_function = new ceres::HuberLoss(0.1);
            ceres::LocalParameterization* angle_local_parameterization =
                    AngleLocalParameterization::Create();

            list<KeyFramePtr>::iterator it;

            int i = 0;
            for (it = keyframelist.begin(); it != keyframelist.end(); it++)
            {
                if ((*it)->index < first_looped_index)
                    continue;
                (*it)->local_index = i;
                Quaterniond tmp_q;
                Matrix3d tmp_r;
                Vector3d tmp_t;
                (*it)->getVioPose(tmp_t, tmp_r);
                tmp_q = tmp_r;
                t_array[i][0] = tmp_t(0);
                t_array[i][1] = tmp_t(1);
                t_array[i][2] = tmp_t(2);
                q_array[i] = tmp_q;

                Vector3d euler_angle = Utility::R2ypr(tmp_q.toRotationMatrix());
                euler_array[i][0] = euler_angle.x();
                euler_array[i][1] = euler_angle.y();
                euler_array[i][2] = euler_angle.z();

                sequence_array[i] = (*it)->sequence;

                problem.AddParameterBlock(euler_array[i], 1, angle_local_parameterization);
                problem.AddParameterBlock(t_array[i], 3);

                if ((*it)->index == first_looped_index || (*it)->sequence == 0)
                {
                    problem.SetParameterBlockConstant(euler_array[i]);
                    problem.SetParameterBlockConstant(t_array[i]);
                }

                //add edge
                for (int j = 1; j < 5; j++)
                {
                    if (i - j >= 0 && sequence_array[i] == sequence_array[i-j])
                    {
                        Vector3d euler_conncected = Utility::R2ypr(q_array[i-j].toRotationMatrix());
                        Vector3d relative_t(t_array[i][0] - t_array[i-j][0], t_array[i][1] - t_array[i-j][1], t_array[i][2] - t_array[i-j][2]);
                        relative_t = q_array[i-j].inverse() * relative_t;
                        double relative_yaw = euler_array[i][0] - euler_array[i-j][0];
                        ceres::CostFunction* cost_function = FourDOFAnalyticError::Create( relative_t.x(), relative_t.y(), relative_t.z(),
                                                                                           relative_yaw, euler_conncected.y(), euler_conncected.z());
                        problem.AddResidualBlock(cost_function, NULL, euler_array[i-j],
                                                 t_array[i-j],
                                                 euler_array[i],
                                                 t_array[i]);
                    }
                }

                //add loop edge

                if((*it)->has_loop)
                {
                    assert((*it)->loop_index >= first_looped_index);
                    int connected_index = getKeyFrame((*it)->loop_index)->local_index;
                    Vector3d euler_conncected = Utility::R2ypr(q_array[connected_index].toRotationMatrix());
                    Vector3d relative_t;
                    relative_t = (*it)->getLoopRelativeT();
                    double relative_yaw = (*it)->getLoopRelativeYaw();
                    ceres::CostFunction* cost_function = FourDOFAnalyticError::Create( relative_t.x(), relative_t.y(), relative_t.z(),
                                                                                       relative_yaw, euler_conncected.y(), euler_conncected.z());
                    problem.AddResidualBlock(cost_function, loss_function, euler_array[connected_index],
                                             t_array[connected_index],
                                             euler_array[i],
                                             t_array[i]);

                }

                if ((*it)->index == cur_index)
                    break;
                i++;
            }
            m_keyframelist.unlock();

            ceres::Solve(options, &problem, &summary);

            m_keyframelist.lock();

            i = 0;
            for (it = keyframelist.begin(); it != keyframelist.end(); it++)
            {
                if ((*it)->index < first_looped_index)
                    continue;
                Quaterniond tmp_q;
                tmp_q = Utility::ypr2R(Vector3d(euler_array[i][0], euler_array[i][1], euler_array[i][2]));
                Vector3d tmp_t = Vector3d(t_array[i][0], t_array[i][1], t_array[i][2]);
                Matrix3d tmp_r = tmp_q.toRotationMatrix();
                (*it)-> updatePose(tmp_t, tmp_r);

                if ((*it)->index == cur_index)
                    break;
                i++;
            }

            Vector3d cur_t, vio_t;
            Matrix3d cur_r, vio_r;
            cur_kf->getPose(cur_t, cur_r);
            cur_kf->getVioPose(vio_t, vio_r);

            m_drift.lock();
            yaw_drift = Utility::R2ypr(cur_r).x() - Utility::R2ypr(vio_r).x();
            r_drift = Utility::ypr2R(Vector3d(yaw_drift, 0, 0));
            t_drift = cur_t - r_drift * vio_t;
            m_drift.unlock();

            it++;
            for (; it != keyframelist.end(); it++)
            {
                Vector3d P;
                Matrix3d R;
                (*it)->getVioPose(P, R);
                P = r_drift * P + t_drift;
                R = r_drift * R;
                (*it)->updatePose(P, R);
            }
            m_keyframelist.unlock();
        }

        std::chrono::milliseconds dura(2000);
        std::this_thread::sleep_for(dura);
    }
}

void PoseGraph::new_sequence()
{
    printf("new sequence\n");
    sequence++;
    printf("sequence cnt %d \n", sequence);
    
    if (sequence > 5)
    {
        // @todo 이 부분은 안드로이드 logging 시스템 등으로 바꿔야 함.
        // ROS_WARN("only support 5 sequences since it's boring to copy code for more sequences.");
        // ROS_BREAK();
    }
    
    m_buf.lock();

    while(!keyframe_buf.empty())
        keyframe_buf.pop();
    
    m_buf.unlock();
}

void PoseGraph::loopClosure()
{
    while (true)
    {
        KeyFramePtr keyframe = NULL;
        
        m_buf.lock();
        
        if(!keyframe_buf.empty()){
            keyframe = keyframe_buf.front();
            keyframe_buf.pop();
        }
        
        m_buf.unlock();

        if (keyframe != NULL)
        {
            // skip fisrt few
            if (skip_first_cnt < SKIP_FIRST_CNT)
            {
                skip_first_cnt++;
                continue;
            }
            
            Eigen::Vector3d T;
            Eigen::Matrix3d R;
            keyframe->getPose(T, R);
            
            if((T - last_t).norm() > SKIP_DIS)
            {
                m_process.lock();
                keyframe->index = frame_index;
                addKeyFrame(keyframe, 1);
                m_process.unlock();
                frame_index++;
                last_t = T;
            }
        }

        std::chrono::milliseconds dura(5);
        std::this_thread::sleep_for(dura);
    }
}

void PoseGraph::savePoseGraph(const std::vector<std::string>& labels)
{
    m_keyframelist.lock();
    std::string save_folder = EXTERNAL_PATH + "/" + labels.back() + "/";

    FILE *pFile, *labeled_pFile;
    string file_path = save_folder + labels.back() + ".txt";
    pFile = fopen(file_path.c_str(), "w");
    assert(pFile != nullptr);

    string labeled_file_path = save_folder + "labeled_" + labels.back() + ".txt";
    labeled_pFile = fopen(labeled_file_path.c_str(), "w");
    assert(labeled_pFile != nullptr);

    fprintf(labeled_pFile, "{\n");

    list<KeyFramePtr>::iterator it;
    vector<int>::iterator it2;
    std::vector<std::string>::const_iterator it3 = labels.begin();
    for (it = keyframelist.begin(), it2 = labeled_index.begin(); it != keyframelist.end(); it++)
    {
        std::string descriptor_path, brief_path, keypoints_path, depth_img_path, img_path, confidence_img_path;
        Quaterniond VIO_tmp_Q{(*it)->vio_R_w_i};
        Quaterniond PG_tmp_Q{(*it)->R_w_i};
        Vector3d VIO_tmp_T = (*it)->vio_T_w_i;
        Vector3d PG_tmp_T = (*it)->T_w_i;

        fprintf (pFile, " %d %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %d %f %f %f %f %f %f %f %f %d\n",(*it)->index, (*it)->time_stamp,
                 VIO_tmp_T.x(), VIO_tmp_T.y(), VIO_tmp_T.z(),
                 PG_tmp_T.x(), PG_tmp_T.y(), PG_tmp_T.z(),
                 VIO_tmp_Q.w(), VIO_tmp_Q.x(), VIO_tmp_Q.y(), VIO_tmp_Q.z(),
                 PG_tmp_Q.w(), PG_tmp_Q.x(), PG_tmp_Q.y(), PG_tmp_Q.z(),
                 (*it)->loop_index,
                 (*it)->loop_info(0), (*it)->loop_info(1), (*it)->loop_info(2), (*it)->loop_info(3),
                 (*it)->loop_info(4), (*it)->loop_info(5), (*it)->loop_info(6), (*it)->loop_info(7),
                 (int)(*it)->keypoints.size());

        if(it2 != labeled_index.end() && (*it)->index == *it2){
            fprintf (labeled_pFile, "\t\"%s\": [%f %f]\n", it3->c_str(), PG_tmp_T.x(), PG_tmp_T.z());
            it2++;
            it3++;
        }

        img_path = save_folder + to_string((*it)->index) + "_img.jpg";
        cv::imwrite(img_path, (*it)->image);

        assert((*it)->keypoints.size() == (*it)->brief_descriptors.size());
        brief_path = save_folder + to_string((*it)->index) + "_briefdes.dat";
        std::ofstream brief_file(brief_path, std::ios::binary);

        keypoints_path = save_folder + to_string((*it)->index) + "_keypoints.txt";
        FILE *keypoints_file;
        keypoints_file = fopen(keypoints_path.c_str(), "w");
        for (int i = 0; i < (int)(*it)->keypoints.size(); i++)
        {
            brief_file << (*it)->brief_descriptors[i] << endl;
            fprintf(keypoints_file, "%f %f %f %f\n", (*it)->keypoints[i].pt.x, (*it)->keypoints[i].pt.y,
                    (*it)->keypoints_norm[i].pt.x, (*it)->keypoints_norm[i].pt.y);
        }
        brief_file.close();
        fclose(keypoints_file);
    }

    fprintf(labeled_pFile, "}");

    fclose(labeled_pFile);
    fclose(pFile);

    m_keyframelist.unlock();
}

void PoseGraph::loadPoseGraph()
{
    TicToc tmp_t;
    FILE * pFile;
    string file_path = EXTERNAL_PATH + "/pose_graph.txt";
    printf("lode pose graph from: %s \n", file_path.c_str());
    printf("pose graph loading...\n");
    pFile = fopen (file_path.c_str(),"r");
    if (pFile == NULL)
    {
        LOGI("lode previous pose graph error: wrong previous pose graph path or no previous pose graph \n the system will start with new pose graph \n");
        printf("lode previous pose graph error: wrong previous pose graph path or no previous pose graph \n the system will start with new pose graph \n");
        return;
    }
    int index;
    double time_stamp;
    double VIO_Tx, VIO_Ty, VIO_Tz;
    double PG_Tx, PG_Ty, PG_Tz;
    double VIO_Qw, VIO_Qx, VIO_Qy, VIO_Qz;
    double PG_Qw, PG_Qx, PG_Qy, PG_Qz;
    double loop_info_0, loop_info_1, loop_info_2, loop_info_3;
    double loop_info_4, loop_info_5, loop_info_6, loop_info_7;
    int loop_index;
    int keypoints_num;
    Eigen::Matrix<double, 8, 1 > loop_info;
    int cnt = 0;
    while (fscanf(pFile,"%d %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %d %lf %lf %lf %lf %lf %lf %lf %lf %d", &index, &time_stamp,
                  &VIO_Tx, &VIO_Ty, &VIO_Tz,
                  &PG_Tx, &PG_Ty, &PG_Tz,
                  &VIO_Qw, &VIO_Qx, &VIO_Qy, &VIO_Qz,
                  &PG_Qw, &PG_Qx, &PG_Qy, &PG_Qz,
                  &loop_index,
                  &loop_info_0, &loop_info_1, &loop_info_2, &loop_info_3,
                  &loop_info_4, &loop_info_5, &loop_info_6, &loop_info_7,
                  &keypoints_num) != EOF)
    {
        /*
        printf("I read: %d %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %d %lf %lf %lf %lf %lf %lf %lf %lf %d\n", index, time_stamp,
                                    VIO_Tx, VIO_Ty, VIO_Tz,
                                    PG_Tx, PG_Ty, PG_Tz,
                                    VIO_Qw, VIO_Qx, VIO_Qy, VIO_Qz,
                                    PG_Qw, PG_Qx, PG_Qy, PG_Qz,
                                    loop_index,
                                    loop_info_0, loop_info_1, loop_info_2, loop_info_3,
                                    loop_info_4, loop_info_5, loop_info_6, loop_info_7,
                                    keypoints_num);
        */
        cv::Mat image;
        std::string image_path, descriptor_path;

        Vector3d VIO_T(VIO_Tx, VIO_Ty, VIO_Tz);
        Vector3d PG_T(PG_Tx, PG_Ty, PG_Tz);
        Quaterniond VIO_Q;
        VIO_Q.w() = VIO_Qw;
        VIO_Q.x() = VIO_Qx;
        VIO_Q.y() = VIO_Qy;
        VIO_Q.z() = VIO_Qz;
        Quaterniond PG_Q;
        PG_Q.w() = PG_Qw;
        PG_Q.x() = PG_Qx;
        PG_Q.y() = PG_Qy;
        PG_Q.z() = PG_Qz;
        Matrix3d VIO_R, PG_R;
        VIO_R = VIO_Q.toRotationMatrix();
        PG_R = PG_Q.toRotationMatrix();
        Eigen::Matrix<double, 8, 1 > loop_info;
        loop_info << loop_info_0, loop_info_1, loop_info_2, loop_info_3, loop_info_4, loop_info_5, loop_info_6, loop_info_7;

        if (loop_index != -1)
            if (earliest_loop_index > loop_index || earliest_loop_index == -1)
            {
                earliest_loop_index = loop_index;
            }

        // load keypoints, brief_descriptors
        string brief_path = EXTERNAL_PATH + "/" + to_string(index) + "_briefdes.dat";
        std::ifstream brief_file(brief_path, std::ios::binary);
        string keypoints_path = EXTERNAL_PATH + "/" + to_string(index) + "_keypoints.txt";
        FILE *keypoints_file;
        keypoints_file = fopen(keypoints_path.c_str(), "r");
        vector<cv::KeyPoint> keypoints;
        vector<cv::KeyPoint> keypoints_norm;
        vector<BRIEF::bitset> brief_descriptors;
        for (int i = 0; i < keypoints_num; i++)
        {
            BRIEF::bitset tmp_des;
            brief_file >> tmp_des;
            brief_descriptors.push_back(tmp_des);
            cv::KeyPoint tmp_keypoint;
            cv::KeyPoint tmp_keypoint_norm;
            double p_x, p_y, p_x_norm, p_y_norm;
            if(!fscanf(keypoints_file,"%lf %lf %lf %lf", &p_x, &p_y, &p_x_norm, &p_y_norm))
                printf(" fail to load pose graph \n");
            tmp_keypoint.pt.x = p_x;
            tmp_keypoint.pt.y = p_y;
            tmp_keypoint_norm.pt.x = p_x_norm;
            tmp_keypoint_norm.pt.y = p_y_norm;
            keypoints.push_back(tmp_keypoint);
            keypoints_norm.push_back(tmp_keypoint_norm);
        }
        brief_file.close();
        fclose(keypoints_file);

        KeyFramePtr keyframe = std::make_shared<KeyFrame>(time_stamp, index, VIO_T, VIO_R, PG_T, PG_R, image, loop_index, loop_info, keypoints, keypoints_norm, brief_descriptors);
        loadKeyFrame(keyframe, 0);
        cnt++;
    }
    fclose (pFile);
    printf("load pose graph time: %f s\n", tmp_t.toc()/1000);
    base_sequence = 0;
}

void PoseGraph::setIntrinsicParam(double fx, double fy, double cx, double cy)
{
    K = (cv::Mat_<double>(3, 3) << fx, 0, cx, 0, fy, cy, 0, 0, 1.0);
}


bool PoseGraph::InitialPose(KeyFramePtr init_frame)
{
    if(LOAD_PREVIOUS_POSE_GRAPH)
    {
        loadPoseGraph();
        LOAD_PREVIOUS_POSE_GRAPH = false;
    }

    if(keyframelist.size() == 0) return false;

    //2. brief 매칭
    QueryResults ret;
    db.query(init_frame->brief_descriptors, ret, 1, 15);
    LOGI("열심히 찾고는 있는데 ㅠㅠ");
    if(ret.size() == 0 ) return false;
    if(ret[0].Score < 0.015) return false;
    idx = ret[0].Id;
    
    auto it = keyframelist.begin();
    std::advance(it, idx);

    //3. PnP-RANSAC
    Eigen::Vector3d rel_t;
    Eigen::Quaterniond rel_q;
    KeyFramePtr map_frame = *it;
    if (max_count < 10000)
    {
        bool check = init_frame->findRelativePose(map_frame, rel_t, rel_q);
        if(!check) return false;
        max_count++;
        LOGI("Estimated Pose in Map: t = [%.2f %.2f %.2f], q = [%.2f %.2f %.2f %.2f]",
             rel_t.x(), rel_t.y(), rel_t.z(), rel_q.w(), rel_q.x(), rel_q.y(), rel_q.z());
        if(rel_t.norm() > 0.3)
            return false;
    } 
    else 
    {
        LOGI("Pose estimation failed");
        Eigen::Vector3d t_map = map_frame->vio_T_w_i;
        x = t_map.x();
        z = t_map.z();
        return true;
    }

    //4. 상대 위치 정보
    Eigen::Vector3d t_map = map_frame->vio_T_w_i;
    // LOGI("이것은 입에서 나는 소리가 아니여 x, x, z, z, %.2f %.2f %.2f %.2f", t_map.x(), rel_t.x(), t_map.z(), rel_t.z());
    x = t_map.x() + rel_t.x();
    z = t_map.z() + rel_t.z();
    // LOGI("이것은 입에서 나는 소리여 x, z, %.2f %.2f", x, z);

    // LOGI("아이고 변환했슈");
    Eigen::Matrix3d R_w_m = map_frame->vio_R_w_i;
    // t_w_m: map_frame의 위치
    Quaterniond R_w_m_q = Eigen::Quaterniond(R_w_m);
    Eigen::Vector3d t_w_m = map_frame->vio_T_w_i;
    Eigen::Vector3d t_w_c = -R_w_m * rel_t + t_w_m;
    Eigen::Quaterniond R_w_c = R_w_m_q * rel_q;
    
    x = t_w_c.x();
    z = t_w_c.z();
    // LOGI("이것은 입에서 나는 소리여 x, z, %.2f %.2f", x, z);
    return true;
}