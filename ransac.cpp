#include "ht-api.h"
#include "ht-internal.h"
using namespace std;
using namespace cv;

bool ht_ransac_best_indices(headtracker_t& ctx, float& mean_error, Mat& rvec_, Mat& tvec_) {
    Mat intrinsics = Mat::eye(3, 3, CV_32FC1);
    intrinsics.at<float> (0, 0) = ctx.focal_length_w;
    intrinsics.at<float> (1, 1) = ctx.focal_length_h;
    intrinsics.at<float> (0, 2) = ctx.grayscale.cols/2;
    intrinsics.at<float> (1, 2) = ctx.grayscale.rows/2;

    Mat dist_coeffs = Mat::zeros(5, 1, CV_32FC1);
    Mat rvec = Mat::zeros(3, 1, CV_64FC1);
    Mat tvec = Mat::zeros(3, 1, CV_64FC1);

    rvec.at<double> (0, 0) = 1.0;
    tvec.at<double> (0, 0) = 1.0;
    tvec.at<double> (1, 0) = 1.0;

    vector<Point3f> object_points;
    vector<Point2f> image_points;
    for (int i = 0; i < ctx.config.max_keypoints; i++) {
        if (ctx.keypoints[i].idx == -1)
            continue;
        object_points.push_back(ctx.keypoint_uv[i]);
        image_points.push_back(ctx.keypoints[i].position);
    }

    if (object_points.size() >= 8)
    {
        vector<int> inliers;
        solvePnPRansac(object_points,
                       image_points,
                       intrinsics,
                       dist_coeffs,
                       rvec,
                       tvec,
					   false,
                       ctx.config.ransac_num_iters,
                       ctx.config.ransac_max_inlier_error * ctx.zoom_ratio,
                       object_points.size() * ctx.config.ransac_min_features,
                       inliers,
                       HT_PNP_TYPE);

		vector<Point2f> projected;

		projectPoints(object_points, rvec, tvec, intrinsics, dist_coeffs, projected);

        if (inliers.size() >= 4)
        {
			int j = 0, k = 0;
			mean_error = 0;
			float max_dist = ctx.config.ransac_max_reprojection_error * ctx.zoom_ratio;
			max_dist *= max_dist;

			for (int i = 0; i < ctx.config.max_keypoints; i++)
			{
				if (ctx.keypoints[i].idx == -1)
					continue;

				float dist = ht_distance2d_squared(image_points[j], projected[j]);

				if (dist > max_dist)
				{
					ctx.keypoints[i].idx = -1;
				} else {
					k++;
					mean_error += dist;
				}
				j++;
			}

			object_points.clear();
            image_points.clear();

            for (int i = 0; i < ctx.config.max_keypoints; i++) {
                if (ctx.keypoints[i].idx == -1)
                    continue;
                object_points.push_back(ctx.keypoint_uv[i]);
                image_points.push_back(ctx.keypoints[i].position);
            }

            if (object_points.size() >= 4)
            {
				solvePnP(object_points, image_points, intrinsics, dist_coeffs, rvec, tvec, true, HT_PNP_TYPE);

				mean_error = sqrt(mean_error / std::max(1, k));

				if (j >= 4)
				{

					rvec_ = rvec;
					tvec_ = tvec;

					return true;
				}
			}
        }
    }

    if (ctx.config.debug)
        fprintf(stderr, "ransac failed maxerr=%f zoom-ratio=%f cur=%d\n",
                ctx.config.ransac_max_inlier_error, ctx.zoom_ratio, object_points.size());

    return false;
}

