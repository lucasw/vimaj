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
int ind;
std::string dir;
std::vector<std::string> files;

public:

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
  const bool rv2 = loadImages(frames_orig);
  float t1_elapsed = t1.elapsed();

  LOG(INFO) << "loaded " << frames_orig.size() << " in time " << t1_elapsed 
      << " " << (float)t1_elapsed/(float)frames_orig.size();
  
  
  const bool rv3 = resizeImages(
      frames_orig, frames, 
      sz, max_scale
      );

  // TBD test out freeing up this memory
  frames_orig.clear();

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
  cv::Mat tmp_aspect = frames_scaled[i];
  tmp1 = cv::Mat( sz, tmp_aspect.type(), cv::Scalar::all(0));

  // center the image
  int off_x = (sz.width - tmp_aspect.size().width)/2;
  int off_y = (sz.height - tmp_aspect.size().height)/2;

  int ind_prev = ((i-1)+frames_scaled.size()) % frames_scaled.size();
  int ind_next = ((i+1)+frames_scaled.size()) % frames_scaled.size();

  // TBD flag?
  int border = 4;
  // TBD off_y should be a function of sz and the prev/next image size
  renderImage(frames_scaled[ind_prev], tmp1, 
      off_x - frames_scaled[ind_prev].cols - border, off_y);
  renderImage(frames_scaled[ind_next], tmp1, 
      off_x + tmp_aspect.size().width + border, off_y);
  renderImage(tmp_aspect, tmp1, off_x, off_y);

#if 0
  // TBD put offset so image is centered
  cv::Mat tmp1_roi = tmp1(cv::Rect(off_x, off_y, 
        tmp_aspect.cols, tmp_aspect.rows));
  tmp_aspect.copyTo(tmp1_roi);
#endif

  VLOG(3) //<< aspect_0 << " " << aspect_1 << ", " 
    << off_x << " " << off_y << " " << tmp_aspect.cols << " " << tmp_aspect.rows;

  return true;
}

bool resizeImages(
    const std::vector<cv::Mat>& frames_orig, 
    std::vector<cv::Mat>& frames,
    const cv::Size sz,
    const double max_scale
    )
{
  boost::timer t2;

  {
    boost::mutex::scoped_lock l(im_mutex);
    frames.clear();
  }

    for (int i = 0; i < frames_orig.size(); i++) {
      cv::Mat tmp0 = frames_orig[i]; 
      
      cv::Mat tmp_aspect;
      resizeImage(tmp0, tmp_aspect, sz);

      frames_scaled.push_back(tmp_aspect);
    }

    float t2_elapsed = t2.elapsed();
    LOG(INFO) << "scale time " << t2_elapsed
      << " " << (float)t2_elapsed/(float)frames_scaled.size();

    boost::timer t1;
    for (int i = 0; i < frames_orig.size(); i++) {
      cv::Mat tmp1;
      renderMultiImage(i, tmp1);

      boost::mutex::scoped_lock l(im_mutex);
      frames.push_back(tmp1);
    }

    float t1_elapsed = t1.elapsed();
    LOG(INFO) << "render time " << t1_elapsed
      << " " << (float)t1_elapsed/(float)frames_scaled.size();

    return true;
  }


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

bool loadImages(std::vector<cv::Mat>& frames_orig) 
{
   frames_orig.clear();

   //sort(files.begin(), files.end());
  
   for (int i=0; i < files.size(); i++) {
      const std::string next_im = files[i];

      // TBD only store the names in first pass, then load in second?
      cv::Mat new_out = cv::imread( next_im );
   
      if (new_out.data == NULL) { //.empty()) {
        LOG(WARNING) << " not an image? " << next_im;
        continue;
      }
   
     
      //cv::Mat tmp0 = cv::Mat(new_out.size(), CV_8UC4, cv::Scalar(0)); 
        // just calling reshape(4) doesn't do the channel reassignment like this does
      //  int ch[] = {0,0, 1,1, 2,2}; 
      //  mixChannels(&new_out, 1, &tmp0, 1, ch, 3 );

        
      VLOG(1) << " " << i << " loaded image " << next_im;

      frames_orig.push_back(new_out);
    }
    
    /// TBD or has sized increased since beginning of function?
    if (frames_orig.size() == 0) {
      LOG(ERROR) << CLERR << " no images loaded" << CLNRM << " " << dir;
      return false;
    }

    return true;
}

  cv::Mat getCurFrame() 
  {
    
    boost::mutex::scoped_lock l(im_mutex);
    if (frames.size() == 0) return cv::Mat();
    return frames[ind];
  }

  cv::Mat getNextFrame() 
  {
    ind += 1;
    boost::mutex::scoped_lock l(im_mutex);
    if (frames.size() == 0) return cv::Mat();
    if (ind >= frames.size()) ind = 0;
    return frames[ind];
  }

  cv::Mat getPrevFrame() 
  {
    ind -= 1;
    boost::mutex::scoped_lock l(im_mutex);
    if (frames.size() == 0) return cv::Mat();
    if (ind < 0) ind = frames.size() - 1;
    return frames[ind];
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

  cv::namedWindow("frames", CV_GUI_NORMAL | CV_WINDOW_AUTOSIZE);

  cv::Mat im = images->getCurFrame();
  
  bool run = true; // rv && rv2;
  while (run) {

    char key = cv::waitKey(0);
    
    // there seems to be a delay when key switching, holding down
    // a key produces all the events I expect but changing from one to another
    // produces a noticeable pause.
    if (key == 'q') run = false;
    else if (key == 'j') {
      im = images->getNextFrame();     
    }
    else if (key == 'k') {
      im = images->getPrevFrame();
    }

    if (!im.empty()) {
      cv::imshow("frames", im);
    }
  }
  
  return 0;
}

