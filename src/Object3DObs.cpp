#include "opencv2/core/core.hpp"
#include <iostream>
#include <opencv2/aruco/charuco.hpp>
#include <opencv2/opencv.hpp>
#include <stdio.h>

#include "Camera.hpp"
#include "Object3DObs.hpp"
#include "geometrytools.hpp"
#include "logger.h"

Object3DObs::Object3DObs() {}

/**
 * @brief initialize the object in the observation (what board is observed)
 *
 * @param new_obj_obs object to be added
 * @param object_idx object index
 */
void Object3DObs::initializeObject(std::shared_ptr<Object3D> obj_obs,
                                   int object_idx) {
  object_3d_ = obj_obs;
  object_3d_id_ = object_idx;
}

/**
 * @brief Insert a new board in the object
 *
 * @note Do not forget to initialize the object_3d_ first.
 *
 * @param new_board pointer to the new board observation
 */
void Object3DObs::insertNewBoardObs(std::shared_ptr<BoardObs> new_board_obs) {
  board_id_.push_back(new_board_obs->board_id_);
  cam_ = new_board_obs->cam_;
  camera_id_ = new_board_obs->camera_id_;
  frame_id_ = new_board_obs->frame_id_;
  board_observations_[board_observations_.size()] = new_board_obs;

  // push the 2d pts and index
  for (int i = 0; i < new_board_obs->pts_2d_.size(); i++) {
    // Convert the index from the board to the object
    std::pair<int, int> board_id_pts_id =
        std::make_pair(new_board_obs->board_id_, new_board_obs->charuco_id_[i]);
    int pts_idx_obj = object_3d_.lock()->pts_board_2_obj_[board_id_pts_id];
    pts_2d_.push_back(new_board_obs->pts_2d_[i]);
    pts_id_.push_back(pts_idx_obj);
  }
}

Object3DObs::~Object3DObs() {
  delete[] pose_;
  delete[] group_pose_;
}

/**
 * @brief Get the pose of the object w.r.t. the camera
 *
 * @param r_vec return by reference Rodrigues rotation vector
 * @param t_vec return by reference the translation vector
 */
void Object3DObs::getPoseVec(cv::Mat &r_vec, cv::Mat &t_vec) {
  cv::Mat rot_v = cv::Mat::zeros(3, 1, CV_64F);
  cv::Mat trans_v = cv::Mat::zeros(3, 1, CV_64F);
  rot_v.at<double>(0) = pose_[0];
  rot_v.at<double>(1) = pose_[1];
  rot_v.at<double>(2) = pose_[2];
  trans_v.at<double>(0) = pose_[3];
  trans_v.at<double>(1) = pose_[4];
  trans_v.at<double>(2) = pose_[5];
  rot_v.copyTo(r_vec);
  trans_v.copyTo(t_vec);
}

/**
 * @brief Get the matrix pose of the object w.r.t. the camera
 *
 * @return 4x4 pose matrix
 */
cv::Mat Object3DObs::getPoseMat() {
  cv::Mat r_vec;
  cv::Mat t_vec;
  getPoseVec(r_vec, t_vec);
  cv::Mat pose = RVecT2Proj(r_vec, t_vec);
  return pose;
}

/**
 * @brief Get rotation vector of the object w.r.t. the camera
 *
 * @return 1x3 Rodrigues vector
 */
cv::Mat Object3DObs::getRotVec() {
  cv::Mat r_vec;
  cv::Mat t_vec;
  getPoseVec(r_vec, t_vec);
  return r_vec;
}

/**
 * @brief Get translation vector of the object w.r.t. the camera
 *
 * @return 1x3 translation vector
 */
cv::Mat Object3DObs::getTransVec() {
  cv::Mat r_vec;
  cv::Mat t_vec;
  getPoseVec(r_vec, t_vec);
  return t_vec;
}

/**
 * @brief Set pose of the object w.r.t. the camera from pose matrix
 *
 * @param pose 4x4 pose matrix
 */
void Object3DObs::setPoseMat(cv::Mat pose) {
  cv::Mat r_vec, t_vec;
  Proj2RT(pose, r_vec, t_vec);
  pose_[0] = r_vec.at<double>(0);
  pose_[1] = r_vec.at<double>(1);
  pose_[2] = r_vec.at<double>(2);
  pose_[3] = t_vec.at<double>(0);
  pose_[4] = t_vec.at<double>(1);
  pose_[5] = t_vec.at<double>(2);
}

/**
 * @brief Set pose of the object w.r.t. the cam from rotation and translation
 * vectors
 *
 * @param r_vec Rodrigues rotation vector
 * @param t_vec translation vector
 */
void Object3DObs::setPoseVec(cv::Mat r_vec, cv::Mat t_vec) {
  pose_[0] = r_vec.at<double>(0);
  pose_[1] = r_vec.at<double>(1);
  pose_[2] = r_vec.at<double>(2);
  pose_[3] = t_vec.at<double>(0);
  pose_[4] = t_vec.at<double>(1);
  pose_[5] = t_vec.at<double>(2);
}

/**
 * @brief Set the pose of the object in the referential of the group observing
 * the object
 *
 * @param pose 4x4 pose matrix
 */
void Object3DObs::setPoseInGroupMat(cv::Mat pose) {
  cv::Mat r_vec, t_vec;
  Proj2RT(pose, r_vec, t_vec);
  group_pose_[0] = r_vec.at<double>(0);
  group_pose_[1] = r_vec.at<double>(1);
  group_pose_[2] = r_vec.at<double>(2);
  group_pose_[3] = t_vec.at<double>(0);
  group_pose_[4] = t_vec.at<double>(1);
  group_pose_[5] = t_vec.at<double>(2);
}

/**
 * @brief Set the pose of the object in the referential of the group observing
 * the object (from rotation / translation vectors)
 *
 * @param r_vec Rodrigues rotation vector
 * @param t_vec translation vector
 */
void Object3DObs::setPoseInGroupVec(cv::Mat r_vec, cv::Mat t_vec) {
  group_pose_[0] = r_vec.at<double>(0);
  group_pose_[1] = r_vec.at<double>(1);
  group_pose_[2] = r_vec.at<double>(2);
  group_pose_[3] = t_vec.at<double>(0);
  group_pose_[4] = t_vec.at<double>(1);
  group_pose_[5] = t_vec.at<double>(2);
}

/**
 * @brief Get the pose of the object w.r.t. the camera group
 *
 * @param r_vec return by reference Rodrigues rotation vector
 * @param t_vec return by reference the translation vector
 */
void Object3DObs::getPoseInGroupVec(cv::Mat &r_vec, cv::Mat &t_vec) {
  cv::Mat rot_v = cv::Mat::zeros(3, 1, CV_64F);
  cv::Mat trans_v = cv::Mat::zeros(3, 1, CV_64F);
  rot_v.at<double>(0) = group_pose_[0];
  rot_v.at<double>(1) = group_pose_[1];
  rot_v.at<double>(2) = group_pose_[2];
  trans_v.at<double>(0) = group_pose_[3];
  trans_v.at<double>(1) = group_pose_[4];
  trans_v.at<double>(2) = group_pose_[5];
  rot_v.copyTo(r_vec);
  trans_v.copyTo(t_vec);
}

/**
 * @brief Get the matrix pose of the object w.r.t. the group of camera
 *
 * @return 4x4 pose matrix
 */
cv::Mat Object3DObs::getPoseInGroupMat() {
  cv::Mat r_vec;
  cv::Mat t_vec;
  getPoseInGroupVec(r_vec, t_vec);
  cv::Mat pose = RVecT2Proj(r_vec, t_vec);
  return pose;
}

/**
 * @brief Get rotation vector of the object w.r.t. the camera group
 *
 * @return 1x3 Rodrigues vector
 */
cv::Mat Object3DObs::getRotInGroupVec() {
  cv::Mat r_vec;
  cv::Mat t_vec;
  getPoseInGroupVec(r_vec, t_vec);
  return r_vec;
}

/**
 * @brief Get translation vector of the object w.r.t. the camera
 *
 * @return 1x3 translation vector
 */
cv::Mat Object3DObs::getTransInGroupVec() {
  cv::Mat r_vec;
  cv::Mat t_vec;
  getPoseInGroupVec(r_vec, t_vec);
  return t_vec;
}

/**
 * @brief Estimate the pose of the 3D object w.r.t. to the camera
 *
 * It using a PnP RANSAC algorithm.
 *
 */
void Object3DObs::estimatePose(double ransac_thresh) {
  std::vector<cv::Point3f> object_pts_temp;
  for (int i = 0; i < pts_id_.size(); i++) {
    object_pts_temp.push_back(object_3d_.lock()->pts_3d_[pts_id_[i]]);
  }

  // Estimate the pose using a RANSAC
  cv::Mat r_vec, t_vec;
  std::shared_ptr<Camera> cam_ptr = cam_.lock();
  cv::Mat inliers = ransacP3PDistortion(
      object_pts_temp, pts_2d_, cam_ptr->getCameraMat(),
      cam_ptr->getDistortionVectorVector(), r_vec, t_vec, ransac_thresh, 0.99,
      1000, true, cam_ptr->distortion_model_);
  LOG_DEBUG << "Trans :: " << t_vec << "       Rot :: " << r_vec;
  LOG_DEBUG << "input pts 3D :: " << object_pts_temp.size();
  LOG_DEBUG << "Inliers :: " << inliers.rows;
  r_vec.convertTo(r_vec, CV_64F);
  t_vec.convertTo(t_vec, CV_64F);
  setPoseVec(r_vec, t_vec);
}

/**
 * @brief Compute the reprojection error for this object observation
 *
 * @return mean reprojection error for this observation
 */
float Object3DObs::computeReprojectionError() {
  float sum_err_object = 0;
  std::vector<cv::Point3f> object_pts_temp;
  for (int i = 0; i < pts_id_.size(); i++)
    object_pts_temp.push_back(object_3d_.lock()->pts_3d_[pts_id_[i]]);

  // Project the 3D pts on the image
  std::vector<cv::Point2f> repro_pts;
  std::vector<float> error_object_vec;
  std::shared_ptr<Camera> cam_ptr = cam_.lock();
  projectPointsWithDistortion(object_pts_temp, getRotVec(), getTransVec(),
                              cam_ptr->getCameraMat(),
                              cam_ptr->getDistortionVectorVector(), repro_pts,
                              cam_ptr->distortion_model_);
  for (int j = 0; j < repro_pts.size(); j++) {
    float rep_err = sqrt(pow((pts_2d_[j].x - repro_pts[j].x), 2) +
                         pow((pts_2d_[j].y - repro_pts[j].y), 2));
    error_object_vec.push_back(rep_err);
    sum_err_object += rep_err;
    // if (rep_err > 6.0)
    // LOG_WARNING << "LARGE REPROJECTION ERROR ::: " << rep_err  ;
  }
  LOG_DEBUG << "Frame :: " << this->frame_id_
            << "  object :: " << this->object_3d_id_
            << "  --- Mean Error ::" << sum_err_object / error_object_vec.size()
            << "  Nb pts :: " << error_object_vec.size();

  // return mean error for the board
  return sum_err_object / error_object_vec.size();
}
