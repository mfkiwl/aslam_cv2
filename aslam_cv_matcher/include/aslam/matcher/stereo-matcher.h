#ifndef MATCHER_STEREO_MATCHER_H_
#define MATCHER_STEREO_MATCHER_H_

#include <algorithm>
#include <vector>

#include <Eigen/Core>
#include <aslam/common/feature-descriptor-ref.h>
#include <aslam/common/pose-types.h>
#include <aslam/frames/visual-frame.h>
#include <glog/logging.h>

#include "aslam/matcher/match.h"

namespace aslam {

/// \class StereoMatcher
/// \brief Frame to frame matcher using the epipolar constraint to restrict
/// the search window.
/// The initial matcher attempts to match every keypoint of frame k to a
/// keypoint in frame (k+1). This is done by predicting the keypoint location
/// by using an interframe rotation matrix. Then a rectangular search window
/// around that location is searched for the best match greater than a
/// threshold. If the initial search was not successful, the search window is
/// increased once.
/// The initial matcher is allowed to discard a previous match if the new one
/// has a higher score. The discarded matches are called inferior matches and
/// a second matcher tries to match them. The second matcher only tries
/// to match a keypoint of frame k with the queried keypoints of frame (k+1)
/// of the initial matcher. Therefore, it does not compute distances between
/// descriptors anymore because the initial matcher has already done that.
/// The second matcher is executed several times because it is also allowed
/// to discard inferior matches of the current iteration.
/// The matches are exclusive.
class StereoMatcher {
 public:
  ASLAM_DISALLOW_EVIL_CONSTRUCTORS(StereoMatcher);
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW

  /// \brief Constructs the StereoMatcher.
  /// @param[in]  stereo_pairs  The stereo pairs found in the current setup.
  explicit StereoMatcher(
      const dense_reconstruction::StereoPairIdentifier& stereo_pair,
      const alsam::NCamera::ConstPtr camera_rig)
      : stereo_pair_(stereo_pair),
        camera_rig_(camera_rig_),
        kImageHeight(
            camera_rig_->getCameraShared(stereo_pair.first_camera_id)
                ->imageHeight()){};
  virtual ~StereoMatcher(){};

  /// @param[in]  frame0        The first VisualFrame that needs to contain
  ///                           the keypoints and descriptor channels. Usually
  ///                           this is an output of the VisualPipeline.
  /// @param[in]  frame1        The second VisualFrame that needs to contain
  ///                           the keypoints and descriptor channels. Usually
  ///                           this is an output of the VisualPipeline.
  /// @param[out] matches_frame0_frame1 Vector of structs containing the found
  /// matches.
  ///                           Indices correspond to the ordering of the
  ///                           keypoint/descriptor vector in the respective
  ///                           frame channels.
  void match(
      const VisualFrame& frame0, const VisualFrame& frame1,
      StereoMatchesWithScore* matches_frame0_frame1);

 private:
  struct KeypointData {
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    KeypointData(const Eigen::Vector2d& measurement, const int index)
        : measurement(measurement), channel_index(index) {}
    Eigen::Vector2d measurement;
    int channel_index;
  };

  typedef typename Aligned<std::vector, KeypointData>::const_iterator
      KeyPointIterator;
  typedef typename StereoMatchesWithScore::iterator MatchesIterator;

  struct MatchData {
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    MatchData() = default;
    void addCandidate(
        const KeyPointIterator keypoint_iterator_frame1,
        const double matching_score) {
      CHECK_GT(matching_score, 0.0);
      CHECK_LE(matching_score, 1.0);
      keypoint_match_candidates_frame1.push_back(keypoint_iterator_frame1);
      match_candidate_matching_scores.push_back(matching_score);
    }
    // Iterators of keypoints of frame1 that were candidates for the match
    // together with their scores.
    std::vector<KeyPointIterator> keypoint_match_candidates_frame1;
    std::vector<double> match_candidate_matching_scores;
  };

  /// \brief Match a keypoint of frame0 with one of frame1 if possible.
  ///
  /// Initial matcher that tries to match a keypoint of frame0 with
  /// a keypoint of frame1 once. It is allowed to discard an
  /// already existing match.
  void matchKeypoint(const int idx_k);

  void getKeypointIteratorsInWindow(
      const Eigen::Vector2d& predicted_keypoint_position,
      const int window_half_side_length_px,
      KeyPointIterator* it_keypoints_begin,
      KeyPointIterator* it_keypoints_end) const;

  /// \brief Try to match inferior matches without modifying initial matches.
  ///
  /// Second matcher that is only quering keypoints of frame1 that the
  /// initial matcher has queried before. Should be executed several times.
  /// Returns true if matches are still found.
  bool matchInferiorMatches(
      std::vector<bool>* is_inferior_keypoint_frame1_matched);

  int clamp(const int lower, const int upper, const int in) const;

  // The larger the matching score (which is smaller or equal to 1),
  // the higher the probability that a true match occurred.
  double computeMatchingScore(
      const int num_matching_bits,
      const unsigned int descriptor_size_bits) const;

  // Compute ratio test. Test is inspired by David Lowe's "ratio test"
  // for matching descriptors. Returns true if test is passed.
  bool ratioTest(
      const unsigned int descriptor_size_bits,
      const unsigned int distance_shortest,
      const unsigned int distance_second_shortest) const;

  const dense_reconstruction::StereoPairIdentifier& stereo_pair_;
  const alsam::NCamera::ConstPtr camera_rig_;
  const uint32_t kImageHeight;

  // Map from keypoint indices of frame1 to
  // the corresponding match iterator.
  std::unordered_map<int, MatchesIterator> frame1_idx_to_matches_iterator_map_;

  // The queried keypoints in frame1 and the corresponding
  // matching score are stored for each attempted match.
  // A map from the keypoint in frame0 to the corresponding
  // match data is created.
  std::unordered_map<int, MatchData> idx_frame0_to_attempted_match_data_map_;
  // Inferior matches are a subset of all attempted matches.
  // Remeber indices of keypoints in frame0 that are deemed inferior matches.
  std::vector<int> inferior_match_keypoint_idx_frame0_;

  // Two descriptors could match if the number of matching bits normalized
  // with the descriptor length in bits is higher than this threshold.
  static constexpr float kMatchingThresholdBitsRatioRelaxed = 0.8f;
  // The more strict threshold is used for matching inferior matches.
  // It is more strict because there is no ratio test anymore.
  static constexpr float kMatchingThresholdBitsRatioStrict = 0.85f;
  // Two descriptors could match if they pass the Lowe ratio test.
  static constexpr float kLoweRatio = 0.8f;
  // Number of iterations to match inferior matches.
  static constexpr size_t kMaxNumInferiorIterations = 3u;
};

void StereoMatcher::getKeypointIteratorsInWindow(
    const Eigen::Vector2d& predicted_keypoint_position,
    const int window_half_side_length_px, KeyPointIterator* it_keypoints_begin,
    KeyPointIterator* it_keypoints_end) const {
  CHECK_NOTNULL(it_keypoints_begin);
  CHECK_NOTNULL(it_keypoints_end);
  CHECK_GT(window_half_side_length_px, 0);

  // Compute search area for LUT iterators row-wise.
  int LUT_index_top = clamp(
      0, kImageHeight - 1,
      static_cast<int>(
          predicted_keypoint_position(1) + 0.5 - window_half_side_length_px));
  int LUT_index_bottom = clamp(
      0, kImageHeight - 1,
      static_cast<int>(
          predicted_keypoint_position(1) + 0.5 + window_half_side_length_px));

  *it_keypoints_begin =
      keypoints_frame1_sorted_by_y_.begin() + corner_row_LUT_[LUT_index_top];
  *it_keypoints_end =
      keypoints_frame1_sorted_by_y_.begin() + corner_row_LUT_[LUT_index_bottom];

  CHECK_LE(LUT_index_top, LUT_index_bottom);
  CHECK_GE(LUT_index_bottom, 0);
  CHECK_GE(LUT_index_top, 0);
  CHECK_LT(LUT_index_top, kImageHeight);
  CHECK_LT(LUT_index_bottom, kImageHeight);
}

inline int StereoMatcher::clamp(
    const int lower, const int upper, const int in) const {
  return std::min<int>(std::max<int>(in, lower), upper);
}

inline double StereoMatcher::computeMatchingScore(
    const int num_matching_bits,
    const unsigned int descriptor_size_bits) const {
  return static_cast<double>(num_matching_bits) / descriptor_size_bits;
}

inline bool StereoMatcher::ratioTest(
    const unsigned int descriptor_size_bits,
    const unsigned int distance_closest,
    const unsigned int distance_second_closest) const {
  CHECK_LE(distance_closest, distance_second_closest);
  if (distance_second_closest > descriptor_size_bits) {
    // There has never been a second matching candidate.
    // Consequently, we cannot conclude with this test.
    return true;
  } else if (distance_second_closest == 0u) {
    // Unusual case of division by zero:
    // Let the ratio test be successful.
    return true;
  } else {
    return distance_closest / static_cast<float>(distance_second_closest) <
           kLoweRatio;
  }
}

}  // namespace aslam

#endif  // MATCHER_STEREO_MATCHER_H_