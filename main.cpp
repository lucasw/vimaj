/*
  
  Copyright 2012 Lucas Walter

     This file is part of Vimjay.

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

#include <deque>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/timer.hpp>
#include <boost/thread.hpp>


#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include <glog/logging.h>
#include <gflags/gflags.h>

// bash color codes
#define CLNRM "\e[0m"
#define CLWRN "\e[0;43m"
#define CLERR "\e[1;41m"
#define CLVAL "\e[1;36m"
#define CLTXT "\e[1;35m"
// BOLD black text with blue background
#define CLTX2 "\e[1;44m"  

// TBD move to config file in $HOME/.vimaj/config.yml
DEFINE_int32(width, 1200, "width");
DEFINE_int32(height, 800, "height");
DEFINE_double(max_scale, 1.5, "maximum amount to scale the image");

//namespace bm

class Images
{

cv::Size sz;
float max_scale;
std::vector<cv::Mat> frames_scaled; 
// rendered, TBD do this live
std::vector<cv::Mat> frames; 
boost::thread im_thread;
boost::mutex im_mutex;
boost::mutex im_scaled_mutex;
std::string dir;
std::vector<std::string> files;
std::vector<std::string> files_used;

public:

int ind;

Images(
  cv::Size sz, 
  float max_scale
) :
  sz(sz),
  max_scale(max_scale),
  ind(0)
{
  im_thread = boost::thread(&Images::runThread, this);
  
} // Images

void runThread() 
{
  std::vector<cv::Mat> frames_orig; 
  boost::timer t1;
  const bool rv = getFileNames(".");

  const bool rv2 = loadAndResizeImages(frames_orig, frames, sz, max_scale);
  
  float t1_elapsed = t1.elapsed();

  LOG(INFO) << "loaded " << frames_orig.size() << " in time " << t1_elapsed 
      << " " << (float)t1_elapsed/(float)frames_scaled.size();
  
  // TBD test out freeing up this memory
  //frames_orig.clear();
}

// TBD is this any faster than warpImage?
bool renderImage(cv::Mat& src, cv::Mat& dst, int offx, int offy)
{
  if (offx > dst.cols ) return false;
  if (offy > dst.rows ) return false;
  if (-offx >= src.cols ) return false;
  if (-offy >= src.rows ) return false;

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
    src_wd = dst.cols - offx;
  }
  if (src.rows + offy - src_y > dst.rows) {
    src_ht = dst.rows - offy;
  }
  src_wd -= src_x;
  src_ht -= src_y;

  VLOG(1) << src_x << " " << src_y << " " << src_wd << " " << src_ht 
      << ", offxy " << offx << " " << offy
      << ", src " 
      << src.cols << " " << src.rows << ", dst "
      << dst.cols << " " << dst.rows; 
  cv::Mat src_clipped = src(cv::Rect(src_x, src_y, src_wd, src_ht));
  
  cv::Mat dst_roi = dst(cv::Rect(offx, offy, 
        src_clipped.cols, src_clipped.rows));
  src_clipped.copyTo(dst_roi);

  return true;
}

bool resizeImage(const cv::Mat& tmp0, cv::Mat& tmp_aspect, const cv::Size sz)
{
const float aspect_0 = (float)tmp0.cols/(float)tmp0.rows;
      const float aspect_1 = (float)sz.width/(float)sz.height;

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
        //if (tmp_sz.height > tmp0.rows * max_scale) { 
        //  tmp_sz.width = tmp0.cols * max_scale;
        //  tmp_sz.height = tmp0.rows * max_scale;
        //}

    //int mode = cv::INTER_NEAREST;
    //int mode = cv::INTER_CUBIC;
    int mode = cv::INTER_LINEAR;
    
    cv::resize( tmp0, tmp_aspect, tmp_sz, 0, 0, mode );
  return true;

}

bool renderMultiImage(const int i, cv::Mat& tmp1)
{
  int ind = i;
  cv::Mat tmp_aspect = getScaledFrame(ind);

  tmp1 = cv::Mat( sz, tmp_aspect.type(), cv::Scalar::all(0));
  
  if (tmp_aspect.empty()) {
    LOG(INFO) << "scaled frame " << ind << " is empty";
    return false;
  }

  // center the image
  int off_x = (sz.width - tmp_aspect.size().width)/2;
  int off_y = (sz.height - tmp_aspect.size().height)/2;

  // TBD flag?
  int border = 4;
  // TBD off_y should be a function of sz and the prev/next image size
  int ind2 = ind - 1;
  cv::Mat prev = getScaledFrame(ind2);
  
  if (ind2 != i) {
    renderImage(prev, tmp1, 
        off_x - prev.cols - border, off_y);

    ind2 = ind + 1;
    cv::Mat next = getScaledFrame(ind);
    renderImage(next, tmp1, 
        off_x + tmp_aspect.size().width + border, off_y);
  }
  renderImage(tmp_aspect, tmp1, off_x, off_y);

#if 0
  // TBD put offset so image is centered
  cv::Mat tmp1_roi = tmp1(cv::Rect(off_x, off_y, 
        tmp_aspect.cols, tmp_aspect.rows));
  tmp_aspect.copyTo(tmp1_roi);
#endif

  VLOG(3) //<< aspect_0 << " " << aspect_1 << ", " 
    << off_x << " " << off_y << " " << tmp_aspect.cols << " " << tmp_aspect.rows;

  cv::putText(tmp1, files_used[ind], cv::Point(10, 10), 1, 1, cv::Scalar::all(255) );

  return true;
}

bool loadAndResizeImages(
    std::vector<cv::Mat>& frames_orig, 
    std::vector<cv::Mat>& frames,
    const cv::Size sz,
    const double max_scale
    )
{

  frames.clear();
  frames_orig.clear();
  frames_scaled.clear();

  // TBD make optional
  sort(files.begin(), files.end());

  for (int i = 0; i < files.size(); i++) {
    const std::string next_im = files[i];

    // TBD only store the names in first pass, then load in second?
    cv::Mat new_out = cv::imread( next_im );

    if (new_out.data == NULL) { //.empty()) {
      LOG(WARNING) << " not an image? " << next_im;
      continue;
    }

    files_used.push_back(files[i]);
    
    VLOG(1) << " " << i << " loaded image " << next_im;

    frames_orig.push_back(new_out);

    cv::Mat frame_scaled;
    resizeImage(new_out, frame_scaled, sz);

    {
      boost::mutex::scoped_lock l(im_scaled_mutex);
      frames_scaled.push_back(frame_scaled);
    }
    
    cv::Mat multi_im;
    if (i < 1) {
      renderMultiImage(i, multi_im);
      frames.push_back(multi_im);
    } else 
    if (i > 1) {
      renderMultiImage(i - 1, multi_im);
      {
        boost::mutex::scoped_lock l(im_mutex);
        frames.push_back(multi_im);
      }
    } // 

  } // files loop

  cv::Mat multi_im;
  renderMultiImage(0, multi_im);
  {
    boost::mutex::scoped_lock l(im_mutex);
    frames[0] = multi_im;
  }
  
  // now fill unrendered frames
  renderMultiImage(1, multi_im);
  {
    boost::mutex::scoped_lock l(im_mutex);
    frames[1] = multi_im;
  }

  renderMultiImage(frames_scaled.size()-1, multi_im);
  {
    boost::mutex::scoped_lock l(im_mutex);
    frames.push_back(multi_im);
  }

} // loadAndResizeImages


bool getFileNames(std::string dir)
{
  this->dir = dir;
  std::string name = "vimaj";
    LOG(INFO) << name << " loading " << dir;
    
    boost::filesystem::path image_path(dir);
    if (!is_directory(image_path)) {
      LOG(ERROR) << name << CLERR << " not a directory " << CLNRM << dir; 
      return false;
    }

    // TBD clear frames first?
    
    boost::filesystem::directory_iterator end_itr; // default construction yields past-the-end
    for (boost::filesystem::directory_iterator itr( image_path );
        itr != end_itr;
        ++itr )
    {
      if ( is_directory( *itr ) ) continue;
      
      std::stringstream ss;
      ss << *itr;
      std::string next_im = ( ss.str() );
      // strip off "" at beginning/end
      next_im = next_im.substr(1, next_im.size()-2);

      if (!((next_im.substr(next_im.size()-3,3) != "jpg") || 
            (next_im.substr(next_im.size()-3,3) != "png"))
            ) {
        LOG(INFO) << "not expected image type: " << next_im;
        continue;
      }

      files.push_back(next_im);
   }
}


  /////////////////////////////////
  cv::Mat getScaledFrame(int& ind) 
  {
    boost::mutex::scoped_lock l(im_scaled_mutex);
    if (frames_scaled.size() == 0) return cv::Mat();
    ind = (ind + frames_scaled.size()) % frames_scaled.size();
    return frames_scaled[ind];
  }

  /* get a rendered multi frame 
  */
  cv::Mat getFrame(int& ind) 
  {
    boost::mutex::scoped_lock l(im_mutex);
    if (frames.size() == 0) return cv::Mat();
    ind = (ind + frames.size()) % frames.size();
    return frames[ind];
  }

  int getNum() 
  {
    boost::mutex::scoped_lock l(im_mutex);
    return frames.size();
  }

};

/*

 */
int main( int argc, char* argv[] )
{
  google::InitGoogleLogging(argv[0]);
  google::LogToStderr();
  google::ParseCommandLineFlags(&argc, &argv, false);


  Images* images = new Images(
      cv::Size(FLAGS_width, FLAGS_height),
      FLAGS_max_scale
      );

  while(images->getNum() == 0) {
    usleep(100000);
  }

  cv::namedWindow("frames", CV_GUI_NORMAL | CV_WINDOW_AUTOSIZE);

  
  bool run = true; // rv && rv2;
  while (run) {

    cv::Mat im = images->getFrame(images->ind);
    
    if (!im.empty()) {
      cv::imshow("frames", im);
    }

    char key = cv::waitKey(0);
    
    // there seems to be a delay when key switching, holding down
    // a key produces all the events I expect but changing from one to another
    // produces a noticeable pause.
    if (key == 'q') run = false;
    else if (key == 'j') {
      images->ind += 1;
    }
    else if (key == 'k') {
      images->ind -= 1;
    }
    else if (key == 'n') {
      images->ind = 0;
    }

  }
  
  return 0;
}

