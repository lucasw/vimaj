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
  const bool rv = loadImages(".", frames_orig);

  bool run = rv;
  int ind = 0;
  while (run) {

    char key = cv::waitKey(0);

    if (key == 'q') run = false;
    else if (key == 'j') {
      ind += 1;
      if (ind >= frames_orig.size()) ind = 0;
    }
    else if (key == 'k') {
      ind -= 1;
      if (ind < 0) ind = frames_orig.size() - 1;
    }

    cv::imshow("frames", frames_orig[ind]);
  }
  
  return 0;
}

