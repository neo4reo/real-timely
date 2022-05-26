/*
 *  Adapted by Nick McCrea. Original preamble is below:
 *
 *  ---
 *
 *  Example by Sam Siewert
 *
 *  Updated 12/6/18 for OpenCV 3.1
 *
 */

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include "../utils/time.h"

char difftext[20];
char timetext[20];

int main(int argc, char **argv)
{
  cv::Mat mat_frame, mat_gray, mat_diff, mat_gray_prev;
  cv::VideoCapture video_capture;
  unsigned int diffsum, maxdiff, framecnt = 0;
  double percent_diff = 0.0, percent_diff_old = 0.0;
  double average_percent_diff = 0.0;

  struct timespec current_time;
  get_current_realtime_time(&current_time);

  // open the video stream and make sure it's opened
  //  "0" is the default video device which is normally the built-in webcam
  if (!video_capture.open(0))
  {
    std::cout << "Error opening video stream or file" << std::endl;
    return -1;
  }
  else
  {
    std::cout << "Opened default camera interface" << std::endl;
  }

  while (!video_capture.read(mat_frame))
  {
    std::cout << "No frame" << std::endl;
    cv::waitKey(33);
  }

  cv::cvtColor(mat_frame, mat_gray, CV_BGR2GRAY);

  mat_diff = mat_gray.clone();
  mat_gray_prev = mat_gray.clone();

  maxdiff = (mat_diff.cols) * (mat_diff.rows) * 255;

  while (1)
  {
    if (!video_capture.read(mat_frame))
    {
      std::cout << "No frame" << std::endl;
      cv::waitKey();
    }
    else
    {
      framecnt++;
      get_current_realtime_time(&current_time);
    }

    cv::cvtColor(mat_frame, mat_gray, CV_BGR2GRAY);
    cv::absdiff(mat_gray_prev, mat_gray, mat_diff);

    // worst case sum is resolution * 255
    diffsum = (unsigned int)cv::sum(mat_diff)[0]; // single channel sum

    percent_diff = ((double)diffsum / (double)maxdiff) * 100.0;

    if (framecnt < 3)
      average_percent_diff = (percent_diff + percent_diff_old) / (double)framecnt;
    else
      average_percent_diff = ((average_percent_diff * (double)framecnt) + percent_diff) / (double)(framecnt + 1);

    syslog(LOG_CRIT, "TICK: percent diff, %lf, old, %lf, ma, %lf, cnt, %u, change, %lf\n", percent_diff, percent_diff_old, average_percent_diff, framecnt, (percent_diff - percent_diff_old));
    sprintf(difftext, "%8d", diffsum);
    sprintf(timetext, "%6.3lf", get_time_in_seconds(&current_time));

    percent_diff_old = percent_diff;

    // tested in ERAU Jetson lab
    if (percent_diff > 0.5)
    {
      cv::putText(mat_diff, difftext, cvPoint(30, 30), cv::FONT_HERSHEY_COMPLEX_SMALL, 0.8, cvScalar(200, 200, 250), 1, CV_AA);
      cv::putText(mat_diff, timetext, cvPoint(500, 30), cv::FONT_HERSHEY_COMPLEX_SMALL, 0.8, cvScalar(200, 200, 250), 1, CV_AA);
    }

    cv::imshow("Clock Current", mat_gray);
    cv::imshow("Clock Previous", mat_gray_prev);
    cv::imshow("Clock Diff", mat_diff);

    char c = cvWaitKey(100); // sample rate
    if (c == 'q')
      break;

    mat_gray_prev = mat_gray.clone();
  }
};
