/*

  Copyright 2012-2020 Lucas Walter

    This file is part of Vimaj.

    Vimjay is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Vimjay is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Vimjay.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <sstream>
#include <stdio.h>
#include <time.h>

#include <boost/filesystem/operations.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>
#include <boost/timer.hpp>
#include <deque>

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include <gflags/gflags.h>
#include <glog/logging.h>

// bash color codes
#define CLNRM "\e[0m"
#define CLWRN "\e[0;43m"
#define CLERR "\e[1;41m"
#define CLVAL "\e[1;36m"
#define CLTXT "\e[1;35m"
// BOLD black text with blue background
#define CLTX2 "\e[1;44m"

// TBD move to config file in $HOME/.vimaj/config.yml
DEFINE_int32(width, 800, "width");
DEFINE_int32(height, 600, "height");
DEFINE_double(max_scale, 1.5, "maximum amount to scale the image");

// namespace bm

class Images {

  float progress;

  // the size of the rendered image
  cv::Size sz;
  // an roi in units of the rendered image
  // cv::Rect roi;

  float max_scale;
  std::vector<cv::Mat> frames_orig;
  std::vector<cv::Mat> frames_scaled;
  // rendered, TBD do this live
  std::vector<cv::Mat> frames_rendered;
  boost::thread im_thread;
  boost::mutex im_mutex;
  boost::mutex im_scaled_mutex;
  std::string dir;
  std::vector<std::string> files;
  std::vector<std::string> files_used;

  int cur_ind;
  cv::Mat cur_roi_im;
  cv::Rect cur_roi;
  // full sized image
  cv::Mat cur_im;

public:
  float roi_aspect;

  bool continue_loading;

  int ind;

  Images(cv::Size sz, float max_scale)
      : sz(sz), max_scale(max_scale), continue_loading(true), ind(0),
        progress(0.0), roi_aspect(1.0) {
    im_thread = boost::thread(&Images::runThread, this);

  } // Images

  ~Images() { im_thread.join(); }

  void runThread() {
    boost::timer t1;
    const bool rv = getFileNames(".");

    const bool rv2 = loadAndResizeImages(sz, max_scale);

    float t1_elapsed = t1.elapsed();

    LOG(INFO) << "loaded " << getNum() << " in time " << t1_elapsed << " "
              << (float)t1_elapsed / (float)getNum();
  }

  // TBD is this any faster than warpImage?
  // dst needs to exist before rendering
  bool renderImage(cv::Mat &src, cv::Mat &dst, cv::Rect &roi, int offx = 0,
                   int offy = 0) {
    if (offx > dst.cols)
      return false;
    if (offy > dst.rows)
      return false;
    if (-offx >= src.cols)
      return false;
    if (-offy >= src.rows)
      return false;

    int src_x = 0;
    int src_y = 0;
    int src_wd = src.cols;
    int src_ht = src.rows;
    if (offx < 0) {
      src_x = -offx;
      offx = 0;
    }
    if (offy < 0) {
      src_y = -offy;
      offy = 0;
    }
    if (src.cols + offx - src_x > dst.cols) {
      // if (src_wd + offx - src_x > dst.cols) {
      src_wd = dst.cols - offx + src_x;
    }
    if (src.rows + offy - src_y > dst.rows) {
      // if (src_ht + offy > dst.rows) {
      src_ht = dst.rows - offy + src_y;
    }
    src_wd -= src_x;
    src_ht -= src_y;

    VLOG(2) << "src " << src_x << " " << src_y << ", " << src_wd << " "
            << src_ht << ", offxy " << offx << " " << offy << ", src "
            << src.cols << " " << src.rows << ", dst " << dst.cols << " "
            << dst.rows;
    cv::Mat src_clipped = src(cv::Rect(src_x, src_y, src_wd, src_ht));

    roi = cv::Rect(offx, offy, src_clipped.cols, src_clipped.rows);

    cv::Mat dst_roi = dst(roi);
    src_clipped.copyTo(dst_roi);

    // draw rectangle around the roi
    if (true) {
      cv::Rect roi2 = cv::Rect(offx - 1, offy - 1, src_clipped.cols + 2,
                               src_clipped.rows + 2);
      cv::rectangle(dst, roi2, cv::Scalar(165, 175, 150), 1);
    }

    return true;
  }

  /*
    Figure out the zoom from the auto resized image and multiply the user
    specified zoom before handing the image to this pos is normalized 0.0-1.0

     ______________
    |              |
    |              |
    |              |
    |              |
    |______________|

    IF the source image is scaled so the height is taller than the size sz, then
    the height has to be limited to sz.height and the roi within src sized and
    positioned accordingly.

  */
  bool clipZoom(const cv::Mat &src, cv::Mat &dst, const cv::Size sz,
                const float zoom = 1.0,
                const cv::Point2f pos = cv::Point2f(0.5, 0.5)) {
    cv::Size desired_sz =
        cv::Size(src.size().width * zoom, src.size().height * zoom);

    cv::Size actual_sz = sz;

    float width_fract = 1.0;
    if (desired_sz.width > sz.width) {
      width_fract = (float)sz.width / (float)desired_sz.width;
    } else {
      actual_sz.width = desired_sz.width;
    }

    float height_fract = 1.0;
    if (desired_sz.height > sz.height) {
      height_fract = (float)sz.height / (float)desired_sz.height;
    } else {
      actual_sz.height = desired_sz.height;
    }

    // offset is in the desired_sz scale, need to scale it down
    cv::Rect roi; // = cv::Rect(0, 0, src.cols, src.rows);

    roi.width = width_fract * src.cols;
    roi.height = height_fract * src.rows;

    roi.x = 0; // (src.cols - roi.width) * pos.x;
    roi.y = 0; //(src.rows - roi.height) * pos.y;

    // roi.y = src.rows * (pos.y - 0.5);

    int offx = 0;
    // these are the coordinates if the image had been blown up at full res
    // so they are valid if the zoomed image is smaller than the dst sz
    const float full_offx = -(pos.x * desired_sz.width - sz.width / 2);
    if (sz.width != actual_sz.width)
      offx = full_offx;
    else {
      roi.x = -src.cols * (float)full_offx /
              (float)desired_sz.width; //  -(pos.x * src.size().width);
      VLOG(2) << roi.x;

      if (roi.x < 0) {

        int new_roi_width = roi.width + roi.x;
        int new_actual_sz_width =
            actual_sz.width * (float)(new_roi_width) / (float)roi.width;
        offx = actual_sz.width - new_actual_sz_width;
        actual_sz.width = new_actual_sz_width;

        roi.width = new_roi_width;
        roi.x = 0;

        // TBD adjust offx and actual_sz
        VLOG(3) << offx << " " << actual_sz.width << ", roi " << roi.x << " "
                << roi.width;
      }

      if (roi.x + roi.width > src.cols) {
        int new_roi_width = src.cols - roi.x;
        actual_sz.width *= (float)(new_roi_width) / (float)roi.width;
        roi.width = new_roi_width;
      }
    }

    int offy = 0;
    const float full_offy = -(pos.y * desired_sz.height - sz.height / 2);
    if (sz.height != actual_sz.height)
      offy = full_offy;
    else {
      roi.y = -src.rows * (float)full_offy /
              (float)desired_sz.height; //  -(pos.y * src.size().height);
      VLOG(2) << roi.y;

      if (roi.y < 0) {

        int new_roi_height = roi.height + roi.y;
        int new_actual_sz_height =
            actual_sz.height * (float)(new_roi_height) / (float)roi.height;
        offy = actual_sz.height - new_actual_sz_height;
        actual_sz.height = new_actual_sz_height;

        roi.height = new_roi_height;
        roi.y = 0;

        VLOG(3) << offy << " " << actual_sz.height << ", roi " << roi.y << " "
                << roi.height;
      }

      if (roi.y + roi.height > src.rows) {
        int new_roi_height = src.rows - roi.y;
        actual_sz.height *= (float)(new_roi_height) / (float)roi.height;
        roi.height = new_roi_height;
      }
    }

    dst = cv::Mat(sz, src.type(), cv::Scalar::all(0));

    if ((roi.width > 0) && (roi.height > 0)) {
      const int mode = cv::INTER_NEAREST;
      cv::Mat resized;
      cv::resize(src(roi), resized, actual_sz, 0, 0, mode);

      // this can optionally save the roi image
      // instead of a member variable side effect
      // should this get returned to the caller?
      cur_im = src;
      cur_roi_im = src(roi);
      cur_roi = roi;

      cv::Rect rendered_roi;
      renderImage(resized, dst, rendered_roi, offx, offy);

      VLOG(2) << sz.width << " " << sz.height << ", " << resized.cols << " "
              << resized.rows << " " << zoom;
    }
#if 0 
  if ((actual_sz.height < sz.height) || (actual_sz.width < sz.width)) {
    dst = cv::Mat(sz, resized.type(), cv::Scalar::all(0));
    
    cv::Rect roi = cv::Rect( (sz.width - resized.cols)/2, (sz.height - resized.rows)/2 ,
        resized.cols, resized.rows );
    
    cv::Mat dst_roi = dst(roi);
    resized.copyTo(dst_roi);

  } else {
    dst = resized;
  }
#endif

    return true;
  }

  /* resize the image into a new image of a fixed size, automatically scale it
   * down to fit
   */
  bool resizeImage(const cv::Mat &tmp0, cv::Mat &tmp_aspect,
                   const cv::Size sz) {
    const float aspect_0 = (float)tmp0.cols / (float)tmp0.rows;
    const float aspect_1 = (float)sz.width / (float)sz.height;

    cv::Size tmp_sz = sz;

    // TBD could have epsilon defined by 1 pixel width
    if (aspect_0 > aspect_1) {
      tmp_sz.height = tmp_sz.width / aspect_0;
    } else if (aspect_0 < aspect_1) {
      tmp_sz.width = tmp_sz.height * aspect_0;
    }

    VLOG(2) << tmp0.cols << " " << tmp0.rows << " * " << max_scale << " -> "
            << tmp_sz.width << " " << tmp_sz.height;
    // make sure not to upscale the image too much
    if (tmp_sz.width > tmp0.cols * max_scale) {
      tmp_sz.width = tmp0.cols * max_scale;
      tmp_sz.height = tmp0.rows * max_scale;
    }
    // if (tmp_sz.height > tmp0.rows * max_scale) {
    //  tmp_sz.width = tmp0.cols * max_scale;
    //  tmp_sz.height = tmp0.rows * max_scale;
    //}

    // int mode = cv::INTER_NEAREST;
    // int mode = cv::INTER_CUBIC;
    int mode = cv::INTER_LINEAR;

    cv::resize(tmp0, tmp_aspect, tmp_sz, 0, 0, mode);
    return true;
  }

  bool renderMultiImage(const int i, cv::Mat &tmp1) {
    int ind = i;
    cv::Mat tmp_aspect = getScaledFrame(ind);

    tmp1 = cv::Mat(sz, tmp_aspect.type(), cv::Scalar::all(0));

    if (tmp_aspect.empty()) {
      LOG(INFO) << "scaled frame " << ind << " is empty";
      return false;
    }

    // center the image
    int off_x = (sz.width - tmp_aspect.size().width) / 2;
    int off_y = (sz.height - tmp_aspect.size().height) / 2;

    // TBD flag?
    int border = 4;
    // TBD off_y should be a function of sz and the prev/next image size
    int ind2 = ind - 1;
    cv::Mat prev = getScaledFrame(ind2);

    cv::Rect roi;
    if (ind2 != i) {
      renderImage(prev, tmp1, roi, off_x - prev.cols - border, off_y);

      ind2 = ind + 1;
      cv::Mat next = getScaledFrame(ind);
      renderImage(next, tmp1, roi, off_x + tmp_aspect.size().width + border,
                  off_y);
    }
    renderImage(tmp_aspect, tmp1, roi, off_x, off_y);

#if 0
  // TBD put offset so image is centered
  cv::Mat tmp1_roi = tmp1(cv::Rect(off_x, off_y, 
        tmp_aspect.cols, tmp_aspect.rows));
  tmp_aspect.copyTo(tmp1_roi);
#endif

    VLOG(3) //<< aspect_0 << " " << aspect_1 << ", "
        << off_x << " " << off_y << " " << tmp_aspect.cols << " "
        << tmp_aspect.rows;

    std::stringstream ss;
    ss << ind << "/" << getNum();
    cv::putText(tmp1, ss.str(), cv::Point(10, 10), 1, 1,
                cv::Scalar(255, 200, 210));
    cv::putText(tmp1, files_used[ind], cv::Point(100, 10), 1, 1,
                cv::Scalar::all(255));

    return true;
  }

  bool loadAndResizeImages(
      // std::vector<cv::Mat>& frames,
      const cv::Size sz, const double max_scale) {
    frames_orig.clear();
    frames_rendered.clear();
    frames_scaled.clear();

    // TBD make optional
    sort(files.begin(), files.end());

    LOG(INFO) << "loading " << files.size() << " files";

    for (int i = 0; (i < files.size()) && (continue_loading == true); i++) {

      const std::string next_im = files[i];

      // TBD only store the names in first pass, then load in second?
      cv::Mat new_out = cv::imread(next_im);

      if (new_out.data == NULL) { //.empty()) {
        LOG(WARNING) << " not an image? " << next_im;
        continue;
      }

      files_used.push_back(files[i]);

      VLOG(2) << " " << i << " loaded image " << next_im;

      frames_orig.push_back(new_out);

      cv::Mat frame_scaled;
      resizeImage(new_out, frame_scaled, sz);

      {
        boost::mutex::scoped_lock l(im_scaled_mutex);
        frames_scaled.push_back(frame_scaled);
      }

#if 0
    cv::Mat multi_im;
    if (i < 1) {
      renderMultiImage(i, multi_im);
      frames_rendered.push_back(multi_im);
    } else
    if (i > 1) {
      renderMultiImage(i - 1, multi_im);
      {
        boost::mutex::scoped_lock l(im_mutex);
        frames_rendered.push_back(multi_im);
      }

    } //

    if (i % 20 == 0) LOG(INFO) << "loaded " << i;
    // clear frames as we go
    if (true && (i > 3)) {
      boost::mutex::scoped_lock l(im_scaled_mutex);
      frames_scaled[i-2].release();
    }
#endif

      progress = (float)i / (float)files.size();
    } // files loop

#if 0
  cv::Mat multi_im;
  renderMultiImage(0, multi_im);
  {
    boost::mutex::scoped_lock l(im_mutex);
    frames_rendered[0] = multi_im;
  }

  // now fill unrendered frames
  renderMultiImage(1, multi_im);
  {
    boost::mutex::scoped_lock l(im_mutex);
    frames_rendered[1] = multi_im;
  }

  renderMultiImage(frames_scaled.size()-1, multi_im);
  {
    boost::mutex::scoped_lock l(im_mutex);
    frames_rendered.push_back(multi_im);
  }

  frames_scaled.clear();
#endif
    return true;
  } // loadAndResizeImages

  bool getFileNames(std::string dir) {
    this->dir = dir;
    std::string name = "vimaj";
    LOG(INFO) << name << " loading " << dir;

    boost::filesystem::path image_path(dir);
    if (!is_directory(image_path)) {
      LOG(ERROR) << name << CLERR << " not a directory " << CLNRM << dir;
      return false;
    }

    // TBD clear frames first?

    boost::filesystem::directory_iterator
        end_itr; // default construction yields past-the-end
    for (boost::filesystem::directory_iterator itr(image_path); itr != end_itr;
         ++itr) {
      if (is_directory(*itr))
        continue;

      std::stringstream ss;
      ss << *itr;
      std::string next_im = (ss.str());
      // strip off "" at beginning/end
      next_im = next_im.substr(1, next_im.size() - 2);

      if (!((next_im.substr(next_im.size() - 3, 3) == "jpg") ||
            (next_im.substr(next_im.size() - 3, 3) == "png"))) {
        // LOG(INFO) << "not expected image type: " << next_im;
        continue;
      }

      files.push_back(next_im);
    }
    return true;
  }

  /////////////////////////////////
  cv::Mat getScaledFrame(int &ind) {
    boost::mutex::scoped_lock l(im_scaled_mutex);
    if (frames_scaled.size() == 0)
      return cv::Mat();
    ind = (ind + frames_scaled.size()) % frames_scaled.size();
    return frames_scaled[ind];
  }

  /* get a rendered multi frame
   */
  cv::Mat getFrame(int &ind, const double zoom = 1.0,
                   cv::Point2f pos = cv::Point2f(0.5, 0.5)) {
    ind = (ind + frames_scaled.size()) % frames_scaled.size();
    cur_ind = ind;
    /*
    if (zoom == 1.0) {
      cv::Mat multi_im;
      renderMultiImage(ind, multi_im);
      return multi_im;
    } else
    */
    {
      cv::Mat src = frames_orig[ind];
      const float scaled_zoom =
          (float)frames_scaled[ind].cols / (float)src.cols;
      VLOG(4) << scaled_zoom << " " << zoom << " " << zoom * scaled_zoom;
      cv::Size desired_sz = cv::Size(src.size().width * zoom * scaled_zoom,
                                     src.size().height * zoom * scaled_zoom);

      cv::Mat dst = cv::Mat(sz, src.type(), cv::Scalar::all(0));
      if (false) {
        // This method is super slow because it blows up the image so much
        // it would be better to make clipZoom zoom an area slightly larger than
        // will be displayed and then clip on the zoomed image rather than with
        // the roi in the original src image

        const int mode = cv::INTER_NEAREST;
        cv::Mat resized;
        cv::resize(frames_orig[ind], resized, desired_sz, 0, 0, mode);

        cv::Rect roi;
        renderImage(resized, dst, roi, -(pos.x * resized.cols - sz.width / 2),
                    -(pos.y * resized.rows - sz.height / 2));

        VLOG(4) << scaled_zoom << " " << zoom << " " << zoom * scaled_zoom;

      } else {

        clipZoom(src, dst, sz, zoom * scaled_zoom, pos);
      }

      // draw rectangle on image to show current roi
      cv::rectangle(dst, getRoiRect(1), cv::Scalar(0, 0, 0), 1);
      cv::rectangle(dst, getRoiRect(0), cv::Scalar(255, 255, 255), 1);

      if (VLOG_IS_ON(1))
        cv::circle(dst, cv::Point(dst.cols / 2, dst.rows / 2), 5,
                   cv::Scalar::all(255), -1);
      return dst;
    }

    // it would be nice to
#if 0
      boost::mutex::scoped_lock l(im_mutex);
      if (frames_rendered.size() == 0) return cv::Mat();
      return frames_rendered[ind];
#endif
  }

  int getNum() {
    // boost::mutex::scoped_lock l(im_mutex);
    // return frames_rendered.size();
    boost::mutex::scoped_lock l(im_scaled_mutex);
    return frames_scaled.size();
  }

  ////////////////////////////////////////////////////////////
  cv::Rect getRoiRect(const int pad = 0, double zoom = 1.0) {
    float base_aspect = (float)sz.width / (float)sz.height;

    int p2 = 0;
    if (pad != 0)
      p2 = 1;

    cv::Rect roi;
    if (roi_aspect == 1.0) {
      roi.x = -1;
      roi.y = -1;
      roi.width = sz.width + 2;
      roi.height = sz.height + 2;
    } else if (roi_aspect > 1.0) {
      roi.x = -pad + p2;
      roi.width = sz.width + 2 * pad;

      roi.height =
          (int)((float)sz.width / (roi_aspect * base_aspect)) + 2 * pad;
      roi.y = (sz.height - roi.height) / 2 - pad + p2;
    } else {
      roi.y = -pad + p2;
      roi.height = sz.height + 2 * pad;

      roi.width = (int)((float)sz.width * (roi_aspect * base_aspect)) + 2 * pad;
      roi.x = (sz.width - roi.width) / 2 - pad + p2;
    }

    roi.x *= zoom;
    roi.y *= zoom;
    roi.width *= zoom;
    roi.height *= zoom;

    return roi;
  }

  bool saveRoiImage(const double zoom = 1.0) {

    if (cur_ind > files_used.size()) {
      return false;
    }

    std::stringstream name;
    name << files_used[cur_ind].substr(0, files_used[cur_ind].size() - 4);

    bool matched = true;
    int i = 1000;
    while (matched) {
      std::stringstream nametest;
      nametest << name.str();
      nametest << "_" << i << ".jpg";
      if (!boost::filesystem::exists(nametest.str())) {
        matched = false;
        name.str(nametest.str());
      }
      i++;
    }

    /*if (
        (aspect_roi.x + aspect_roi.width < cur_roi_im.cols) &&
        (aspect_roi.x > 0) &&
        (aspect_roi.y + aspect_roi.height < cur_roi_im.rows) &&
        (aspect_roi.y > 0)
        ) {
    */

    LOG(INFO) << "wrote " << name.str();
    if ((roi_aspect != 1.0)) {
      cv::Rect roi2 = getRoiRect(zoom);
      cv::Rect combined_roi = roi2 & cur_roi; // rectangle intersection
      imwrite(name.str(), cur_im(combined_roi));

    } else {

      imwrite(name.str(), cur_roi_im);
    }

    // TBD put this image in the file/image array
    return true;
  }
};

/*

*/
int main(int argc, char *argv[]) {
  google::InitGoogleLogging(argv[0]);
  google::LogToStderr();
  google::ParseCommandLineFlags(&argc, &argv, false);

  boost::timer t1;
  Images *images =
      new Images(cv::Size(FLAGS_width, FLAGS_height), FLAGS_max_scale);
  // this is effectively 0 to do above

  // this take about 0.2 seconds, how fast is raw Xlib in vimjay for comparison?
  // cv::namedWindow("frames", CV_GUI_NORMAL | CV_WINDOW_AUTOSIZE);
  cv::namedWindow("frames");  //, CV_WINDOW_AUTOSIZE);
  double win_time = t1.elapsed();

  /// this probably is already done by the time the above window is created
  while (images->getNum() == 0) {
    usleep(30000);
  }

  LOG(INFO) << win_time << " for window";
  LOG(INFO) << t1.elapsed() << " to first frame";

  double zoom = 1.0;

  // where zoom center is
  // TBD should image class store this per image?
  // also panning around ought to be in pixel increments for big zooms
  cv::Point2f pos = cv::Point2f(0.5, 0.5);

  bool run = true; // rv && rv2;
  while (run) {

    cv::Mat im = images->getFrame(images->ind, zoom, pos);

    if (!im.empty()) {
      cv::imshow("frames", im);
    }

    char key = cv::waitKey(0);

    const float mv = 0.04;
    const float pos_min = 0.0; //-0.25;
    const float pos_max = 1.0 - pos_min;
    // there seems to be a delay when key switching, holding down
    // a key produces all the events I expect but changing from one to another
    // produces a noticeable pause.
    if (key == 'q') {
      run = false;
      images->continue_loading = false;
      delete images;
    } else if (key == 'j') {
      images->ind += 1;
    } else if (key == 'k') {
      images->ind -= 1;
    } else if (key == 'n') {
      images->ind = 0;
    } else if (key == 'h') {
      zoom *= 1.1;
      if (zoom > 32.0)
        zoom = 32.0;
    } else if (key == 'l') {
      zoom *= 0.95;
      if (zoom < 1.0 / 32.0)
        zoom = 1.0 / 32.0;
    }
    // scroll around
    else if (key == 's') {
      // pos.x *= (1.0 - 0.05/zoom);
      pos.x -= mv / zoom;
      if (pos.x < pos_min)
        pos.x = pos_min;
    } else if (key == 'd') {
      // pos.x *= (1.0 + 0.04/zoom);
      pos.x += mv / zoom;
      if (pos.x > pos_max)
        pos.x = pos_max;
    } else if (key == 'f') {
      // pos.y *= (1.0 + 0.05/zoom);
      pos.y += mv / zoom;
      if (pos.y > pos_max)
        pos.y = pos_max;
    } else if (key == 'a') {
      // pos.y *= (1.0 - 0.04/zoom);
      pos.y -= mv / zoom;
      if (pos.y < pos_min)
        pos.y = pos_min;
    } else if (key == 'g') {
      pos.x = 0.5;
      pos.y = 0.5;
      zoom = 1.0;
      images->roi_aspect = 1.0;
    } else if (key == 'e') {
      // increase roi horizontal aspect
      images->roi_aspect *= 1.05;
    } else if (key == 'w') {
      // increase roi horizontal aspect
      images->roi_aspect *= 0.97;
    } else if (key == 'p') {
      // TBD zoom belongs in Images
      images->saveRoiImage(zoom);
    }
  }

  return 0;
}
