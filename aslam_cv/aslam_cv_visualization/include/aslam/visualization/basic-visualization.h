#ifndef ASLAM_VISUALIZATION_BASIC_VISUALIZATION_H_
#define ASLAM_VISUALIZATION_BASIC_VISUALIZATION_H_

#include <aslam/common/memory.h>
#include <aslam/frames/visual-frame.h>
#include <aslam/matcher/match.h>
#include <opencv2/core/core.hpp>
#include <Eigen/Dense>

namespace aslam_cv_visualization {

// Colors are in BGR8.
const cv::Scalar kBlue(255, 0, 0);
const cv::Scalar kGreen(0, 255, 0);
const cv::Scalar kBrightGreen(110, 255, 110);
const cv::Scalar kRed(0, 0, 255);
const cv::Scalar kYellow(0, 255, 255);
const cv::Scalar kTurquoise(180, 180, 0);
const cv::Scalar kBlack(0, 0, 0);
const cv::Scalar kWhite(255, 255, 255);

void drawKeypoints(const aslam::VisualFrame& frame, cv::Mat* image);
void drawKeypointMatches(const aslam::VisualFrame& frame_kp1,
                         const aslam::VisualFrame& frame_k,
                         const aslam::Matches& matches_kp1_k,
                         cv::Scalar color_keypoint_kp1, cv::Scalar line_color,
                         cv::Mat* image);
  
}  // namespace aslam_cv_visualization

#endif  // ASLAM_VISUALIZATION_BASIC_VISUALIZATION_H_