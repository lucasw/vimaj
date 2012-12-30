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
#include <boost/timer.hpp>

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include <glog/logging.h>
#include <gflags/gflags.h>

/*


 */
int main( int argc, char* argv[] )
{
  google::InitGoogleLogging(argv[0]);
  google::LogToStderr();
  google::ParseCommandLineFlags(&argc, &argv, false);
 
  
  return 0;
}

