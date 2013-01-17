#include "ht-api.h"
#include "ht-internal.h"
using namespace std;
using namespace cv;

bool ht_ransac_best_indices(headtracker_t& ctx, float& mean_error, Mat& rvec_, Mat& tvec_) {
    if (ctx.keypoint_count < 4)
        return false;

    Mat intrinsics = Mat::eye(3, 3, CV_32FC1);
    intrinsics.at<float> (0, 0) = ctx.focal_length_w;
    intrinsics.at<float> (1, 1) = ctx.focal_length_h;
    intrinsics.at<float> (0, 2) = ctx.grayscale.cols/2;
    intrinsics.at<float> (1, 2) = ctx.grayscale.rows/2;

    Mat dist_coeffs = Mat::zeros(5, 1, CV_32FC1);
    Mat rvec = Mat::zeros(3, 1, CV_64FC1);
    Mat tvec = Mat::zeros(3, 1, CV_64FC1);

    Mat rvec2 = Mat::zeros(3, 1, CV_64FC1);
    Mat tvec2 = Mat::zeros(3, 1, CV_64FC1);

    rvec.at<double> (0, 0) = 1.0;
    tvec.at<double> (0, 0) = 1.0;
    tvec.at<double> (1, 0) = 1.0;

    rvec2.at<double> (0, 0) = 1.0;
    tvec2.at<double> (0, 0) = 1.0;
    tvec2.at<double> (1, 0) = 1.0;

    vector<Point3f> object_points;
    vector<Point2f> image_points;
    vector<int> inliers;
    for (int i = 0, j = 0; i < ctx.config.max_keypoints; i++) {
        if (ctx.keypoints[i].idx == -1)
            continue;
        object_points.push_back(ctx.keypoint_uv[i]);
        image_points.push_back(ctx.keypoints[i].position);
    }

    if (ctx.has_pose) {
        rvec = ctx.rvec;
        tvec = ctx.tvec;
        rvec2 = ctx.rvec;
        tvec2 = ctx.tvec;
    }

    solvePnPRansac(object_points,
                   image_points,
                   intrinsics,
                   dist_coeffs,
                   rvec2,
                   tvec2,
                   ctx.has_pose,
                   ctx.config.ransac_num_iters,
                   ctx.config.ransac_max_inlier_error * ctx.zoom_ratio,
                   ctx.keypoint_count * ctx.config.ransac_min_features,
                   inliers,
                   CV_ITERATIVE);

    if (inliers.size() >= 4)
    {
        vector<Point2f> image_points2;

        projectPoints(object_points, rvec2, tvec2, intrinsics, dist_coeffs, image_points2);

        float max_dist = ctx.config.ransac_max_inlier_error * ctx.zoom_ratio;
        max_dist *= max_dist;
        int j = 0, k = 0;
        mean_error = 0;

        for (int i = 0; i < ctx.config.max_keypoints; i++)
        {
            if (ctx.keypoints[i].idx == -1)
                continue;
            float dist = ht_distance2d_squared(image_points[j], image_points2[j]);

            if (dist > max_dist)
            {
                ctx.keypoint_count--;
                ctx.keypoints[i].idx = -1;
            } else {
                k++;
                mean_error += dist;
            }
            j++;
        }

        mean_error = sqrt(mean_error / std::max(1, k));

        object_points.clear();
        image_points.clear();

        for (int i = 0, j = 0; i < ctx.config.max_keypoints; i++) {
            if (ctx.keypoints[i].idx == -1)
                continue;
            object_points.push_back(ctx.keypoint_uv[i]);
            image_points.push_back(ctx.keypoints[i].position);
        }

        solvePnP(object_points, image_points, intrinsics, dist_coeffs, rvec, tvec, ctx.has_pose, CV_ITERATIVE);

        ctx.has_pose = true;

        rvec_ = rvec;
        tvec_ = tvec;

        return true;
    }

    return false;
}

