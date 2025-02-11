#include "opencv2/core/core.hpp"
#include <iostream>
#include <opencv2/opencv.hpp>
#include <stdio.h>

#include "geometrytools.hpp"
#include "logger.h"

// Tools for rotation and projection matrix
cv::Mat RT2Proj(cv::Mat R, cv::Mat T) {
  cv::Mat Proj = (cv::Mat_<double>(4, 4) << 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0,
                  0, 0, 0, 1);
  R.copyTo(Proj(cv::Range(0, 3), cv::Range(0, 3)));
  T.copyTo(Proj(cv::Range(0, 3), cv::Range(3, 4)));
  return Proj;
}

cv::Mat RVecT2Proj(cv::Mat RVec, cv::Mat T) {
  cv::Mat R;
  cv::Rodrigues(RVec, R);
  cv::Mat Proj = (cv::Mat_<double>(4, 4) << 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0,
                  0, 0, 0, 1);
  R.copyTo(Proj(cv::Range(0, 3), cv::Range(0, 3)));
  T.copyTo(Proj(cv::Range(0, 3), cv::Range(3, 4)));
  return Proj;
}

cv::Mat RVecT2ProjInt(cv::Mat RVec, cv::Mat T, cv::Mat K) {
  cv::Mat R;
  cv::Rodrigues(RVec, R);
  cv::Mat Proj = (cv::Mat_<double>(4, 4) << 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0,
                  0, 0, 0, 1);
  cv::Mat KR = K * R;
  cv::Mat KT = K * T;
  KR.copyTo(Proj(cv::Range(0, 3), cv::Range(0, 3)));
  KT.copyTo(Proj(cv::Range(0, 3), cv::Range(3, 4)));
  return Proj;
}

void Proj2RT(cv::Mat Proj, cv::Mat &R,
             cv::Mat &T) // Rodrigues and translation vector
{
  cv::Rodrigues(Proj(cv::Range(0, 3), cv::Range(0, 3)), R);
  T = Proj(cv::Range(0, 3), cv::Range(3, 4));
}

cv::Mat vectorProj(std::vector<float> ProjV) // R Rodrigues | T vector
{
  cv::Mat RV(1, 3, CV_64F);
  cv::Mat TV(3, 1, CV_64F);
  RV.at<double>(0) = (double)ProjV[0];
  RV.at<double>(1) = (double)ProjV[1];
  RV.at<double>(2) = (double)ProjV[2];
  cv::Mat R;
  cv::Rodrigues(RV, R);
  TV.at<double>(0) = (double)ProjV[3];
  TV.at<double>(1) = (double)ProjV[4];
  TV.at<double>(2) = (double)ProjV[5];
  cv::Mat Proj = (cv::Mat_<double>(4, 4) << 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0,
                  0, 0, 0, 1);
  R.copyTo(Proj(cv::Range(0, 3), cv::Range(0, 3)));
  TV.copyTo(Proj(cv::Range(0, 3), cv::Range(3, 4)));
  return Proj;
}

std::vector<float> ProjToVec(cv::Mat Proj) // Projection matrix to float vector
{
  cv::Mat R(1, 3, CV_64F);
  cv::Mat T(3, 1, CV_64F);
  cv::Rodrigues(Proj(cv::Range(0, 3), cv::Range(0, 3)), R);
  T = Proj(cv::Range(0, 3), cv::Range(3, 4));
  std::vector<float> Output;
  Output.push_back((float)R.at<double>(0));
  Output.push_back((float)R.at<double>(1));
  Output.push_back((float)R.at<double>(2));
  Output.push_back((float)T.at<double>(0));
  Output.push_back((float)T.at<double>(1));
  Output.push_back((float)T.at<double>(2));
  return Output;
}

// Invert vector representation
void invertRvecT(cv::Mat Rvec, cv::Mat T, cv::Mat &iR, cv::Mat &iT) {
  cv::Mat Proj = RVecT2Proj(Rvec, T);
  Proj = Proj.inv();
  Proj2RT(Proj, iR, iT);
}

void invertRvecT(cv::Mat &Rvec, cv::Mat &T) {
  cv::Mat Proj = RVecT2Proj(Rvec, T);
  Proj = Proj.inv();
  Proj2RT(Proj, Rvec, T);
}

// My SVD triangulation
// Triangulate a 3D point from multiple observations
cv::Point3f triangulateNViewLinearEigen(std::vector<cv::Point2f> Pts2D,
                                        std::vector<cv::Mat> RotationVec,
                                        std::vector<cv::Mat> TranslationVec,
                                        cv::Mat Intrinsic) {
  cv::Mat A; // Projection matrix
  for (int i = 0; i < RotationVec.size(); i++) {
    cv::Mat cam_temp =
        RVecT2ProjInt(RotationVec[i], TranslationVec[i], Intrinsic);
    float px = Pts2D[i].x;
    float py = Pts2D[i].y;
    cv::Mat M1 = cam_temp.row(0);
    cv::Mat M2 = cam_temp.row(1);
    cv::Mat M3 = cam_temp.row(2);
    A.push_back(px * M3 - M1);
    A.push_back(py * M3 - M2);
  }
  cv::Mat S, U, VT;
  SVDecomp(A, S, U, VT);
  cv::Mat PtsTri = VT.row(3);
  PtsTri.convertTo(PtsTri, CV_32F);
  cv::Point3f PtsTrip;
  PtsTrip.x = PtsTri.at<float>(0) / PtsTri.at<float>(3);
  PtsTrip.y = PtsTri.at<float>(1) / PtsTri.at<float>(3);
  PtsTrip.z = PtsTri.at<float>(2) / PtsTri.at<float>(3);
  return PtsTrip;
}

// Fit the line according to the point set ax + by + c = 0, res is the residual
void calcLinePara(std::vector<cv::Point2f> pts, double &a, double &b, double &c, double &res)
{
    res = 0;
    cv::Vec4f line;
    std::vector<cv::Point2f> ptsF;
    for (unsigned int i = 0; i < pts.size(); i++)
        ptsF.push_back(pts[i]);

    cv::fitLine(ptsF, line,cv::DistanceTypes::DIST_L2, 0, 1e-2, 1e-2);
    a = line[1];
    b = -line[0];
    c = line[0] * line[3] - line[1] * line[2];

    for (unsigned int i = 0; i < pts.size(); i++)
    {
        double resid_ = fabs(pts[i].x * a + pts[i].y * b + c);
        res += resid_;
    }
    res /= pts.size();
}

// RANSAC algorithm
// Return Inliers, p = proba (typical = 0.99), Output : Best Pts3D maximizing
// inliers, Thresh = reprojection tolerance in pixels, it = max iteration
void ransacTriangulation(std::vector<cv::Point2f> point2d,
                         std::vector<cv::Mat> rotation_vec,
                         std::vector<cv::Mat> translation_vec,
                         cv::Mat intrinsic, cv::Mat distortion_vector,
                         cv::Point3f &best_points3d, double thresh, double p,
                         int it) {
  // Init parameters
  int N = it;
  int trialcount = 0;
  cv::Mat InliersR;
  int countit = 0;
  int BestInNb = 0;
  double myepsilon = 0.00001; // small value for numerical problem

  // Vector of index to shuffle
  std::srand(unsigned(std::time(0)));
  std::vector<int> myvector;
  for (unsigned int i = 0; i < point2d.size(); ++i)
    myvector.push_back(i); // 1 2 3 4 5 6 7 8 9

  // Ransac iterations
  while (N > trialcount && countit < it) {
    // pick 2 points
    std::random_shuffle(myvector.begin(), myvector.end());
    std::vector<int> idx;
    idx.push_back(myvector[0]);
    idx.push_back(myvector[1]);
    idx.push_back(myvector[2]);
    idx.push_back(myvector[3]);

    // Select the corresponding sample of 2D pts and corresponding rot and trans
    std::vector<cv::Point2f> image2Pts;
    image2Pts.push_back(point2d[idx[0]]);
    image2Pts.push_back(point2d[idx[1]]);
    std::vector<cv::Mat> Rot2Pts;
    Rot2Pts.push_back(rotation_vec[idx[0]]);
    Rot2Pts.push_back(rotation_vec[idx[1]]);
    std::vector<cv::Mat> Trans2Pts;
    Trans2Pts.push_back(translation_vec[idx[0]]);
    Trans2Pts.push_back(translation_vec[idx[1]]);

    // Triangulate with these 2 pts
    cv::Point3f PtsTrip =
        triangulateNViewLinearEigen(image2Pts, Rot2Pts, Trans2Pts, intrinsic);

    // compute inliers
    cv::Mat Index;
    int NbInliers = 0;
    for (int k = 0; k < rotation_vec.size(); k++) {
      // Reproject points
      std::vector<cv::Point2f> reprojected_pts;
      std::vector<cv::Point3f> point3d_tmp;
      point3d_tmp.push_back(PtsTrip);
      projectPoints(point3d_tmp, rotation_vec[k], translation_vec[k], intrinsic,
                    distortion_vector, reprojected_pts);
      if (sqrt(pow((point2d[k].x - reprojected_pts[0].x), 2) +
               pow((point2d[k].y - reprojected_pts[0].y), 2)) < thresh) {
        Index.push_back(k);
        NbInliers++;
      }
    }
    trialcount++;

    // keep the best one
    if (NbInliers > BestInNb) {
      Index.copyTo(InliersR);
      BestInNb = NbInliers;
      best_points3d = PtsTrip;

      // with probability p, a data set with no outliers.
      double totalPts = point2d.size();
      double fracinliers = BestInNb / totalPts;
      double pNoOutliers = 1 - pow(fracinliers, 3);
      if (pNoOutliers == 0)
        pNoOutliers = myepsilon; // Avoid division by Inf
      if (pNoOutliers > (1 - myepsilon))
        pNoOutliers = 1 - myepsilon; // Avoid division by zero
      double tempest = log(1 - p) / log(pNoOutliers);
      N = int(round(tempest));
      trialcount = 0;
    }
    countit++;
  }
}

// RANSAC algorithm
// Return Inliers, p = proba (typical = 0.99), Output : Rot and Trans Mat,
// Thresh = reprojection tolerance in pixels, it = max iteration
cv::Mat ransacP3P(std::vector<cv::Point3f> scenePoints,
                  std::vector<cv::Point2f> imagePoints, cv::Mat Intrinsic,
                  cv::Mat Disto, cv::Mat &BestR, cv::Mat &BestT, double thresh,
                  double p, int it, bool refine) {

  // Init parameters
  int N = it;
  int trialcount = 0;
  cv::Mat InliersR;
  int countit = 0;
  int BestInNb = 0;
  double myepsilon = 0.00001; // small value for numerical problem
  cv::Mat Rot(1, 3, CV_64F);
  cv::Mat Trans(1, 3, CV_64F);

  // Vector of index to shuffle
  std::srand(unsigned(std::time(0)));
  std::vector<int> myvector;
  for (unsigned int i = 0; i < imagePoints.size(); ++i)
    myvector.push_back(i); // 1 2 3 4 5 6 7 8 9

  // Ransac iterations
  while (N > trialcount && countit < it) {

    // pick 4 points
    std::random_shuffle(myvector.begin(), myvector.end());
    std::vector<int> idx;
    idx.push_back(myvector[0]);
    idx.push_back(myvector[1]);
    idx.push_back(myvector[2]);
    idx.push_back(myvector[3]);

    // Select the corresponding sample
    std::vector<cv::Point3f> scenePoints3Pts;
    scenePoints3Pts.push_back(scenePoints[idx[0]]);
    scenePoints3Pts.push_back(scenePoints[idx[1]]);
    scenePoints3Pts.push_back(scenePoints[idx[2]]);
    scenePoints3Pts.push_back(scenePoints[idx[3]]);
    std::vector<cv::Point2f> imagePoints3Pts;
    imagePoints3Pts.push_back(imagePoints[idx[0]]);
    imagePoints3Pts.push_back(imagePoints[idx[1]]);
    imagePoints3Pts.push_back(imagePoints[idx[2]]);
    imagePoints3Pts.push_back(imagePoints[idx[3]]);

    // Apply P3P (fourth point for disambiguation)
    solvePnP(scenePoints3Pts, imagePoints3Pts, Intrinsic, Disto, Rot, Trans,
             false, 2); // CV_P3P = 2

    // Reproject points
    std::vector<cv::Point2f> reprojected_pts;
    projectPoints(scenePoints, Rot, Trans, Intrinsic, Disto, reprojected_pts);

    // compute inliers
    cv::Mat Index;
    int NbInliers = 0;
    for (int k = 0; k < (int)scenePoints.size(); k++) {
      if (sqrt(pow(imagePoints[k].x - reprojected_pts[k].x, 2) +
               pow(imagePoints[k].y - reprojected_pts[k].y, 2)) < thresh) {
        Index.push_back(k);
        NbInliers++;
      }
    }
    trialcount++;

    // keep the best one
    if (NbInliers > BestInNb) {
      Index.copyTo(InliersR);
      BestInNb = NbInliers;
      Trans.copyTo(BestT);
      Rot.copyTo(BestR);

      // with probability p, a data set with no outliers.
      double totalPts = scenePoints.size();
      double fracinliers = BestInNb / totalPts;
      double pNoOutliers = 1 - pow(fracinliers, 3);
      if (pNoOutliers == 0)
        pNoOutliers = myepsilon; // Avoid division by Inf
      if (pNoOutliers > (1 - myepsilon))
        pNoOutliers = 1 - myepsilon; // Avoid division by zero
      double tempest = log(1 - p) / log(pNoOutliers);
      N = int(round(tempest));
      trialcount = 0;
    }
    countit++;
  }

  if (refine == true & (BestInNb >= 4)) {
    std::vector<cv::Point3f> scenePointsInliers;
    std::vector<cv::Point2f> imagePointsInliers;

    for (int j = 0; j < BestInNb; j++) {
      imagePointsInliers.push_back(imagePoints[InliersR.at<int>(j)]);
      scenePointsInliers.push_back(scenePoints[InliersR.at<int>(j)]);
    }
    solvePnP(scenePointsInliers, imagePointsInliers, Intrinsic, Disto, BestR,
             BestT, true, 0); // CV_ITERATIVE = 0 non linear
  }
  return InliersR;
}

std::vector<cv::Point3f> transform3DPts(std::vector<cv::Point3f> pts3D,
                                        cv::Mat Rot, cv::Mat Trans) {
  cv::Mat RotM;
  cv::Rodrigues(Rot, RotM);
  double r11 = RotM.at<double>(0, 0);
  double r12 = RotM.at<double>(0, 1);
  double r13 = RotM.at<double>(0, 2);
  double r21 = RotM.at<double>(1, 0);
  double r22 = RotM.at<double>(1, 1);
  double r23 = RotM.at<double>(1, 2);
  double r31 = RotM.at<double>(2, 0);
  double r32 = RotM.at<double>(2, 1);
  double r33 = RotM.at<double>(2, 2);
  double tx = Trans.at<double>(0);
  double ty = Trans.at<double>(1);
  double tz = Trans.at<double>(2);

  for (int i = 0; i < pts3D.size(); i++) {
    float x = tx + r11 * pts3D[i].x + r12 * pts3D[i].y + r13 * pts3D[i].z;
    float y = ty + r21 * pts3D[i].x + r22 * pts3D[i].y + r23 * pts3D[i].z;
    float z = tz + r31 * pts3D[i].x + r32 * pts3D[i].y + r33 * pts3D[i].z;
    pts3D[i].x = x;
    pts3D[i].y = y;
    pts3D[i].z = z;
  }

  return pts3D;
}

/**
 * @brief Calibrate 2 cameras with handeye calibration
 *

 */
cv::Mat handeyeCalibration(std::vector<cv::Mat> pose_abs_1,
                           std::vector<cv::Mat> pose_abs_2) {

  // Prepare the poses for handeye calibration
  std::vector<cv::Mat> r_cam_group_1, t_cam_group_1, r_cam_group_2,
      t_cam_group_2;
  for (int i = 0; i < pose_abs_1.size(); i++) {
    // get the poses
    cv::Mat pose_cam_group_1 = pose_abs_1[i].inv();
    cv::Mat pose_cam_group_2 = pose_abs_2[i];

    // save in datastruct
    cv::Mat r_1, r_2, t_1, t_2;
    Proj2RT(pose_cam_group_1, r_1, t_1);
    Proj2RT(pose_cam_group_2, r_2, t_2);
    cv::Mat r_1_mat, r_2_mat;
    Rodrigues(r_1, r_1_mat);
    Rodrigues(r_2, r_2_mat);
    r_cam_group_1.push_back(r_1_mat);
    t_cam_group_1.push_back(t_1);
    r_cam_group_2.push_back(r_2_mat);
    t_cam_group_2.push_back(t_2);
  }

  // Hand-eye calibration
  cv::Mat r_g1_g2, t_g1_g2;
  cv::calibrateHandEye(r_cam_group_1, t_cam_group_1, r_cam_group_2,
                       t_cam_group_2, r_g1_g2, t_g1_g2,
                       cv::CALIB_HAND_EYE_HORAUD);
  cv::Mat pose_g1_g2 = RT2Proj(r_g1_g2, t_g1_g2);

  return pose_g1_g2;
}

/**
 * @brief Calibrate 2 cameras with handeye calibration
 *
 * In this function only N pairs of images are used
 * These pair of images are selected with a clustering technique
 * The clustering is achieved via the translation of cameras
 * The process is repeated multiple time on subset of the poses
 * A test of consistency is performed, all potentially valid poses are saved
 * The mean value of valid poses is returned
 */
cv::Mat handeyeBootstratpTranslationCalibration(
    unsigned int nb_cluster, unsigned int nb_it,
    std::vector<cv::Mat> pose_abs_1, std::vector<cv::Mat> pose_abs_2) {
  // N clusters but less if less images available
  nb_cluster =
      (pose_abs_1.size() < nb_cluster) ? pose_abs_1.size() : nb_cluster;

  // Prepare the translational component of the cameras to be clustered
  cv::Mat position_1_2; // concatenation of the translation of pose 1 and 2 for
                        // clustering
  for (unsigned int i = 0; i < pose_abs_1.size(); i++) {
    cv::Mat trans_1, trans_2, rot_1, rot_2;
    Proj2RT(pose_abs_1[i], rot_1, trans_1);
    Proj2RT(pose_abs_2[i], rot_2, trans_2);
    cv::Mat concat_trans_1_2;
    cv::hconcat(trans_1.t(), trans_2.t(), concat_trans_1_2);
    position_1_2.push_back(concat_trans_1_2);
  }
  position_1_2.convertTo(position_1_2, CV_32F);

  // Cluster the observation to select the most diverse poses
  cv::Mat labels;
  cv::Mat centers;
  int nb_kmean_iterations = 5;
  double compactness =
      cv::kmeans(position_1_2, nb_cluster, labels,
                 cv::TermCriteria(
                     cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 10, 0.01),
                 nb_kmean_iterations, cv::KMEANS_PP_CENTERS, centers);
  labels.convertTo(labels, CV_32S);

  // Iterate n times the
  std::vector<double> r1_he, r2_he, r3_he; // structure to save valid rot
  std::vector<double> t1_he, t2_he, t3_he; // structure to save valid trans
  unsigned int nb_clust_pick = 6;
  unsigned int nb_success = 0;
  for (unsigned int iter = 0; iter < nb_it; iter++) {

    // pick from n of these clusters randomly
    std::vector<unsigned int> shuffled_ind;
    for (unsigned int k = 0; k < nb_cluster; ++k)
      shuffled_ind.push_back(k);
    std::random_device rd; // initialize random number generator
    std::mt19937 g(rd());
    std::shuffle(shuffled_ind.begin(), shuffled_ind.end(), g);
    std::vector<unsigned int> cluster_select;
    for (unsigned int k = 0; k < nb_clust_pick; ++k) {
      cluster_select.push_back(shuffled_ind[k]);
    }

    // Select one pair of pose for each cluster
    std::vector<unsigned int> pose_ind;
    for (unsigned int i = 0; i < cluster_select.size(); i++) {
      unsigned int clust_ind = cluster_select[i];
      std::vector<unsigned int> idx;
      for (unsigned int j = 0; j < pose_abs_2.size(); j++) {
        if (labels.at<unsigned int>(j) == clust_ind) {
          idx.push_back(j);
        }
      }
      // randomly select an index in the occurrences of the cluster
      srand(time(NULL));
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> dis(0, idx.size() - 1);
      unsigned int cluster_idx = dis(gen);
      pose_ind.push_back(idx[cluster_idx]);
    }

    // Prepare the poses for handeye calibration
    std::vector<cv::Mat> r_cam_group_1, t_cam_group_1, r_cam_group_2,
        t_cam_group_2;
    for (unsigned int i = 0; i < pose_ind.size(); i++) {
      // get the poses
      cv::Mat pose_cam_group_1 = pose_abs_1[pose_ind[i]].inv();
      cv::Mat pose_cam_group_2 = pose_abs_2[pose_ind[i]];

      // save in datastruct
      cv::Mat r_1, r_2, t_1, t_2;
      Proj2RT(pose_cam_group_1, r_1, t_1);
      Proj2RT(pose_cam_group_2, r_2, t_2);
      cv::Mat r_1_mat, r_2_mat;
      cv::Rodrigues(r_1, r_1_mat);
      cv::Rodrigues(r_2, r_2_mat);
      r_cam_group_1.push_back(r_1_mat);
      t_cam_group_1.push_back(t_1);
      r_cam_group_2.push_back(r_2_mat);
      t_cam_group_2.push_back(t_2);
    }

    // Hand-eye calibration
    cv::Mat r_g1_g2, t_g1_g2;
    cv::calibrateHandEye(r_cam_group_1, t_cam_group_1, r_cam_group_2,
                         t_cam_group_2, r_g1_g2, t_g1_g2,
                         cv::CALIB_HAND_EYE_TSAI);
    // cv::CALIB_HAN
    cv::Mat pose_g1_g2 = RT2Proj(r_g1_g2, t_g1_g2);

    // Check the consistency of the set
    double max_error = 0;
    for (unsigned int i = 0; i < pose_ind.size(); i++) {
      cv::Mat pose_cam_group_1_1 = pose_abs_1[pose_ind[i]];
      cv::Mat pose_cam_group_2_1 = pose_abs_2[pose_ind[i]];
      for (unsigned int j = 0; j < pose_ind.size(); j++) {
        if (i != j) {
          cv::Mat pose_cam_group_1_2 = pose_abs_1[pose_ind[i]];
          cv::Mat pose_cam_group_2_2 = pose_abs_2[pose_ind[i]];
          cv::Mat PP1 = pose_cam_group_1_2.inv() * pose_cam_group_1_1;
          cv::Mat PP2 = pose_cam_group_2_1.inv() * pose_cam_group_2_2;
          cv::Mat ErrMat = PP2.inv() * pose_g1_g2 * PP1 * pose_g1_g2;
          cv::Mat ErrRot, ErrTrans;
          Proj2RT(ErrMat, ErrRot, ErrTrans);
          cv::Mat ErrRotMat;
          cv::Rodrigues(ErrRot, ErrRotMat);
          double traceRot =
              cv::trace(ErrRotMat)[0] - std::numeric_limits<double>::epsilon();
          double err_degree = std::acos(0.5 * (traceRot - 1.0)) * 180.0 / M_PI;
          if (err_degree > max_error)
            max_error = err_degree;
        }
      }
    }
    if (max_error < 15) {
      nb_success++;
      // if it is a sucess then add to our valid pose evector
      cv::Mat rot_temp, trans_temp;
      Proj2RT(pose_g1_g2, rot_temp, trans_temp);
      r1_he.push_back(rot_temp.at<double>(0));
      r2_he.push_back(rot_temp.at<double>(1));
      r3_he.push_back(rot_temp.at<double>(2));
      t1_he.push_back(trans_temp.at<double>(0));
      t2_he.push_back(trans_temp.at<double>(1));
      t3_he.push_back(trans_temp.at<double>(2));
    }
  }

  // if enough sucess (at least 3) then compute median value
  if (nb_success > 3) {
    cv::Mat r_he = cv::Mat::zeros(3, 1, CV_64F);
    cv::Mat t_he = cv::Mat::zeros(3, 1, CV_64F);
    r_he.at<double>(0) = median(r1_he);
    r_he.at<double>(1) = median(r2_he);
    r_he.at<double>(2) = median(r3_he);
    t_he.at<double>(0) = median(t1_he);
    t_he.at<double>(1) = median(t2_he);
    t_he.at<double>(2) = median(t3_he);
    cv::Mat pose_g1_g2 = RVecT2Proj(r_he, t_he);
    return pose_g1_g2;
  } else // else run the normal handeye calibration on all the samples
  {
    cv::Mat pose_g1_g2 = handeyeCalibration(pose_abs_1, pose_abs_1);
    return pose_g1_g2;
  }
}

double median(std::vector<double> &v) {
  size_t n = v.size() / 2;
  nth_element(v.begin(), v.begin() + n, v.end());
  return v[n];
}

// RANSAC algorithm
// Return Inliers, p = proba (typical = 0.99), Output : Rot and Trans Mat,
// Thresh = reprojection tolerance in pixels, it = max iteration
// distortion_type: 0 (perspective), 1 (fisheye)
cv::Mat ransacP3PDistortion(std::vector<cv::Point3f> scene_points,
                            std::vector<cv::Point2f> image_points,
                            cv::Mat intrinsic, cv::Mat distortion_vector,
                            cv::Mat &best_R, cv::Mat &best_T, double thresh,
                            double p, int it, bool refine,
                            int distortion_type) {
  cv::Mat Inliers;
  // P3P for perspective (Brown model)
  if (distortion_type == 0) {
    Inliers =
        ransacP3P(scene_points, image_points, intrinsic, distortion_vector,
                  best_R, best_T, thresh, p, it, refine);
  }

  // P3P for fisheye
  if (distortion_type == 1) {
    // undistord the point
    std::vector<cv::Point2f> imagePointsUndis;
    cv::Mat New_Intrinsic;
    intrinsic.copyTo(New_Intrinsic);
    cv::Mat I = cv::Mat::eye(3, 3, CV_32F);
    cv::fisheye::undistortPoints(image_points, imagePointsUndis, intrinsic,
                                 distortion_vector);

    // multiply by K to go into cam ref (because the OpenCV function is so
    // broken ...)
    for (int i = 0; i < imagePointsUndis.size(); i++) {
      imagePointsUndis[i].x =
          imagePointsUndis[i].x * float(intrinsic.at<double>(0, 0)) +
          float(intrinsic.at<double>(0, 2));
      imagePointsUndis[i].y =
          imagePointsUndis[i].y * float(intrinsic.at<double>(1, 1)) +
          float(intrinsic.at<double>(1, 2));
    }
    // Run p3p
    cv::Mat distortion_vector = (cv::Mat_<double>(1, 5) << 0, 0, 0, 0, 0);
    Inliers =
        ransacP3P(scene_points, imagePointsUndis, New_Intrinsic,
                  distortion_vector, best_R, best_T, thresh, p, it, refine);
  }

  return Inliers;
}

// Project point for fisheye and perspective ()
// distortion_type: 0 (perspective), 1 (fisheye)
void projectPointsWithDistortion(std::vector<cv::Point3f> object_pts,
                                 cv::Mat rot, cv::Mat trans,
                                 cv::Mat camera_matrix,
                                 cv::Mat distortion_vector,
                                 std::vector<cv::Point2f> &repro_pts,
                                 int distortion_type) {
  if (distortion_type == 0) // perspective (Brown)
  {
    cv::projectPoints(object_pts, rot, trans, camera_matrix, distortion_vector,
                      repro_pts);
  }
  if (distortion_type == 1) // fisheye (Kannala)
  {
    cv::fisheye::projectPoints(object_pts, repro_pts, rot, trans, camera_matrix,
                               distortion_vector, 0.0);
  }
}