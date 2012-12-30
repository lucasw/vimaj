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
DEFINE_int32(height, 800, "width");
DEFINE_double(max_scale, 1.5, "maximum amount to scale the image");

bool resizeImages(
  const std::vector<cv::Mat>& frames_orig, 
  std::vector<cv::Mat>& frames,
  const cv::Size sz,
  const double max_scale
  )
{
    std::vector<cv::Mat> frames_scaled; 
    frames.clear();
  
    //int mode = cv::INTER_NEAREST;
    //int mode = cv::INTER_CUBIC;
    int mode = cv::INTER_LINEAR;

    for (int i = 0; i < frames_orig.size(); i++) {
      cv::Mat tmp0 = frames_orig[i]; 
      
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

        cv::Mat tmp_aspect;
        cv::resize( tmp0, tmp_aspect, tmp_sz, 0, 0, mode );
        
        frames_scaled.push_back(tmp_aspect);
    }

    for (int i = 0; i < frames_orig.size(); i++) {
      cv::Mat tmp_aspect = frames_scaled[i]; 
      cv::Mat tmp1 = cv::Mat( sz, tmp_aspect.type(), cv::Scalar::all(0));

      int off_x = (sz.width - tmp_aspect.size().width)/2;
      int off_y = (sz.height - tmp_aspect.size().height)/2;

      // TBD put offset so image is centered
      cv::Mat tmp1_roi = tmp1(cv::Rect(off_x, off_y, 
        tmp_aspect.cols, tmp_aspect.rows));
      tmp_aspect.copyTo(tmp1_roi);

      VLOG(3) //<< aspect_0 << " " << aspect_1 << ", " 
        << off_x << " " << off_y << " " << tmp_aspect.cols << " " << tmp_aspect.rows;


      frames.push_back(tmp1);
    }

    return true;
  }


bool loadImages(std::string dir, std::vector<cv::Mat>& frames_orig) 
{
  std::string name = "vimaj";
    LOG(INFO) << name << " loading " << dir;
    
    boost::filesystem::path image_path(dir);
    if (!is_directory(image_path)) {
      LOG(ERROR) << name << CLERR << " not a directory " << CLNRM << dir; 
      return false;
    }

    // TBD clear frames first?
    
    std::vector<std::string> files;
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
      files.push_back(next_im);
   }

   frames_orig.clear();

   sort(files.begin(), files.end());
  
   for (int i=0; i < files.size(); i++) {
      const std::string next_im = files[i];

      if (!((next_im.substr(next_im.size()-3,3) != "jpg") || 
            (next_im.substr(next_im.size()-3,3) != "png"))
            ) {
        LOG(INFO) << "not an image " << next_im;
        continue;
      }

      // TBD only store the names in first pass, then load in second?
      cv::Mat new_out = cv::imread( next_im );
   
      if (new_out.data == NULL) { //.empty()) {
        LOG(WARNING) << name << " not an image? " << next_im;
        continue;
      }
   
     
      //cv::Mat tmp0 = cv::Mat(new_out.size(), CV_8UC4, cv::Scalar(0)); 
        // just calling reshape(4) doesn't do the channel reassignment like this does
      //  int ch[] = {0,0, 1,1, 2,2}; 
      //  mixChannels(&new_out, 1, &tmp0, 1, ch, 3 );

        
      VLOG(1) << name << " " << i << " loaded image " << next_im;

      frames_orig.push_back(new_out);
    }
    
    /// TBD or has sized increased since beginning of function?
    if (frames_orig.size() == 0) {
      LOG(ERROR) << name << CLERR << " no images loaded" << CLNRM << " " << dir;
      return false;
    }

    return true;
}
/*


 */
int main( int argc, char* argv[] )
{
  google::InitGoogleLogging(argv[0]);
  google::LogToStderr();
  google::ParseCommandLineFlags(&argc, &argv, false);

  std::vector<cv::Mat> frames_orig; 
  boost::timer t1;
  const bool rv = loadImages(".", frames_orig);
  LOG(INFO) << "load time " << t1.elapsed();
  std::vector<cv::Mat> frames; 
  boost::timer t2;
  const bool rv2 = resizeImages(frames_orig, frames, 
      cv::Size(FLAGS_width, FLAGS_height),
      FLAGS_max_scale
      );
  LOG(INFO) << "resize time " << t2.elapsed();
  
  cv::namedWindow("frames", CV_GUI_NORMAL | CV_WINDOW_AUTOSIZE);

  bool run = rv && rv2;
  int ind = 0;
  while (run) {

    char key = cv::waitKey(0);

    // there seems to be a delay when key switching, holding down
    // a key produces all the events I expect but changing from one to another
    // produces a noticeable pause.
    if (key == 'q') run = false;
    else if (key == 'j') {
      ind += 1;
      if (ind >= frames.size()) ind = 0;
    }
    else if (key == 'k') {
      ind -= 1;
      if (ind < 0) ind = frames.size() - 1;
    }

    cv::imshow("frames", frames[ind]);
  }
  
  return 0;
}

